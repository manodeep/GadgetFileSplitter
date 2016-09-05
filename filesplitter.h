#pragma once

#ifdef __cplusplus
extern "C" {
#endif

    int filesplitter(int in_fd, int out_fd, off_t in_offset, off_t out_offset, const size_t bytes, void *userbuf, const size_t bufsize);
    int copy_bytes_with_preadwrite(int in_fd, int out_fd, off_t in_offset, off_t out_offset, const size_t bytes, void *userbuf, const size_t bufsize);
    int write_bytes(int out_fd, void *buf, const size_t bytes, off_t out_offset);
    
#ifdef __cplusplus
}
#endif
