#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <limits.h>
#include <inttypes.h>

#include "split_gadget.h"
#include "handle_gadget.h"

#ifndef MAXLEN
#define MAXLEN (1024)
#endif

int allocate_file_mapping(struct file_mapping *fmap, const int64_t numfiles)
{
    if(fmap == NULL) {
        fprintf(stderr,"Error In %s> Input pointer can not be NULL\n",__FUNCTION__);
        return EXIT_FAILURE;
    }
    size_t totbytes = 0;
    fmap->numfiles = numfiles;
    size_t bytes = sizeof(*(fmap->input_file_id)) * numfiles;
    fmap->input_file_id = malloc(bytes);
    totbytes += bytes;
    
    bytes = sizeof(*(fmap->input_file_start_particle)) * numfiles;
    fmap->input_file_start_particle = malloc(bytes);
    totbytes += bytes;
    
    bytes = sizeof(*(fmap->input_file_end_particle) * numfiles);
    fmap->input_file_end_particle = malloc(bytes);
    totbytes += bytes;

    bytes = sizeof(*(fmap->output_file_nwritten)) * numfiles;
    fmap->output_file_nwritten = malloc(bytes);
    totbytes += bytes;
    if(fmap->input_file_start_particle == NULL ||
       fmap->input_file_end_particle == NULL ||
       fmap->input_file_id == NULL ||
       fmap->output_file_nwritten == NULL) {
        fprintf(stderr,"Error: In %s> Could not allocate memory for file mapping for particles. Requested memory = %zu bytes\n",
                __FUNCTION__, totbytes);
        free_file_mapping(fmap);
        return EXIT_FAILURE;
    }
    
    return EXIT_SUCCESS;
}


void free_file_mapping(struct file_mapping *fmap)
{
    if(fmap == NULL) {
        return;
    }

    free(fmap->input_file_start_particle);
    free(fmap->input_file_end_particle);
    free(fmap->input_file_id);
    free(fmap->output_file_nwritten);

    fmap->input_file_start_particle = NULL;
    fmap->input_file_end_particle = NULL;
    fmap->input_file_id = NULL;
    fmap->output_file_nwritten = NULL;
}


int split_gadget(const char *filebase, const char *outfilebase, const int noutfiles)
{
    struct file_mapping *fmap = NULL;
    
    fmap = malloc(sizeof(*(fmap)) * noutfiles);
    if(fmap == NULL) {
        fprintf(stderr,"Error: Could not allocate memory to store all the file mapping for particles. "
                "Number of bytes requested = %zu\n", sizeof(*fmap) * noutfiles);
        return EXIT_FAILURE;
    }

    const int32_t num_input_files = find_numfiles(filebase);
    int32_t *numpart_in_input_file = NULL;
    size_t bytes = sizeof(*numpart_in_input_file) * num_input_files;
    numpart_in_input_file = malloc(bytes);
    if(numpart_in_input_file == NULL) {
        fprintf(stderr,"Error: Could not allocate memory to hold particles numbers per file. Requested bytes = %zu\n",
                bytes);
        return EXIT_FAILURE;
    }
    int64_t totnumpart = count_total_number_of_particles(filebase, num_input_files, numpart_in_input_file);
    fprintf(stdout,"Found %"PRId64" dark matter particles spread over %d input files\n",
            totnumpart, num_input_files);

    {
        /* Check that it is possible to split/merge the snapshot into the requested
           number of files. Biggest constraint comes from writing the fortran-style
           binary files -> 4 bytes to contain the number of data-bytes following the pad.

           Now, Gadget writes out npart * 3 * sizeof(float) for the 'POS/VEL' fields.
           (ID could at most be npart * sizeof(int64_t) bytes, therefore the 'POS/VEL'
           fields are the constraint)

           The final constraint is then: npart * 3 * sizeof(float) < UINT_MAX
           This condition automatically implies npart < INT_MAX
         */
        
        const int64_t npart_per_file = totnumpart/noutfiles;
        const int spill = totnumpart % noutfiles;
        const int extra = spill > 0 ? 1:0;
        const size_t dummy = (npart_per_file + extra) * sizeof(float) * 3;//this is the largest 4-byte padding that will be
        if(dummy > UINT_MAX) {
            fprintf(stderr,"Error: The requested number of output files = %d will result in garbage for the\n"
                    "padding bytes. Please increase the number of output files and run again. You will need to increase\n"
                    "the number of outputfiles ~%d times\n",noutfiles, (int) ((dummy/UINT_MAX) + 1));
            return EXIT_FAILURE;
        }
    }

    /* Note using 'int' here */
    const int32_t npart_per_file = (int) (totnumpart/noutfiles);
    int32_t spill = totnumpart % noutfiles;

    /* Okay now generate the file mapping for each output file */
    int32_t inp = 0;//input file
    int32_t curr_offset = 0;
    for(int32_t out=0;out<noutfiles;out++) {
        fmap[out].numfiles = 0;


        /* First figure out how many input files are mapped into this output file */
        int32_t numfiles = 1;
        int32_t start_inp = inp;
        int32_t start_offset = curr_offset;
        int32_t npart=0;
        
        /* Have to figure out how many particles to take from which input files */
        while(inp < num_input_files) {
            int32_t particles_left_inp = numpart_in_input_file[inp] - curr_offset;
            if(inp == (num_input_files-1) && out == (noutfiles-1)) {
                /* Last input file on last output file -> assign all remaining particles */
                npart += particles_left_inp;
                break;
            }
            
            if((npart + particles_left_inp) < npart_per_file) {
                npart += particles_left_inp;
                inp++;
                numfiles++;
                curr_offset = 0;
            } else {
                //do not increment numfiles or inp.
                int32_t num_particles = npart_per_file - npart;
                curr_offset += num_particles;
                /* Only assign the spill if there are particles left in this file.
                   Worst case, the last output file will have some extra particles */
                if(spill > 0 && particles_left_inp > num_particles) {
                    npart++;
                    spill--;
                    curr_offset++;
                }
                break;
            }
        }/* Done with mapping the input files to the output files */

        int status = allocate_file_mapping(&fmap[out], numfiles);
        if(status != EXIT_SUCCESS) {
            return status;
        }
        fmap[out].numpart = npart;
        
        int32_t numpart_written_this_file = 0;
        for(int i=0;i<numfiles;i++) {
            const int this_inp = start_inp + i;
            fmap[out].input_file_id[i] = this_inp;
            if(inp == start_inp) {
                if(numfiles != 1) {
                    fprintf(stderr,"Error: There is only one input file (start=%d, end=%d) but the number of files requested (=%d) is not 1\n",
                            start_inp, inp, numfiles);
                    return EXIT_FAILURE;
                }
                if(start_offset < 0 || start_offset >= numpart_in_input_file[this_inp] ||
                   curr_offset  <= 0 || curr_offset  > numpart_in_input_file[this_inp] )
                {
                    fprintf(stderr,"Error: Start (=%d) or end offset (=%d) is incorrect. The offsets must be in range [0, %d] (not inclusive last edge for start)\n",
                            start_offset, curr_offset, numpart_in_input_file[this_inp]);
                    return EXIT_FAILURE;
                }
                fmap[out].input_file_start_particle[i] = start_offset;
                fmap[out].input_file_end_particle[i] = curr_offset;
            } else {
                /* There are multple input files for this output file */
                /* Assign entire input file to this output file (wrong, will get fixed immediately) */
                fmap[out].input_file_start_particle[i] = 0;
                fmap[out].input_file_end_particle[i] = numpart_in_input_file[this_inp];

                /* Okay now fix the start for the first file */
                if(i == 0) {
                    if(start_offset < 0 || start_offset >= numpart_in_input_file[this_inp]) {
                        fprintf(stderr,"Error: Start offset (=%d) must be in range [0, %d)\n",
                                start_offset, numpart_in_input_file[this_inp]);
                        return EXIT_FAILURE;
                    }
                    fmap[out].input_file_start_particle[i] = start_offset;
                }

                /* Now the end for the last file */
                if(i == (numfiles-1) ) {
                    if(curr_offset <= 0 || curr_offset > numpart_in_input_file[this_inp]) {
                        fprintf(stderr,"Error: End offset (=%d) must be in range (0, %d]\n",
                                curr_offset, numpart_in_input_file[this_inp]);
                        return EXIT_FAILURE;
                    }
                    fmap[out].input_file_end_particle[i] = curr_offset;
                }
            }

            if(numfiles == 1) {
                fmap[out].output_file_nwritten[i] = 0;
            } else {
                fmap[out].output_file_nwritten[i] = numpart_written_this_file;
            }
            numpart_written_this_file += fmap[out].input_file_end_particle[i] - fmap[out].input_file_start_particle[i];
        }
    }

    /* All the file-mappings have been created -> can be farmed out */
    ssize_t id_bytes = find_id_bytes(filebase);
    if(id_bytes < 0) {
        return EXIT_FAILURE;
    }

    for(int i=0;i<noutfiles;i++) {
        int status = gadget_snapshot_create(filebase, outfilebase, &fmap[i], (size_t) id_bytes);
        if(status != EXIT_SUCCESS) {
            return status;
        }
    }

    free(numpart_in_input_file);
    for(int i=0;i<noutfiles;i++){
        free_file_mapping(&fmap[i]);
    }
    
    free(fmap);
    return EXIT_SUCCESS;
}
    

