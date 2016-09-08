/* File: progressbar.h */

#pragma once

#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif


    void init_my_progressbar(const int64_t N,int *interrupted);
    void my_progressbar(const int64_t curr_index,int *interrupted);
    void finish_myprogressbar(int *interrupted);

#ifdef __cplusplus
}
#endif
