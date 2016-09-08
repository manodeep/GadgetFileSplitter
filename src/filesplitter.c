#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

/* for memory-mapping*/
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include "filesplitter.h"
#include "utils.h"

int file_copy_bytes(FILE *inp, FILE *out, off_t in_offset, off_t out_offset, const size_t bytes, void *userbuf, const size_t bufsize);
int memcopy_bytes(const char *in, char *out, const off_t in_offset, const off_t out_offset, const size_t bytes);
int copy_bytes_with_preadwrite(int in_fd, int out_fd, off_t in_offset, off_t out_offset, const size_t bytes, void *userbuf, const size_t bufsize);
int write_bytes(int out_fd, void *buf, const size_t bytes, off_t out_offset);
int initialize_file_copy_with_memcpy(file_copy_union *un_fp, const char *fname, const int mode);
int initialize_file_copy_with_fread(file_copy_union *un_fp, const char *fname, const int mode);
int initialize_file_copy_with_pread(file_copy_union *un_fp, const char *fname, const int mode);
int finalize_file_copy_with_memcpy(file_copy_union *un_fp);
int finalize_file_copy_with_fread(file_copy_union *un_fp); 
int finalize_file_copy_with_pread(file_copy_union *un_fp);

int initialize_file_copy(file_copy_union *un_fp, const file_copy_options copy_kind, const char *fname, const int mode)
{
  switch(copy_kind) {
  case(PRDWR):return initialize_file_copy_with_pread(un_fp, fname, mode);
  case(MEMCP):return initialize_file_copy_with_memcpy(un_fp, fname, mode);
  case(FRDWR):return initialize_file_copy_with_fread(un_fp, fname, mode);
  default: fprintf(stderr,"Error: Could not initialize file copying union for copy type = %u. This copy type not implemented\n",copy_kind);break;
  }

  return EXIT_FAILURE;
}


int finalize_file_copy(file_copy_union *un_fp, const file_copy_options copy_kind)
{
  switch(copy_kind) {
  case(PRDWR):return finalize_file_copy_with_pread(un_fp);
  case(MEMCP):return finalize_file_copy_with_memcpy(un_fp);
  case(FRDWR):return finalize_file_copy_with_fread(un_fp);
  default: fprintf(stderr,"Error: Could not initialize file copying union for copy type = %u. This copy type not implemented\n",copy_kind);break;
  }

  return EXIT_FAILURE;
}


int finalize_file_copy_with_pread(file_copy_union *un_fp) 
{
  int status = close(un_fp->fd);
  if(status < 0) {
	fprintf(stderr,"Error: Could not close file descriptor\n");
	perror(NULL);
	return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}

int finalize_file_copy_with_fread(file_copy_union *un_fp) 
{
  int status = fclose(un_fp->fp);
  if(status < 0) {
	fprintf(stderr,"Error: Could not close file pointer\n");
	perror(NULL);
	return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}

int finalize_file_copy_with_memcpy(file_copy_union *un_fp) 
{
  int status = munmap(un_fp->fc, un_fp->st_size);
  if(status < 0) {
	fprintf(stderr,"Error: Could not unmap the memory-map\n");
	perror(NULL);
	return EXIT_FAILURE;
  }
  
  status = finalize_file_copy_with_pread(un_fp);
  if(status < 0) {
	return status;
  }

  return EXIT_SUCCESS;
}


int initialize_file_copy_with_fread(file_copy_union *un_fp, const char *fname, const int mode)
{
  const int len=4;
  char fread_mode[len];
  switch(mode){
  case(WRITE_ONLY_MODE): snprintf(fread_mode, len, "%s", "wb"); break;
  case(READ_ONLY_MODE): snprintf(fread_mode, len, "%s", "rb"); break;
  default:fprintf(stderr,"Error: The file modes supported are `read-only' (mode=%d) and `write-only' (mode=%d). "
						"Got unknown mode = %d\n", READ_ONLY_MODE, WRITE_ONLY_MODE, mode);return EXIT_FAILURE;
  }
  
  un_fp->fp = fopen(fname, fread_mode);
  if(un_fp->fp == NULL) {
	fprintf(stderr,"Error: Could not open file `%s'\n", fname);
	perror(NULL);
	return EXIT_FAILURE;
  }

  /* un_fp->fd = fileno(un_fp->fp); */
  /* if(mode == READ_ONLY_MODE) { */
  /* 	int status = posix_fadvise(un_fp->fd, 0, 0, POSIX_FADV_SEQUENTIAL | POSIX_FADV_NOREUSE); */
  /* 	if(status < 0) { */
  /* 	  fprintf(stderr,"Warning: Could not set file read/write hints to the kernel\n"); */
  /* 	  perror(NULL); */
  /* 	} */
  /* } */

  return EXIT_SUCCESS;
}


int initialize_file_copy_with_pread(file_copy_union *un_fp, const char *fname, const int mode)
{
  int fd_mode;
  switch(mode){
  case(WRITE_ONLY_MODE): fd_mode = O_WRONLY | O_LARGEFILE; break;
  case(READ_ONLY_MODE): fd_mode = O_RDONLY | O_LARGEFILE; break;
  default:fprintf(stderr,"Error: The file modes supported are `read-only' (mode=%d) and `write-only' (mode=%d). "
						"Got unknown mode = %d\n", READ_ONLY_MODE, WRITE_ONLY_MODE, mode);return EXIT_FAILURE;
  }

  un_fp->fd = open(fname, fd_mode);
  if(un_fp->fd < 0 ) {
	fprintf(stderr,"Error: Could not open file `%s' for memory-mapping\n", fname);
	perror(NULL);
	return EXIT_FAILURE;
  }

  /* Some optimization about reads */
  /* if(mode == READ_ONLY_MODE) { */
  /* 	int status = posix_fadvise(un_fp->fd, 0, 0, POSIX_FADV_SEQUENTIAL | POSIX_FADV_NOREUSE); */
  /* 	if(status < 0) { */
  /* 	  fprintf(stderr,"Warning: Could not set file read/write hints to the kernel\n"); */
  /* 	  perror(NULL); */
  /* 	} */
  /* } */

  return EXIT_SUCCESS;
}


int initialize_file_copy_with_memcpy(file_copy_union *un_fp, const char *fname, const int mode)
{  
  int fd_mode, mmap_mode;
  switch(mode){
  case(WRITE_ONLY_MODE): fd_mode = O_WRONLY | O_LARGEFILE;mmap_mode = PROT_WRITE; break;
  case(READ_ONLY_MODE): fd_mode = O_RDONLY | O_LARGEFILE; mmap_mode = PROT_READ; break;
  default:fprintf(stderr,"Error: The file modes supported are `read-only' (mode=%d) and `write-only' (mode=%d). "
						"Got unknown mode = %d\n", READ_ONLY_MODE, WRITE_ONLY_MODE, mode);return EXIT_FAILURE;
  }

  un_fp->fd = open(fname, fd_mode);
  if(un_fp->fd < 0 ) {
	fprintf(stderr,"Error: Could not open file `%s' for memory-mapping\n", fname);
	perror(NULL);
	return EXIT_FAILURE;
  }

  struct stat sb;
  if(fstat(un_fp->fd, &sb) < 0) {
	fprintf(stderr,"Error: Could not get file size for file `%s' for memory-mapping\n",fname);
	perror(NULL);
	return EXIT_FAILURE;
  }
  
  un_fp->st_size = sb.st_size;
  un_fp->fc  = mmap(NULL, un_fp->st_size, mmap_mode, MAP_SHARED, un_fp->fd, 0);
  if(un_fp->fc == NULL) {
	fprintf(stderr,"Error: Memory-mapping failed for file `%s'.\n", fname);
	close(un_fp->fd);
	return EXIT_FAILURE;
  }

  /* Some optimization about reads */
  /* if(mode == READ_ONLY_MODE) { */
  /* 	int status = posix_madvise(un_fp->fc, sb.st_size, POSIX_MADV_SEQUENTIAL); */
  /* 	if(status < 0) { */
  /* 	  fprintf(stderr,"Warning: Could not set file read/write hints to the kernel for the memory-map\n"); */
  /* 	  perror(NULL); */
  /* 	} */
  /* } */

  return EXIT_SUCCESS;
}


int filesplitter(file_copy_union *inp, file_copy_union *out, off_t in_offset, off_t out_offset, const size_t bytes, void *userbuf, const size_t bufsize, const file_copy_options copy_kind)
{
  switch(copy_kind) {
  case(PRDWR):return copy_bytes_with_preadwrite(inp->fd, out->fd, in_offset, out_offset, bytes, userbuf, bufsize);
  case(FRDWR):return file_copy_bytes(inp->fp, out->fp, in_offset, out_offset, bytes, userbuf, bufsize);
  case(MEMCP):return memcopy_bytes(inp->fc, out->fc, in_offset, out_offset, bytes);
  default: fprintf(stderr,"Error: copy_kind = %u for copying files is not implemented\n",copy_kind);break;
  }
  return EXIT_FAILURE;
}

int memcopy_bytes(const char *in, char *out, const off_t in_offset, const off_t out_offset, const size_t bytes) 
{
  
  memmove(out + (size_t) out_offset, in + (size_t) in_offset, bytes);
  return EXIT_SUCCESS;
}


int file_copy_bytes(FILE *inp, FILE *out, off_t in_offset, off_t out_offset, const size_t bytes, void *userbuf, const size_t bufsize)
{
  long this_offset = ftell(inp);
  if(this_offset < 0) {
	fprintf(stderr,"Error: Could not get file pointer position. Attempting to check if input file pointer is at %lld bytes\n", (long long) in_offset);
	perror(NULL);
	return this_offset;
  }
  if(this_offset != in_offset) {
	int status = fseek(inp, in_offset, SEEK_SET);
	fprintf(stderr,"Error:Could not seek to position = %lld in input file\n", (long long) in_offset);
	perror(NULL);
	return status;
  }

  this_offset = ftell(out);
  if(this_offset < 0) {
	fprintf(stderr,"Error: Could not get file pointer position. Attempting to check if output file pointer is at %lld bytes\n", (long long) out_offset);
	perror(NULL);
	return this_offset;
  }
  if(this_offset != out_offset) {
	int status = fseek(out, out_offset, SEEK_SET);
	fprintf(stderr,"Error:Could not seek to position = %lld in output file\n", (long long) out_offset);
	perror(NULL);
	return status;
  }
  
  size_t bytes_left = bytes;
  size_t total_bytes_read = 0, total_bytes_written = 0;
  while(bytes_left > 0) {
	const size_t bytes_to_read = bufsize > bytes_left ? bytes_left: bufsize;
	size_t nread = my_fread(userbuf, 1, bytes_to_read, inp);
	if(nread != bytes_to_read) {
	  fprintf(stderr,"Error: During read of %zu bytes, encountered while reading bytes [%zu -- %zu].\n",
			  bytes, total_bytes_read, total_bytes_read + bytes_to_read);
	  perror(NULL);
	  return EXIT_FAILURE;
	}
	total_bytes_read += (size_t) bytes_to_read;
            
	size_t nwritten = my_fwrite(userbuf, 1, bytes_to_read, out);
	if(nwritten != bytes_to_read) {
	  fprintf(stderr,"Error: During write of %zu bytes, encountered while writing bytes [%zu -- %zu].\n",
			  bytes, total_bytes_read, total_bytes_read + bytes_to_read);
	  perror(NULL);
	  return EXIT_FAILURE;

	}

	total_bytes_written += (size_t) bytes_to_read;
	bytes_left -= (size_t) bytes_to_read;
  }
  
  return EXIT_SUCCESS;
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
        const size_t bytes_to_read = bufsize > bytes_left ? bytes_left: bufsize;
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

