// Copyright 2016 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once

#include "trace.h"

#include <mxio/debug.h>
#include <mxio/remoteio.h>

#include <stdlib.h>
#include <stdint.h>
#ifdef __Fuchsia__
#include <threads.h>
#endif
#include <sys/types.h>

#include <magenta/assert.h>
#include <magenta/compiler.h>
#include <magenta/types.h>

#include <mxio/vfs.h>
#include <mxio/dispatcher.h>

#define MXDEBUG 0

// VFS Helpers (vfs.c)
#define V_FLAG_DEVICE                 1
#define V_FLAG_MOUNT_READY            2
#define V_FLAG_RESERVED_MASK 0x0000FFFF

// On Fuchsia, the Block Device is transmitted by file descriptor, rather than
// by path. This can prevent some racy behavior relating to FS start-up.
#ifdef __Fuchsia__
#define FS_FD_BLOCKDEVICE 200
#endif

#ifdef __cplusplus

namespace fs {

#include <mxtl/macros.h>

// The VFS interface declares a default abtract Vnode class with
// common operations that may be overwritten.
//
// The ops are used for dispatch and the refcount
// is used by the generic RefAcquire and RefRelease.
//
// The lower half of flags (V_FLAG_RESERVED_MASK) is reserved
// for usage by fs::Vnode, but the upper half of flags may
// be used by subclasses of Vnode.

class Vnode {
public:
    void RefAcquire();
    void RefRelease();

    // Allocate iostate, create a channel, register it with the dispatcher
    // and return the other end.
    // Allows Vnode to act as server.
#ifdef __Fuchsia__
    mx_status_t Serve(uint32_t flags, mx_handle_t* out);

    // Extract handle(s), type, and extra info from a vnode.
    //  - type == '0' means the vn represents a non-local device.
    //  - If the vnode can be acquired, it is acquired by this function.
    //  - Returns the number of handles acquired.
    virtual mx_status_t GetHandles(uint32_t flags, mx_handle_t* hnds,
                                   uint32_t* type, void* extra, uint32_t* esize) = 0;
#endif

    virtual mx_status_t IoctlWatchDir(const void* in_buf, size_t in_len, void* out_buf, size_t out_len) {
        return ERR_NOT_SUPPORTED;
    }
    // Called when something is added to a watched directory.
    virtual void NotifyAdd(const char* name, size_t len) {}

    // TODO(smklein): Automate using RefPtr
    // Called when refcount reaches zero.
    virtual void Release() = 0;

    // Attempts to open vn, refcount++ on success.
    virtual mx_status_t Open(uint32_t flags) = 0;

    // Closes vn, refcount--
    virtual mx_status_t Close() = 0;

    // Read data from vn at offset.
    virtual ssize_t Read(void* data, size_t len, size_t off) {
        return ERR_NOT_SUPPORTED;
    }

    // Write data to vn at offset.
    virtual ssize_t Write(const void* data, size_t len, size_t off) {
        return ERR_NOT_SUPPORTED;
    }

    // Attempt to find child of vn, child returned with refcount++ on success.
    // Name is len bytes long, and does not include a null terminator.
    virtual mx_status_t Lookup(Vnode** out, const char* name, size_t len) {
        return ERR_NOT_SUPPORTED;
    }

    // Read attributes of vn.
    virtual mx_status_t Getattr(vnattr_t* a) {
        return ERR_NOT_SUPPORTED;
    }

    // Set attributes of vn.
    virtual mx_status_t Setattr(vnattr_t* a) {
        return ERR_NOT_SUPPORTED;
    }

    // Read directory entries of vn, error if not a directory.
    // FS-specific Cookie must be a buffer of vdircookie_t size or smaller.
    // Cookie must be zero'd before first call and will be used by.
    // the readdir implementation to maintain state across calls.
    // To "rewind" and start from the beginning, cookie may be zero'd.
    virtual mx_status_t Readdir(void* cookie, void* dirents, size_t len) {
        return ERR_NOT_SUPPORTED;
    }

    // Create a new node under vn.
    // Name is len bytes long, and does not include a null terminator.
    // Mode specifies the type of entity to create.
    virtual mx_status_t Create(Vnode** out, const char* name, size_t len, uint32_t mode) {
        return ERR_NOT_SUPPORTED;
    }

    // Performs the given ioctl op on vn.
    // On success, returns the number of bytes received.
    virtual ssize_t Ioctl(uint32_t op, const void* in_buf, size_t in_len,
                          void* out_buf, size_t out_len) {
        return ERR_NOT_SUPPORTED;
    }

    // Removes name from directory vn
    virtual mx_status_t Unlink(const char* name, size_t len, bool must_be_dir) {
        return ERR_NOT_SUPPORTED;
    }

    // Change the size of vn
    virtual mx_status_t Truncate(size_t len) {
        return ERR_NOT_SUPPORTED;
    }

    // Renames the path at oldname in olddir to the path at newname in newdir.
    // Called on the "olddir" vnode.
    // Unlinks any prior newname if it already exists.
    virtual mx_status_t Rename(Vnode* newdir,
                               const char* oldname, size_t oldlen,
                               const char* newname, size_t newlen,
                               bool src_must_be_dir, bool dst_must_be_dir) {
        return ERR_NOT_SUPPORTED;
    }

    // Creates a hard link to the 'target' vnode with a provided name in vndir
    virtual mx_status_t Link(const char* name, size_t len, Vnode* target) {
        return ERR_NOT_SUPPORTED;
    }

    // Syncs the vnode with its underlying storage
    virtual mx_status_t Sync() {
        return ERR_NOT_SUPPORTED;
    }

    // Attaches a handle to the vnode, if possible. Otherwise, returns an error.
    virtual mx_status_t AttachRemote(mx_handle_t h) {
        return ERR_NOT_SUPPORTED;
    }

    virtual ~Vnode() {};

    // The vnode is acting as a mount point for a remote filesystem or device.
    bool IsRemote() const { return remote_ > 0; }
    // The vnode is a device. Devices may opt to reveal themselves as directories
    // or endpoints, depending on context. For the purposes of our VFS layer,
    // during path traversal, devices are NOT treated as mount points, even though
    // they contain remote handles.
    bool IsDevice() const { return (flags_ & V_FLAG_DEVICE) && IsRemote(); }
    // The vnode is "open elsewhere".
    bool IsBusy() const { return refcount_ > 1; }

    mx_handle_t DetachRemote() {
        mx_handle_t h = remote_;
        remote_ = MX_HANDLE_INVALID;
        flags_ &= ~V_FLAG_MOUNT_READY;
        return h;
    }

    // TODO(smklein): Encapsulate the "remote_" flag more, here and in "GetHandles",
    // so we can avoid leaking information outside the Vnode / Vfs classes.
    mx_handle_t WaitForRemote();
protected:
    DISALLOW_COPY_ASSIGN_AND_MOVE(Vnode);
    Vnode() : flags_(0), remote_(MX_HANDLE_INVALID), refcount_(1) {};

    uint32_t flags_;
    mx_handle_t remote_;
private:
    uint32_t refcount_;
};

struct Vfs {
    // Walk from vn --> out until either only one path segment remains or we
    // encounter a remote filesystem.
    static mx_status_t Walk(Vnode* vn, Vnode** out, const char* path, const char** pathout);
    // Traverse the path to the target vnode, and create / open it using
    // the underlying filesystem functions (lookup, create, open).
    static mx_status_t Open(Vnode* vn, Vnode** out, const char* path, const char** pathout,
                            uint32_t flags, uint32_t mode);
    static mx_status_t Unlink(Vnode* vn, const char* path, size_t len);
    static mx_status_t Link(Vnode* vn, const char* oldpath, const char* newpath,
                            const char** oldpathout, const char** newpathout);
    static mx_status_t Rename(Vnode* vn, const char* oldpath, const char* newpath,
                              const char** oldpathout, const char** newpathout);
    static mx_status_t Close(Vnode* vn); // TODO(smklein): This has questionable utility
    static ssize_t Ioctl(Vnode* vn, uint32_t op, const void* in_buf, size_t in_len,
                         void* out_buf, size_t out_len);

    // Pins a handle to a remote filesystem onto a vnode, if possible.
    static mx_status_t InstallRemote(Vnode* vn, mx_handle_t h);
    // Unpin a handle to a remote filesystem from a vnode, if one exists.
    static mx_status_t UninstallRemote(Vnode* vn, mx_handle_t* h);
};

mx_status_t vfs_fill_dirent(vdirent_t* de, size_t delen,
                            const char* name, size_t len, uint32_t type);

} // namespace fs

using Vnode = fs::Vnode;

#else  // ifdef __cplusplus

typedef struct Vnode Vnode;

#endif // ifdef __cplusplus

__BEGIN_CDECLS

typedef struct vnattr vnattr_t;
typedef struct vdirent vdirent_t;

typedef struct vdircookie {
    uint64_t n;
    void* p;
} vdircookie_t;

// A lock which should be used to protect lookup and walk operations
#ifdef __Fuchsia__
extern mtx_t vfs_lock;
#endif
extern mxio_dispatcher_t* vfs_dispatcher;

// The following function must be defined by the filesystem linking
// with this VFS layer.

// Handle incoming mxrio messages.
mx_status_t vfs_handler(mxrio_msg_t* msg, mx_handle_t rh, void* cookie);

typedef struct vfs_iostate {
    Vnode* vn;
    vdircookie_t dircookie;
    size_t io_off;
    uint32_t io_flags;
} vfs_iostate_t;

// Send an unmount signal on a handle to a filesystem and await a response.
mx_status_t vfs_unmount_handle(mx_handle_t h, mx_time_t timeout);

// Unpins all remote filesystems in the current filesystem, and waits for the
// response of each one with the provided timeout.
mx_status_t vfs_uninstall_all(mx_time_t timeout);

// Generic implementation of vfs_handler, which dispatches messages to fs operations.
mx_status_t vfs_handler_generic(mxrio_msg_t* msg, mx_handle_t rh, void* cookie);

__END_CDECLS
