// Copyright 2016 The Fuchsia Authors
//
// Use of this source code is governed by a MIT-style
// license that can be found in the LICENSE file or at
// https://opensource.org/licenses/MIT

#include <err.h>
#include <inttypes.h>
#include <trace.h>

#include <kernel/vm/vm_object.h>

#include <lib/user_copy.h>
#include <lib/user_copy/user_ptr.h>

#include <magenta/handle_owner.h>
#include <magenta/magenta.h>
#include <magenta/process_dispatcher.h>
#include <magenta/user_copy.h>
#include <magenta/vm_object_dispatcher.h>

#include <mxtl/ref_ptr.h>

#include "syscalls_priv.h"

#define LOCAL_TRACE 0

mx_status_t sys_vmo_create(uint64_t size, uint32_t options, user_ptr<mx_handle_t> _out) {
    LTRACEF("size %#" PRIx64 "\n", size);

    if (options)
        return ERR_INVALID_ARGS;

    // create a vm object
    mxtl::RefPtr<VmObject> vmo = VmObjectPaged::Create(0, size);
    if (!vmo)
        return ERR_NO_MEMORY;

    // create a Vm Object dispatcher
    mxtl::RefPtr<Dispatcher> dispatcher;
    mx_rights_t rights;
    mx_status_t result = VmObjectDispatcher::Create(mxtl::move(vmo), &dispatcher, &rights);
    if (result != NO_ERROR)
        return result;

    // create a handle and attach the dispatcher to it
    HandleOwner handle(MakeHandle(mxtl::move(dispatcher), rights));
    if (!handle)
        return ERR_NO_MEMORY;

    auto up = ProcessDispatcher::GetCurrent();

    if (_out.copy_to_user(up->MapHandleToValue(handle)) != NO_ERROR)
        return ERR_INVALID_ARGS;

    up->AddHandle(mxtl::move(handle));

    return NO_ERROR;
}

mx_status_t sys_vmo_read(mx_handle_t handle, user_ptr<void> _data,
                         uint64_t offset, size_t len, user_ptr<size_t> _actual) {
    LTRACEF("handle %d, data %p, offset %#" PRIx64 ", len %#zx\n",
            handle, _data.get(), offset, len);

    auto up = ProcessDispatcher::GetCurrent();

    // lookup the dispatcher from handle
    mxtl::RefPtr<VmObjectDispatcher> vmo;
    mx_status_t status = up->GetDispatcherWithRights(handle, MX_RIGHT_READ, &vmo);
    if (status != NO_ERROR)
        return status;

    // do the read operation
    size_t nread;
    status = vmo->Read(_data, len, offset, &nread);
    if (status == NO_ERROR)
        status = _actual.copy_to_user(nread);

    return status;
}

mx_status_t sys_vmo_write(mx_handle_t handle, user_ptr<const void> _data,
                          uint64_t offset, size_t len, user_ptr<size_t> _actual) {
    LTRACEF("handle %d, data %p, offset %#" PRIx64 ", len %#zx\n",
            handle, _data.get(), offset, len);

    auto up = ProcessDispatcher::GetCurrent();

    // lookup the dispatcher from handle
    mxtl::RefPtr<VmObjectDispatcher> vmo;
    mx_status_t status = up->GetDispatcherWithRights(handle, MX_RIGHT_WRITE, &vmo);
    if (status != NO_ERROR)
        return status;

    // do the write operation
    size_t nwritten;
    status = vmo->Write(_data, len, offset, &nwritten);
    if (status == NO_ERROR)
        status = _actual.copy_to_user(nwritten);

    return status;
}

mx_status_t sys_vmo_get_size(mx_handle_t handle, user_ptr<uint64_t> _size) {
    LTRACEF("handle %d, sizep %p\n", handle, _size.get());

    auto up = ProcessDispatcher::GetCurrent();

    // lookup the dispatcher from handle
    mxtl::RefPtr<VmObjectDispatcher> vmo;
    mx_status_t status = up->GetDispatcher(handle, &vmo);
    if (status != NO_ERROR)
        return status;

    // no rights check, anyone should be able to get the size

    // do the operation
    uint64_t size = 0;
    status = vmo->GetSize(&size);

    // copy the size back, even if it failed
    if (_size.copy_to_user(size) != NO_ERROR)
        return ERR_INVALID_ARGS;

    return status;
}

mx_status_t sys_vmo_set_size(mx_handle_t handle, uint64_t size) {
    LTRACEF("handle %d, size %#" PRIx64 "\n", handle, size);

    auto up = ProcessDispatcher::GetCurrent();

    // lookup the dispatcher from handle
    mxtl::RefPtr<VmObjectDispatcher> vmo;
    mx_status_t status = up->GetDispatcherWithRights(handle, MX_RIGHT_WRITE, &vmo);
    if (status != NO_ERROR)
        return status;

    // do the operation
    return vmo->SetSize(size);
}

mx_status_t sys_vmo_op_range(mx_handle_t handle, uint32_t op, uint64_t offset, uint64_t size,
                             user_ptr<void> _buffer, size_t buffer_size) {
    LTRACEF("handle %d op %u offset %#" PRIx64 " size %#" PRIx64
            " buffer %p buffer_size %zu\n",
            handle, op, offset, size, _buffer.get(), buffer_size);

    auto up = ProcessDispatcher::GetCurrent();

    // lookup the dispatcher from handle
    // TODO: test rights
    mxtl::RefPtr<VmObjectDispatcher> vmo;
    mx_status_t status = up->GetDispatcher(handle, &vmo);
    if (status != NO_ERROR)
        return status;

    return vmo->RangeOp(op, offset, size, _buffer, buffer_size);
}

mx_status_t sys_vmo_clone(mx_handle_t handle, uint32_t options, uint64_t offset, uint64_t size,
        user_ptr<mx_handle_t>(_out_handle)) {
    LTRACEF("handle %d options %#x offset %#" PRIx64 " size %#" PRIx64 "\n",
            handle, options, offset, size);

    auto up = ProcessDispatcher::GetCurrent();

    mx_status_t status;
    mxtl::RefPtr<VmObject> clone_vmo;

    {
        // lookup the dispatcher from handle
        mxtl::RefPtr<VmObjectDispatcher> vmo;
        status = up->GetDispatcher(handle, &vmo);
        if (status != NO_ERROR)
            return status;

        // clone the vmo into a new one
        status = vmo->Clone(options, offset, size, &clone_vmo);
        if (status != NO_ERROR)
            return status;

        DEBUG_ASSERT(clone_vmo);
    }

    // create a Vm Object dispatcher
    mxtl::RefPtr<Dispatcher> dispatcher;
    mx_rights_t rights;
    mx_status_t result = VmObjectDispatcher::Create(mxtl::move(clone_vmo), &dispatcher, &rights);
    if (result != NO_ERROR)
        return result;

    // TODO: consider what rights this clone handle should start with
    // ie, should it get MAP if the original didn't?
    // based on if it's a private mapping, it should get WRITE.
    // what about EXECUTE?

    // create a handle and attach the dispatcher to it
    HandleOwner clone_handle(MakeHandle(mxtl::move(dispatcher), rights));
    if (!clone_handle)
        return ERR_NO_MEMORY;

    if (_out_handle.copy_to_user(up->MapHandleToValue(clone_handle)) != NO_ERROR)
        return ERR_INVALID_ARGS;

    up->AddHandle(mxtl::move(clone_handle));

    return NO_ERROR;
}
