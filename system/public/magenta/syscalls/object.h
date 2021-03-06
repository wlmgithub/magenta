// Copyright 2016 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once

#include <magenta/types.h>

__BEGIN_CDECLS

// ask clang format not to mess up the indentation:
// clang-format off

// Valid topics for mx_object_get_info.
typedef enum {
    MX_INFO_NONE                       = 0,
    MX_INFO_HANDLE_VALID               = 1,
    MX_INFO_HANDLE_BASIC               = 2,  // mx_info_handle_basic_t[1]
    MX_INFO_PROCESS                    = 3,  // mx_info_process_t[1]
    MX_INFO_PROCESS_THREADS            = 4,  // mx_koid_t[n]
    MX_INFO_RESOURCE_CHILDREN          = 5,  // mx_rrec_t[n]
    MX_INFO_RESOURCE_RECORDS           = 6,  // mx_rrec_t[n]
    MX_INFO_VMAR                       = 7,  // mx_info_vmar_t
    MX_INFO_JOB_CHILDREN               = 8,  // mx_koid_t[n]
    MX_INFO_JOB_PROCESSES              = 9,  // mx_koid_t[n]
    MX_INFO_THREAD                     = 10, // mx_info_thread_t[1]
    MX_INFO_THREAD_EXCEPTION_REPORT    = 11, // mx_exception_report_t[1]
    MX_INFO_TASK_STATS                 = 12, // mx_info_task_stats_t[1]
    MX_INFO_LAST
} mx_object_info_topic_t;

typedef enum {
    MX_OBJ_TYPE_NONE                = 0,
    MX_OBJ_TYPE_PROCESS             = 1,
    MX_OBJ_TYPE_THREAD              = 2,
    MX_OBJ_TYPE_VMEM                = 3,
    MX_OBJ_TYPE_CHANNEL             = 4,
    MX_OBJ_TYPE_EVENT               = 5,
    MX_OBJ_TYPE_IOPORT              = 6,
    MX_OBJ_TYPE_INTERRUPT           = 9,
    MX_OBJ_TYPE_IOMAP               = 10,
    MX_OBJ_TYPE_PCI_DEVICE          = 11,
    MX_OBJ_TYPE_LOG                 = 12,
    MX_OBJ_TYPE_WAIT_SET            = 13,
    MX_OBJ_TYPE_SOCKET              = 14,
    MX_OBJ_TYPE_RESOURCE            = 15,
    MX_OBJ_TYPE_EVENT_PAIR          = 16,
    MX_OBJ_TYPE_JOB                 = 17,
    MX_OBJ_TYPE_VMAR                = 18,
    MX_OBJ_TYPE_FIFO                = 19,
    MX_OBJ_TYPE_IOPORT2             = 20,
    MX_OBJ_TYPE_HYPERVISOR          = 21,
    MX_OBJ_TYPE_GUEST               = 22,
    MX_OBJ_TYPE_LAST
} mx_obj_type_t;

typedef enum {
    MX_OBJ_PROP_NONE            = 0,
    MX_OBJ_PROP_WAITABLE        = 1,
} mx_obj_props_t;

typedef struct mx_info_handle_basic {
    // The unique id assigned by kernel to the object referenced by the
    // handle.
    mx_koid_t koid;

    // The immutable rights assigned to the handle. Two handles that
    // have the same koid and the same rights are equivalent and
    // interchangeable.
    mx_rights_t rights;

    // The object type: channel, event, socket, etc.
    uint32_t type;                // mx_obj_type_t;

    // The koid of the logical counterpart or parent object of the
    // object referenced by the handle. Otherwise this value is zero.
    mx_koid_t related_koid;

    // Set to MX_OBJ_PROP_WAITABLE if the object referenced by the
    // handle can be waited on; zero otherwise.
    uint32_t props;               // mx_obj_props_t;
} mx_info_handle_basic_t;

typedef struct mx_info_process {
    // The process's return code; only valid if |exited| is true.
    // Guaranteed to be non-zero if the process was killed by |mx_task_kill|.
    int return_code;

    // True if the process has ever left the initial creation state,
    // even if it has exited as well.
    bool started;

    // If true, the process has exited and |return_code| is valid.
    bool exited;

    // True if a debugger is attached to the process.
    bool debugger_attached;
} mx_info_process_t;

typedef struct mx_info_thread {
    // If nonzero, the thread has gotten an exception and is waiting for
    // the exception to be handled by the specified port.
    // The value is one of MX_EXCEPTION_PORT_TYPE_*.
    uint32_t wait_exception_port_type;
} mx_info_thread_t;

// Statistics about resources (e.g., memory) used by a task. Can be relatively
// expensive to gather.
typedef struct mx_info_task_stats {
    // The total size of mapped memory ranges in the task.
    // Not all will be backed by physical memory.
    size_t mem_mapped_bytes;

    // The amount of mapped address space backed by physical memory.
    // Will be no larger than mem_mapped_bytes.
    // Some of the pages may be double-mapped (and thus double-counted),
    // or may be shared with other tasks.
    size_t mem_committed_bytes;
} mx_info_task_stats_t;

typedef struct mx_info_vmar {
    // Base address of the region.
    uintptr_t base;

    // Length of the region, in bytes.
    size_t len;
} mx_info_vmar_t;


// Object properties.

// Argument is MX_POLICY_BAD_HANDLE_... (below, uint32_t).
#define MX_PROP_BAD_HANDLE_POLICY           1u
// Argument is a uint32_t.
#define MX_PROP_NUM_STATE_KINDS             2u
// Argument is a char[MX_MAX_NAME_LEN].
#define MX_PROP_NAME                        3u

#if __x86_64__
// Argument is a uintptr_t.
#define MX_PROP_REGISTER_FS                 4u
#endif

// Argument is the value of ld.so's _dl_debug_addr, a uintptr_t.
#define MX_PROP_PROCESS_DEBUG_ADDR          5u

// Policies for MX_PROP_BAD_HANDLE_POLICY:
#define MX_POLICY_BAD_HANDLE_IGNORE         0u
#define MX_POLICY_BAD_HANDLE_LOG            1u
#define MX_POLICY_BAD_HANDLE_EXIT           2u

__END_CDECLS
