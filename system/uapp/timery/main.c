// Copyright 2016 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <fcntl.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>
#include <sys/param.h>

#include <magenta/syscalls.h>
#include <magenta/types.h>

#include <mxio/io.h>


int main(int argc, const char** argv) {
    size_t pages = 1000;
    size_t vmo_size = PAGE_SIZE * pages;
    mx_handle_t vmo_handle;

    mx_status_t status = mx_vmo_create(vmo_size, 0, &vmo_handle);
    if (status != NO_ERROR) {
        fprintf(stderr, "mx_vmo_create fail\n");
        return -1;
    }
    status = mx_vmo_op_range(vmo_handle, MX_VMO_OP_COMMIT, 0, vmo_size, NULL, 0);
    if (status != NO_ERROR) {
        fprintf(stderr, "MX_VMO_OP_COMMIT fail\n");
        return -1;
    }

    mx_paddr_t* paddrs = malloc(pages * sizeof(mx_paddr_t));

    mx_time_t start = mx_time_get(MX_CLOCK_MONOTONIC);

    int iterations = 1000000;
    for (int i = 0; i < iterations; i++) {
        status = mx_vmo_op_range(vmo_handle, MX_VMO_OP_LOOKUP, 0, vmo_size, paddrs, sizeof(mx_paddr_t) * pages);
        if (status != NO_ERROR) {
            fprintf(stderr, "MX_VMO_OP_LOOKUP fail\n");
            return -1;
        }
    }

    mx_time_t end = mx_time_get(MX_CLOCK_MONOTONIC);
    uint64_t usec = (end - start) / 1000;
    double seconds = ((double)usec / 1000000.0);

    printf("%d MX_VMO_OP_LOOKUP in %lf seconds\n", iterations, seconds);

    start = mx_time_get(MX_CLOCK_MONOTONIC);

    for (int i = 0; i < iterations; i++) {
        size_t s;
        status = mx_vmo_get_size(vmo_handle, &s);
        if (status != NO_ERROR) {
            fprintf(stderr, "mx_vmo_get_size fail\n");
            return -1;
        }
    }
    end = mx_time_get(MX_CLOCK_MONOTONIC);
    usec = (end - start) / 1000;
    seconds = ((double)usec / 1000000.0);

    printf("%d mx_vmo_get_size in %lf seconds\n", iterations, seconds);



    start = mx_time_get(MX_CLOCK_MONOTONIC);

    for (int i = 0; i < iterations; i++) {
        free(paddrs);
        paddrs = malloc(pages * sizeof(mx_paddr_t));

    }
   
    end = mx_time_get(MX_CLOCK_MONOTONIC);
    usec = (end - start) / 1000;
    seconds = ((double)usec / 1000000.0);

    printf("%d malloc/free in %lf seconds\n", iterations, seconds);

    return 0;
}
