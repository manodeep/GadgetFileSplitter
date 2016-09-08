#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#include "gadget_defs.h"
#include "filesplitter.h"

#ifdef __cplusplus
extern "C" {
#endif

  int split_gadget(const char *filebase, const char *outfilebase, const int noutfiles, const file_copy_options copy_kind);
    
#ifdef __cplusplus
}
#endif
