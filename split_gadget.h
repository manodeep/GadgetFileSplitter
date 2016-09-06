#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#include "gadget_defs.h"


#ifdef __cplusplus
extern "C" {
#endif

    int allocate_file_mapping(struct file_mapping *, const int64_t);
    void free_file_mapping(struct file_mapping *);
    int split_gadget(const char *filebase, const char *outfilebase, const int noutfiles);
    
#ifdef __cplusplus
}
#endif
