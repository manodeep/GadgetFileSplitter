#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "filesplitter.h"

int filesplitter(int in_fd, int out_fd, off_t in_offset, off_t out_offset, const size_t bytes, void *userbuf, const size_t bufsize, const file_copy_options option)
{
  switch(option) {
  case(PRDWR):return copy_bytes_with_preadwrite(in_fd, out_fd, in_offset, out_offset, bytes, userbuf, bufsize);
  /* case(MEMCP):return memcopy_bytes(in_fd, out_fd, in_offset, out_offset, bytes, userbuf, bufsize); */
  default: fprintf(stderr,"Error: option = %u for copying files is not implemented\n",option);break;
  }
  return EXIT_FAILURE;
}

int write_bytes(int out_fd, void *buf, const size_t bytes, off_t out_offset)
{
    size_t bytes_written = 0;
    size_t bytes_left = bytes;
    while(bytes_left > 0) {
        ssize_t this_bytes = pwrite(out_fd, buf, bytes_left, out_offset);
        if(this_bytes < 0) {
            fprintf(stderr,"Error: During writing out %zu bytes, encountered an error attempting to write bytes [%zu -- %zu]\n",
                    bytes, bytes_written, bytes_written + bytes_left);
            perror(NULL);
            return EXIT_FAILURE;
        }
        bytes_written += (size_t) this_bytes;
        out_offset += this_bytes;
        bytes_left -= (size_t) this_bytes;
    }

    return EXIT_SUCCESS;
}

int copy_bytes_with_preadwrite(int in_fd, int out_fd, off_t in_offset, off_t out_offset, const size_t bytes, void *userbuf, const size_t bufsize)
{
    if(bytes == 0) {
        return EXIT_SUCCESS;
    }

    size_t bytes_left = bytes;
    size_t total_bytes_written = 0;
    size_t total_bytes_read = 0;
    while(bytes_left > 0) {
        const size_t bytes_to_read = bufsize > bytes ? bytes: bufsize;
        ssize_t bytes_read = pread(in_fd, userbuf, bytes_to_read, in_offset);
        if(bytes_read < 0) {
            fprintf(stderr,"Error: During read of %zu bytes, encountered while reading bytes [%zu -- %zu].\n",
                    bytes, total_bytes_read, total_bytes_read + bytes_to_read);
            perror(NULL);
            return EXIT_FAILURE;
        }
        in_offset += (size_t) bytes_read;
        total_bytes_read += (size_t) bytes_read;
            
        int write_status = write_bytes(out_fd, userbuf, bytes_read, out_offset);
        if(write_status != EXIT_SUCCESS) {
            return write_status;
        }
        out_offset += (size_t) bytes_read;
        total_bytes_written += (size_t) bytes_read;
        bytes_left -= (size_t) bytes_read;
    }

    return EXIT_SUCCESS;
}

