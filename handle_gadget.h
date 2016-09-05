#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "gadget_defs.h"
    
int64_t count_total_number_of_particles(const char *filebase, const int nfiles);
int read_header(const char *filename, struct io_header *header);

#ifdef __cplusplus
}
#endif
