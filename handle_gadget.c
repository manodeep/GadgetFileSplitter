#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <inttypes.h>
#include <sys/types.h>
#include <sys/stat.h>
#define _XOPEN_SOURCE 600
#include <fcntl.h>
#include <unistd.h>

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

        /* So we have a dark-matter only simulation. Check that dark matter
           mass array is not 0.0 (i,e. particles don't have individual masses)*/
        if(hdr.mass[1] <= 0.0) {
            fprintf(stderr,"Error: This code can not handle dark matter particles with individual masses\n");
            return EXIT_FAILURE;
        }
        
        totnumpart += hdr.npart[1];
    }

    return totnumpart;
}

int generate_file_skeleton(const char *outfile, const size_t numpart, const size_t id_bytes)
{
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
    close(fd);

    return EXIT_SUCCESS;
}

