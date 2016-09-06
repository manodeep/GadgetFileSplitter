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

#define MAXLEN (1024)

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
    if(nread != sizeof(dummy) || dummy != 256) {
        fprintf(stderr,"Read error: Either failed to read (front) padding bytes for header in file `%s' or the read value (=%d) was not 256\n",
                filename, dummy);
        return -1;
    }
    nread = my_fread(&hdr, sizeof(hdr), 1, fp);
    if(nread != sizeof(hdr)) {
        return -1;
    }
    nread = my_fread(&dummy, sizeof(dummy), 1, fp);
    
    if(nread != sizeof(dummy) || dummy != 256) {
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
        fprintf(stderr,"Error: Could not seek %ld bytes into file `%s' while trying to figure out the number of bytes in each particle ID\n",
                offset, filename);
        return -1;
    }
    nread = my_fread(&dummy, sizeof(dummy), 1, fp);
    if(nread != sizeof(dummy)) {
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

off_t get_offset_from_npart(const int32_t npart, field_type field)
{
    off_t pos_offset = sizeof(struct io_header) + 2*sizeof(int);
    off_t vel_offset = pos_offset + 3*npart*sizeof(float) + 2*sizeof(int);
    off_t id_offset = vel_offset + 3*npart*sizeof(float) + 2*sizeof(int);

    off_t offset;
    switch(field) {
    case(POS):offset = pos_offset;break;
    case(VEL):offset = vel_offset;break;
    case(ID):offset = id_offset;break;
    default:
        fprintf(stderr,"Error: Unknown field type = %u (known fields are POS = %u VEL = %u ID = %u)\n",
                field, POS, VEL, ID);
        offset=-1;
    }

    return offset;
}

int copy_all_dmfields_from_gadget_snapshot(int in_fd, int out_fd, const int32_t start, const int32_t end, const int32_t nwritten, const size_t id_bytes)
{
    const int32_t npart = end - start;//end is not inclusive -> that's why npart = end - start rather than npart = end - start + 1
    const long sz = sysconf(_SC_PAGESIZE);
    const size_t bufsize =  sz > 0 ? (size_t) 4*sz:4*4096;
    void *buf = malloc(bufsize);
    if(buf == NULL) {
        return EXIT_FAILURE;
    }
    
    /* Copy the pos/vel/id */
    for(field_type field = POS; field < NUM_FIELDS; field++) {
        const off_t output_offset = get_offset_from_npart(nwritten, field);
        const off_t input_offset = get_offset_from_npart(start, field);
        if(input_offset < 0 || output_offset < 0) {
            return EXIT_FAILURE;
        }
        const size_t bytes_per_field = field == ID ? id_bytes:3*sizeof(float);
        const size_t bytes = npart * bytes_per_field;
        int status = filesplitter(in_fd, out_fd, input_offset, output_offset, bytes, buf, bufsize);
        if(status != EXIT_SUCCESS) {
            free(buf);
            return status;
        }
    }
    free(buf);
    return EXIT_SUCCESS;
}    

int gadget_snapshot_create(const char *filebase, const char *outfilename, struct file_mapping *fmap, const size_t id_bytes)
{
    int status = generate_file_skeleton(outfilename, fmap->numpart, id_bytes);
    if(status != EXIT_SUCCESS) {
        return status;
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
    outhdr.npart[1] = 0;

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
    fclose(fp);//closed output file

    int out_fd = open(outfilename, O_WRONLY);
    for(int64_t i=0;i<fmap->numfiles;i++) {
        char filename[MAXLEN];
        my_snprintf(filename, MAXLEN, "%s.%d", filebase, fmap->input_file_id[i]);
        struct io_header hdr;
        status = read_header(filename, &hdr);
        if(status != EXIT_SUCCESS) {
            close(out_fd);
            return status;
        }

        int in_fd = open(filename, O_RDONLY);

        /* Number of particles written to this output file so far (assuming serial access) */
        status = copy_all_dmfields_from_gadget_snapshot(in_fd, out_fd,
                                                        fmap->input_file_start_particle[i],
                                                        fmap->input_file_end_particle[i],
                                                        fmap->output_file_nwritten[i],
                                                        id_bytes);
        if(status != EXIT_SUCCESS) {
            close(in_fd);close(out_fd);
            return status;
        }
        
        
        status = close(in_fd);
        if(status < 0) {
            fprintf(stderr,"Error while closing output file = `%s' opened in read-only mode. This is strange\n"
                    "Check the system error message being printed out next\n", filename);
            perror(NULL);
            return status;
        }
        
        
        outhdr.npart[1] += (fmap->input_file_end_particle[i] - fmap->input_file_start_particle[i]);
    }

    status = close(out_fd);
    if(status < 0) {
        fprintf(stderr,"Error while closing output file = `%s'. This really should not happen since disk space was "
                "already reserved for the file.\nCheck the system error message being printed out next\n",  outfilename);
        perror(NULL);
        return status;
    }

    fp = fopen(outfilename, "r+");
    if(fp == NULL) {
        fprintf(stderr,"Error: Very strange behaviour - just closed this file = `%s' but can not seem to re-open it\n",
                outfilename);
        perror(NULL);
    }
    status = write_hdr_to_stream(fp, &outhdr);
    if(status != EXIT_SUCCESS) {
        fclose(fp);
        return status;
    }

    /* Now write out the padding bytes for each field (pos/vel/id)*/
    for(field_type field = POS;field<NUM_FIELDS;field++) {
        const off_t offset = get_offset_from_npart(outhdr.npart[1], field);
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
    


    fclose(fp);
    return EXIT_SUCCESS;
}

int write_front_and_end_padding_bytes_to_stream(uint32_t dummy, FILE *fp)
{
    size_t nwritten = my_fwrite(&dummy, sizeof(dummy), 1, fp);
    if(nwritten != sizeof(dummy)) {
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
    if(nwritten != sizeof(dummy)) {
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
    if(nwritten != sizeof(dummy)) {
        fprintf(stderr,"Error: Could not write (front) padding bytes for the header\n");
        perror(NULL);
        return EXIT_FAILURE;
    }

    nwritten = my_fwrite(header, sizeof(*header), 1, fp);
    if(nwritten != sizeof(*header)) {
        fprintf(stderr,"Error: Could not write Gadget header\n");
        perror(NULL);
        return EXIT_FAILURE;
    }
    
    nwritten = my_fwrite(&dummy, sizeof(dummy), 1, fp);
    if(nwritten != sizeof(dummy)) {
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
    if(status != sizeof(dummy) || dummy != 256) {
        fprintf(stderr,"Expected to read %zu bytes containing the size of "
                "the header := 256. Instead read returned %zu bytes containing %u\n",
                sizeof(dummy), status, dummy);
        return EXIT_FAILURE;
    }
    status = my_fread(header, sizeof(struct io_header), 1, fp);
    if(status != sizeof(*header)){
        return EXIT_FAILURE;
    }
    
    status = my_fread(&dummy, sizeof(dummy), 1, fp);
    if(status != sizeof(dummy) || dummy != 256) {
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
    int64_t totnumpart = 0;
    
    if(nfiles <= 0) {
        fprintf(stderr,"Error: Number of files that the snapshot file is spread over must be specified. Got nfiles = %d\n",
                nfiles);
        return EXIT_FAILURE;
    }
    
    for(int32_t ifile=0;ifile<nfiles;ifile++) {
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
        totnumpart += hdr.npart[1];
        numpart_in_input_file[ifile] = hdr.npart[1];
    }

    return totnumpart;
}



int generate_file_skeleton(const char *outfile, const size_t numpart, const size_t id_bytes)
{
    {
        /* Check that I am not accidentally over-writing some files*/
        FILE *fp = fopen(outfile, "r");
        if(fp != NULL) {
            fprintf(stderr,"Error: Expected output file = `%s' to not exist. Aborting in case there is some error (and avoiding )"
                    "valuable snapshots getting accidentally overwritten)\n",outfile);
            fclose(fp);
            return EXIT_FAILURE;
        }
    }


    /* Reserve disk space for a Gadget file */
    int fd = open(outfile, O_CREAT | O_TRUNC | O_WRONLY);
    if(fd < 0) {
        fprintf(stderr,"Error: During reserving space for file = `%s'\n",outfile);
        perror(NULL);
        return EXIT_FAILURE;
    }

    off_t totbytes = 2*(numpart * sizeof(float) * 3 + 2*sizeof(int)) +
        numpart * id_bytes + 2*sizeof(int) + sizeof(struct io_header) + 2*sizeof(int);
    int status = posix_fallocate(fd, 0, totbytes);
    if(status!= 0) {
        fprintf(stderr,"Error: While trying to reserve disk space = %zu bytes for file = `%s' \n"
                "posix_fallocate returned error status = %d\n", totbytes, outfile, status);
        perror(NULL);
        return EXIT_FAILURE;
    }
    if(close(fd) != 0){
        fprintf(stderr,"Error: While trying to close file descriptor after reserving disk space\n");
        perror(NULL);
    };
    
    return EXIT_SUCCESS;
}

