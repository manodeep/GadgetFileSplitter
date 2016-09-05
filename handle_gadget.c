#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <inttypes.h>

#include "utils.h"
#include "handle_gadget.h"

#define MAXLEN (1024)

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

int64_t count_total_number_of_particles(const char *filebase, const int nfiles)
{
    int64_t totnumpart = 0;
    char filename[MAXLEN];
    for(int ifile=0;ifile<nfiles;ifile++) {
        my_snprintf(filename,MAXLEN,"%s.%d",filebase,ifile);
        struct io_header hdr;
        int status = read_header(filename, &hdr);
        if(status != EXIT_SUCCESS) {
            return -1;
        }

        /* Check that this is a dark matter only */
        for(int type=0;type<6;type++) {
            if(type == 1) continue;

            if(hdr.npart[type] > 0) {
                fprintf(stderr,"Error: This code is only meant to convert dark matter only sims.\n"
                        "Thus, only npart[1] should have non-zero entry but I find npart[%d] = %d\n",
                        type, hdr.npart[type]);
                return EXIT_FAILURE;
            }
        }
        totnumpart += hdr.npart[1];
    }

    return totnumpart;
}



