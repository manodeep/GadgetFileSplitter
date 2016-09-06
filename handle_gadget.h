#pragma once

#include "gadget_defs.h"

#ifdef __cplusplus
extern "C" {
#endif

    int64_t count_total_number_of_particles(const char *filebase, int32_t nfiles, int32_t *numpart_in_input_file);
    int read_header(const char *filename, struct io_header *header);
    int32_t find_numfiles(const char *filebase);
    int gadget_snapshot_create(const char *filebase, const char *outfilename, struct file_mapping *fmap, const size_t id_bytes);
    int generate_file_skeleton(const char *outfile, const size_t numpart, const size_t id_bytes);
    int write_hdr_to_stream(FILE *fp, struct io_header *header);
    ssize_t find_id_bytes(const char *filebase);
    int write_front_and_end_padding_bytes_to_stream(uint32_t dummy, FILE *fp);
    off_t get_offset_from_npart(const int32_t npart, field_type field);
    int copy_all_dmfields_from_gadget_snapshot(int in_fd, int out_fd, const int32_t start, const int32_t end, const int32_t nwritten, const size_t id_bytes);
    
#ifdef __cplusplus
}
#endif
