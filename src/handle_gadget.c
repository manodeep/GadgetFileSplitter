#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <inttypes.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include "utils.h"
#include "handle_gadget.h"
#include "split_gadget.h"
#include "filesplitter.h"
#include "progressbar.h"
#include "mpi_wrapper.h"

#define MAXLEN (1024)

int check_if_file_exists(const char *outfile);
int write_hdr_to_stream(FILE *fp, struct io_header *header);
int write_front_and_end_padding_bytes_to_stream(uint32_t dummy, FILE *fp);
off_t get_offset_from_npart(const int32_t start_npart, const int32_t nparttotal, field_type field, const size_t id_bytes);
int copy_all_dmfields_from_gadget_snapshot(file_copy_union *inp, file_copy_union *out, const int32_t start, const int32_t end, 
										   const int32_t inp_nparttotall,  const int32_t nwritten, const int32_t out_nparttotal,
										   const size_t id_bytes, const file_copy_options copy_kind);


ssize_t find_id_bytes(const char *filebase)
{
    char filename[MAXLEN];
    my_snprintf(filename, MAXLEN, "%s.0", filebase);
    FILE *fp = fopen(filename,"r");
    if(fp == NULL) {
        fprintf(stderr,"Error: Could not open file `%s' while trying to read the number of bytes in particle IDs\n",
            filename);
        return -1;
    }
    struct io_header hdr;
    uint32_t dummy;
    size_t nread = my_fread(&dummy, sizeof(dummy), 1, fp);
    if(nread != 1 || dummy != 256) {
        fprintf(stderr,"Read error: Either failed to read (front) padding bytes for header in file `%s' or the read value (=%d) was not 256\n",
                filename, dummy);
        return -1;
    }
    nread = my_fread(&hdr, sizeof(hdr), 1, fp);
    if(nread != 1) {
        return -1;
    }
    nread = my_fread(&dummy, sizeof(dummy), 1, fp);
    
    if(nread != 1 || dummy != 256) {
        fprintf(stderr,"Read error: Either failed to read (end) padding bytes for header in file `%s' or the read value (=%d) was not 256\n",
                filename, dummy);
        return -1;
    }
    for(int type=0;type<6;type++) {
        if(type == 1) continue;
        
        if(hdr.npart[type] > 0) {
            fprintf(stderr,"Error: This code package only works with files that contain *only* dark matter files. I have type = %d with npart = %d\n",
                    type, hdr.npart[type]);
            return -1;
        }
    }
    uint32_t npart = hdr.npart[1];

    /* We have already read in the header. So offset is relative to currrent */
    off_t offset = 2*sizeof(int) + sizeof(float)*npart*3 +
        2*sizeof(int) + sizeof(float)*npart*3;

    int status = fseeko(fp,offset,SEEK_CUR);
    if(status < 0) {
        fprintf(stderr,"Error: Could not seek %lld bytes into file `%s' while trying to figure out the number of bytes in each particle ID\n",
                (long long) offset, filename);
        return -1;
    }
    nread = my_fread(&dummy, sizeof(dummy), 1, fp);
    if(nread != 1) {
        return -1;
    }
    
    ssize_t id_bytes = -1;//default to indicate failure
    if(dummy == npart * 4) {
        return 4;
    } else if(dummy == npart * 8) {
        return 8;
    } 

    if(id_bytes < 0) {
        fprintf(stderr,"Error: Number of bytes in particle ID should be 4 or 8. Total number of bytes corresponding to %u particles in file `%s' is %u\n"
                "npart * 4 = %zu npart * 8 = %zu does not equal the total number of bytes. Is the input file corrupted ?\n",
                npart, filename, dummy, npart * sizeof(int32_t),  npart * sizeof(int64_t));
    } 
    
    fclose(fp);
    return id_bytes;
}

off_t get_offset_from_npart(const int32_t start_npart, const int32_t nparttotal, field_type field, const size_t id_bytes)
{
  

  off_t pos_start_offset = sizeof(int) + sizeof(struct io_header) + sizeof(int) + sizeof(int);
  off_t vel_start_offset = pos_start_offset + 3*nparttotal*sizeof(float) + sizeof(int) + sizeof(int);
  off_t  id_start_offset = vel_start_offset + 3*nparttotal*sizeof(float) + sizeof(int) + sizeof(int);
  off_t offset;

    switch(field) {
    case(POS):offset = pos_start_offset;break;
    case(VEL):offset = vel_start_offset;break;
    case(ID):offset = id_start_offset;break;
    default:
        fprintf(stderr,"Error: Unknown field type = %u (known fields are POS = %u VEL = %u ID = %u)\n",
                field, POS, VEL, ID);
        offset=-1;
    }
	if(offset > 0 && start_npart > 0) {
	  const size_t bytes_per_field = field == ID ? id_bytes:3*sizeof(float);
  	  offset += bytes_per_field*start_npart;
	}


    return offset;
}

int copy_all_dmfields_from_gadget_snapshot(file_copy_union *inp, file_copy_union *out, const int32_t start, const int32_t end, 
										   const int32_t inp_nparttotall,  const int32_t nwritten, const int32_t out_nparttotal,
										   const size_t id_bytes, const file_copy_options copy_kind)
{
    const int32_t npart = end - start;//end is not inclusive -> that's why npart = end - start rather than npart = end - start + 1
    const long sz = sysconf(_SC_PAGESIZE);
    const size_t bufsize =  sz > 0 ? (size_t) sz:4096;
    void *buf = malloc(bufsize);
    if(buf == NULL) {
        return EXIT_FAILURE;
    }
    
    /* Copy the pos/vel/id */
    for(field_type field = POS; field < NUM_FIELDS; field++) {
	  const off_t output_offset = get_offset_from_npart(nwritten, out_nparttotal, field, id_bytes);
	  const off_t input_offset = get_offset_from_npart(start, inp_nparttotall, field, id_bytes);
        if(input_offset < 0 || output_offset < 0) {
            return EXIT_FAILURE;
        }
        const size_t bytes_per_field = field == ID ? id_bytes:3*sizeof(float);
        const size_t bytes = npart * bytes_per_field;
		fprintf(stderr,"Copying %zu bytes for field = %d (POS=%d VEL=%d ID=%d)...input_offset = %jd output_offset = %jd...\n",
				bytes, field, POS, VEL, ID, input_offset, output_offset);
        int status = filesplitter(inp, out, input_offset, output_offset, bytes, buf, bufsize, copy_kind);
        if(status != EXIT_SUCCESS) {
            free(buf);
            return status;
        }
		fprintf(stderr,"Copying %zu bytes for field = %d (POS=%d VEL=%d ID=%d)...input_offset = %jd output_offset = %jd......done\n",
				bytes, field, POS, VEL, ID, input_offset, output_offset);

    }
    free(buf);
    return EXIT_SUCCESS;
}    

int gadget_snapshot_create(const char *filebase, const char *outfilename, struct file_mapping *fmap, const size_t id_bytes, 
						   const int noutfiles, const file_copy_options copy_kind)
{
  int status = check_if_file_exists(outfilename);
  if(status != EXIT_SUCCESS) {
	return status;
  }

  long expected_file_size = 2*sizeof(int) + sizeof(struct io_header) +
	2*sizeof(int) + fmap->numpart*3*sizeof(float)+
	2*sizeof(int) + fmap->numpart*3*sizeof(float)+
	2*sizeof(int) + fmap->numpart*id_bytes;
  
  {
	char execstring[MAXLEN];
	int nwritten = my_snprintf(execstring, MAXLEN, "truncate -s %ld %s\n", expected_file_size, outfilename);
	if(nwritten >= MAXLEN) {
	  fprintf(stderr,"Could not create string for system command. Please increase MAXLEN in %s\n",__FILE__);
	  perror(NULL);
	  return EXIT_FAILURE;
	}
	status = system(execstring);
	if(status != EXIT_SUCCESS) {
	  fprintf(stderr,"Error: Could not reserve disk space = %ld bytes for file = `%s'\n", expected_file_size, outfilename);
	  perror(NULL);
	  return EXIT_FAILURE;
	}
  }

    struct io_header outhdr;
    /* New scope to hide the "filename" variable */
    {
        char filename[MAXLEN];
        my_snprintf(filename, MAXLEN, "%s.0", filebase);
        status = read_header(filename, &outhdr);
        if(status != EXIT_SUCCESS) {
            return status;
        }
    }
    outhdr.npart[1] = fmap->numpart;
	outhdr.num_files = noutfiles;

    FILE *fp = fopen(outfilename, "r+");
    if(fp == NULL) {
        fprintf(stderr,"Error: Could not open output file = `%s' (even though supposedly disk space reserving worked!) \n",outfilename);
        perror(NULL);
        return EXIT_FAILURE;
    }

    status = write_hdr_to_stream(fp, &outhdr);
    if(status != EXIT_SUCCESS) {
        fclose(fp);
        return status;
    }
    /* Now write out the padding bytes for each field (pos/vel/id)*/
    for(field_type field = POS;field<NUM_FIELDS;field++) {
	  const off_t offset = get_offset_from_npart(0, outhdr.npart[1], field, id_bytes) - sizeof(int);//offset for the beginning of padding byte 
        status = fseeko(fp, offset, SEEK_SET);
        if(status < 0) {
            fprintf(stderr,"Could not seek to offset = %ld while writing (front) padding bytes for field = %u (POS = %u, VEL = %u, ID=%u)\n",
                    (long) offset, field, POS, VEL, ID);
            return EXIT_FAILURE;
        }
        const size_t bytes_per_field = field == ID ? id_bytes:3*sizeof(float);
        const size_t dummy = outhdr.npart[1] * bytes_per_field;
        status = write_front_and_end_padding_bytes_to_stream(dummy, fp);
        if(status != EXIT_SUCCESS) {
            return status;
        }
    }
    fclose(fp);//close output file

	file_copy_union output_filehandler;
	status = initialize_file_copy(&output_filehandler, copy_kind, outfilename, WRITE_ONLY_MODE);
	if(status != EXIT_SUCCESS) {
	  return status;
	}

	outhdr.npart[1] = 0;

	fprintf(stderr,"\n");
	int32_t nwritten = 0;
    for(int64_t i=0;i<fmap->numfiles;i++) {
        char filename[MAXLEN];
        my_snprintf(filename, MAXLEN, "%s.%d", filebase, fmap->input_file_id[i]);
        struct io_header hdr;
        status = read_header(filename, &hdr);
        if(status != EXIT_SUCCESS) {
		  finalize_file_copy(&output_filehandler, copy_kind);
		  return status;
        }
		if(nwritten != fmap->output_file_nwritten[i]) {
		  fprintf(stderr,"Error: Mismatch between the number of particles written (=%d) out so far and what is claimed by the particle mapping =(%d)\n",
				  nwritten, fmap->output_file_nwritten[i]);
		  finalize_file_copy(&output_filehandler, copy_kind);
		  return EXIT_FAILURE;
		}

		fprintf(stderr,"Moving [%d -- %d) particles from input file = `%s' to outputfile = `%s' (nwritten so far = %d)\n",
				fmap->input_file_start_particle[i],
				fmap->input_file_end_particle[i],
				filename,
				outfilename,
				fmap->output_file_nwritten[i]);

		file_copy_union input_filehandler;
        status = initialize_file_copy(&input_filehandler, copy_kind, filename, READ_ONLY_MODE);

        /* Number of particles written to this output file so far (assuming serial access) */
		fprintf(stderr,"Calling copy_all_dmfields_from_gadget_snapshot....\n");
        status = copy_all_dmfields_from_gadget_snapshot(&input_filehandler, &output_filehandler,
                                                        fmap->input_file_start_particle[i],
                                                        fmap->input_file_end_particle[i],
														hdr.npart[1],
                                                        fmap->output_file_nwritten[i],
														fmap->numpart,
                                                        id_bytes,
														copy_kind);

        if(status != EXIT_SUCCESS) {
		  
		  return status;
        }
		nwritten += (fmap->input_file_end_particle[i] - fmap->input_file_start_particle[i]);
		fprintf(stderr,"Calling copy_all_dmfields_from_gadget_snapshot....done. (nwritten = %d)\n\n",nwritten);

		status = finalize_file_copy(&input_filehandler, copy_kind);
		if(status != EXIT_SUCCESS) {
		  return status;
		}
        outhdr.npart[1] += (fmap->input_file_end_particle[i] - fmap->input_file_start_particle[i]);
    }

	status = finalize_file_copy(&output_filehandler, copy_kind);
	if(status != EXIT_SUCCESS) {
	  return status;
	}

	if(outhdr.npart[1] != fmap->numpart){
	  fprintf(stderr,"Error: Number of particles output (=%d) do not equal the number of particles in file mapping (=%d)\n",
			  outhdr.npart[1], fmap->numpart);
	  return EXIT_FAILURE;
	}

    fp = fopen(outfilename, "r");
    if(fp == NULL) {
        fprintf(stderr,"Error: Very strange behaviour - just closed this file = `%s' but can not seem to re-open it\n",
                outfilename);
        perror(NULL);
		return EXIT_FAILURE;
    }

	status = fseek(fp, 0, SEEK_END);
	if(status < 0) {
	  fprintf(stderr,"Error: Could not seek to the end of the file `%s'\n", outfilename);
	  perror(NULL);
	  return status;
	}
    long filesize = ftell(fp);
	if(filesize < 0) {
	  fprintf(stderr,"Error: Could not get file pointer offset for file `%s' (trying to get total filesize)\n", outfilename);
	  perror(NULL);
	  return filesize;
	}
    fclose(fp);

    if(filesize != expected_file_size) {
	  fprintf(stderr,"Error: Expected file size is %ld but after finishing writing the entire file contains %ld bytes\n",
			  expected_file_size,filesize);
	  fprintf(stderr,"Output file = `%s' number of DM particles = %d (on ThisTask = %d)\n",
                outfilename, outhdr.npart[1], ThisTask);
        return EXIT_FAILURE;
    } 
	fprintf(stderr, ANSI_COLOR_GREEN "Finished writing file `%s' (bytes = %ld, numpart = %d) on task=%d"ANSI_COLOR_RESET"\n", 
			outfilename, filesize, outhdr.npart[1], ThisTask);

    return EXIT_SUCCESS;
}

int write_front_and_end_padding_bytes_to_stream(uint32_t dummy, FILE *fp)
{
    size_t nwritten = my_fwrite(&dummy, sizeof(dummy), 1, fp);
    if(nwritten != 1) {
        fprintf(stderr,"Error: While writing (front) padding bytes for field with %u bytes. Managed to only write %zu bytes instead of 4\n",
                dummy, nwritten);
        return EXIT_FAILURE;
    }
    int status = fseeko(fp, dummy,SEEK_CUR);
    if(status < 0) {
        fprintf(stderr,"Error: While seeking file pointer to disk location. Offset was to be %u bytes\n", dummy);
        perror(NULL);
        return EXIT_FAILURE;
    }
    nwritten = my_fwrite(&dummy, sizeof(dummy), 1, fp);
    if(nwritten != 1) {
        fprintf(stderr,"Error: While writing (end) padding bytes for field with %u bytes. Managed to only write %zu bytes instead of 4\n",
                dummy, nwritten);
        return EXIT_FAILURE;
    }    

    return EXIT_SUCCESS;
}


int write_hdr_to_stream(FILE *fp, struct io_header *header)
{
    if(header == NULL || fp == NULL) {
        return EXIT_FAILURE;
    }

    uint32_t dummy = 256;
    size_t nwritten = my_fwrite(&dummy, sizeof(dummy), 1, fp);
    if(nwritten != 1) {
        fprintf(stderr,"Error: Could not write (front) padding bytes for the header\n");
        perror(NULL);
        return EXIT_FAILURE;
    }

    nwritten = my_fwrite(header, sizeof(*header), 1, fp);
    if(nwritten != 1) {
        fprintf(stderr,"Error: Could not write Gadget header\n");
        perror(NULL);
        return EXIT_FAILURE;
    }
    
    nwritten = my_fwrite(&dummy, sizeof(dummy), 1, fp);
    if(nwritten != 1) {
        fprintf(stderr,"Error: Could not write (end) padding bytes for the header\n");
        perror(NULL);
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}



int read_header(const char *filename, struct io_header *header)
{
    FILE *fp = my_fopen(filename, "r");
    if(fp == NULL) {
        perror(NULL);
        return EXIT_FAILURE;
    }
    uint32_t dummy;
    size_t status = my_fread(&dummy, sizeof(dummy), 1, fp);
    if(status != 1 || dummy != 256) {
        fprintf(stderr,"Expected to read %zu bytes containing the size of "
                "the header := 256. Instead read returned %zu bytes containing %u\n",
                sizeof(dummy), status, dummy);
        return EXIT_FAILURE;
    }
    status = my_fread(header, sizeof(struct io_header), 1, fp);
    if(status != 1) {
        return EXIT_FAILURE;
    }
    
    status = my_fread(&dummy, sizeof(dummy), 1, fp);
    if(status != 1 || dummy != 256) {
        return EXIT_FAILURE;
    }
    

    fclose(fp);
    return EXIT_SUCCESS;
}

int32_t find_numfiles(const char *filebase)
{
    char filename[MAXLEN];
    my_snprintf(filename,MAXLEN,"%s.0",filebase);
    struct io_header hdr;
    int status = read_header(filename, &hdr);
    if(status != EXIT_SUCCESS) {
        return -1;
    }
    return hdr.num_files;
}

int64_t count_total_number_of_particles(const char *filebase, int32_t nfiles, int32_t *numpart_in_input_file)
{
  int64_t numpart = 0, totnumpart = 0;
    
    if(nfiles <= 0) {
        fprintf(stderr,"Error: Number of files that the snapshot file is spread over must be specified. Got nfiles = %d\n",
                nfiles);
        return EXIT_FAILURE;
    }
	int interrupted=0, numdone=0,totnumdone=0;
    if(ThisTask == 0) {
	  init_my_progressbar(nfiles, &interrupted);
	}
    for(int32_t ifile=0;ifile<nfiles;ifile++) {
	  if(ThisTask == 0) {
		my_progressbar(totnumdone, &interrupted);
	  }

	  /* fprintf(stderr,"\nThisTask = %5d working on ifile = %5d\n", ThisTask, ifile); */
	  char filename[MAXLEN];
	  my_snprintf(filename,MAXLEN,"%s.%d",filebase,ifile);
	  struct io_header hdr;
	  int status = read_header(filename, &hdr);
	  if(status != EXIT_SUCCESS) {
		return -1;
	  }
	  if(hdr.num_files != nfiles) {
		  fprintf(stderr,"Error: Header in file `%s' claims file is spread over %d files "
				  " However, the code was requested to only read in %d files. Probably an error\n",
				  filename, hdr.num_files, nfiles);
		  return -1;
	  }
	  
	  /* Check that this is a dark matter only */
	  for(int type=0;type<6;type++) {
		if(type == 1) continue;
		
		if(hdr.npart[type] > 0) {
		  fprintf(stderr,"Error: This code is only meant to convert dark matter only sims.\n"
				  "Thus, only npart[1] should have non-zero entry but I find npart[%d] = %d in file = `%s'\n",
				  type, hdr.npart[type], filename);
		  return EXIT_FAILURE;
		}
	  }
	  
	  /* So we have a dark-matter only simulation. Check that dark matter
		 mass array is not 0.0 (i,e. particles don't have individual masses)*/
	  if(hdr.mass[1] <= 0.0) {
		fprintf(stderr,"Error: This code can not handle dark matter particles with individual masses. However, in file `%s' "
				"dark matter mass is = %lf \n", filename, hdr.mass[1]);
		return EXIT_FAILURE;
	  }
	  numpart += hdr.npart[1];
	  numpart_in_input_file[ifile] = hdr.npart[1];
	  numdone++;
	  totnumdone++;

#if 0
#ifdef USE_MPI
	  MPI_Allreduce(&numdone, &totnumdone, 1, MPI_INT, MPI_SUM, MPI_COMM_WORLD);
#else
	  totnumdone = numdone;
#endif
	  fprintf(stderr,"ThisTask = %5d working on ifile = %5d...done. numdone = %5d\n", ThisTask, ifile, numdone);
	  interrupted=1;
#endif//if 0
    }//for loop over nfiles


#if 0
#ifdef USE_MPI
	fprintf(stderr,"ThisTask = %5d (numdone = %5d) - reached MPI Barrier\n",ThisTask, numdone);
	MPI_Barrier(MPI_COMM_WORLD);
	for(int ifile=0;ifile<nfiles;ifile++) {
	  fprintf(stderr,"Broadcasting ifile = %d...",ifile);
	  int ierr = MPI_Bcast(&numpart_in_input_file[ifile], 1, MPI_INT, ifile, MPI_COMM_WORLD);
	  if (ierr != MPI_SUCCESS) {
		return ierr;
	  }
	  fprintf(stderr,"Broadcasting ifile = %d......done\n",ifile);
	}
	MPI_Allreduce(&numpart, &totnumpart, 1, MPI_INT64_T, MPI_SUM, MPI_COMM_WORLD);
#else
	totnumpart = numpart;
#endif
#endif//if 0

	totnumpart = numpart;
	if(ThisTask == 0) {
	  finish_myprogressbar(&interrupted);
	}

    return totnumpart;
}

int check_if_file_exists(const char *outfile)
{
  /* Check that I am not accidentally over-writing some files*/
  FILE *fp = fopen(outfile, "r");
  if(fp != NULL) {
	fprintf(stderr,"Error: Expected output file = `%s' to not exist. Aborting in case there is some error (and avoiding )"
			"valuable snapshots getting accidentally overwritten)\n",outfile);
	fclose(fp);
	return EXIT_FAILURE;
  }
  return EXIT_SUCCESS;
}

