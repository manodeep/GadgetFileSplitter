#pragma once

#include "gadget_defs.h"
#include "filesplitter.h"

#ifdef __cplusplus
extern "C" {
#endif

  int64_t count_total_number_of_particles(const char *filebase, int32_t nfiles, int32_t *numpart_in_input_file);
  int read_header(const char *filename, struct io_header *header);
  int gadget_snapshot_create(const char *filebase, const char *outfilename, struct file_mapping *fmap, 
							 const size_t id_bytes, const int noutfiles, const file_copy_options copy_kind);
  ssize_t find_id_bytes(const char *filebase);
  int32_t find_numfiles(const char *filebase);

#ifdef __cplusplus
}
#endif
