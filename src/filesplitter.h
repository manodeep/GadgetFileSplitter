#pragma once
#include <stdio.h>

typedef enum
{
  PRDWR = 0,
  MEMCP = 1,
  FRDWR = 2,
  NUM_FCPY_OPT
} file_copy_options;

typedef struct
{
  union{
	char *fc;
	FILE *fp;
	int fd;
  };
  off_t st_size;
} file_copy_union;


#define WRITE_ONLY_MODE 1
#define READ_ONLY_MODE 2

#ifdef __cplusplus
extern "C" {
#endif

  int initialize_file_copy(file_copy_union *un_fp, const file_copy_options copy_kind, const char *fname, const int mode);
  int filesplitter(file_copy_union *inp, file_copy_union *out, off_t in_offset, off_t out_offset, const size_t bytes, void *userbuf, const size_t bufsize, const file_copy_options copy_kind);
  int finalize_file_copy(file_copy_union *un_fp, const file_copy_options copy_kind);
    
#ifdef __cplusplus
}
#endif
