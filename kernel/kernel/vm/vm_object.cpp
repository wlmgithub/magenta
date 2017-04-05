// Copyright 2016 The Fuchsia Authors
//
// Use of this source code is governed by a MIT-style
// license that can be found in the LICENSE file or at
// https://opensource.org/licenses/MIT

#include "kernel/vm/vm_object.h"

#include "vm_priv.h"

#include <assert.h>
#include <err.h>
#include <inttypes.h>
#include <kernel/auto_lock.h>
#include <kernel/vm.h>
#include <kernel/vm/vm_address_region.h>
#include <lib/console.h>
#include <mxtl/ref_ptr.h>
#include <new.h>
#include <stdlib.h>
#include <string.h>
#include <trace.h>

#define LOCAL_TRACE MAX(VM_GLOBAL_TRACE, 0)

VmObject::VmObject(mxtl::RefPtr<VmObject> parent)
    : lock_(parent ? parent->lock_ref() : _local_lock_),
    parent_(mxtl::move(parent)) {
    LTRACEF("%p\n", this);
}

VmObject::~VmObject() {
    canary_.Assert();
    LTRACEF("%p\n", this);

    // remove ourself from our parent (if present)
    if (parent_) {
        LTRACEF("removing ourself from our parent %p\n", parent_.get());
        parent_->RemoveChildLocked(this);
        parent_.reset();
    }

    DEBUG_ASSERT(mapping_list_.is_empty());
    DEBUG_ASSERT(children_list_.is_empty());
}

void VmObject::AddMappingLocked(VmMapping* r) TA_REQ(lock_) {
    canary_.Assert();
    mapping_list_.push_front(r);
}

void VmObject::RemoveMappingLocked(VmMapping* r) TA_REQ(lock_) {
    canary_.Assert();
    mapping_list_.erase(*r);
}

void VmObject::AddChildLocked(VmObject* o) TA_REQ(lock_) {
    canary_.Assert();
    children_list_.push_front(o);
}

void VmObject::RemoveChildLocked(VmObject* o) TA_REQ(lock_) {
    canary_.Assert();
    children_list_.erase(*o);
}

static int cmd_vm_object(int argc, const cmd_args* argv, uint32_t flags) {
    if (argc < 2) {
    notenoughargs:
        printf("not enough arguments\n");
    usage:
        printf("usage:\n");
        printf("%s dump <address>\n", argv[0].str);
        printf("%s dump_pages <address>\n", argv[0].str);
        return ERR_INTERNAL;
    }

    if (!strcmp(argv[1].str, "dump")) {
        if (argc < 2)
            goto notenoughargs;

        VmObject* o = reinterpret_cast<VmObject*>(argv[2].u);

        o->Dump(0, false);
    } else if (!strcmp(argv[1].str, "dump_pages")) {
        if (argc < 2)
            goto notenoughargs;

        VmObject* o = reinterpret_cast<VmObject*>(argv[2].u);

        o->Dump(0, true);
    } else {
        printf("unknown command\n");
        goto usage;
    }

    return NO_ERROR;
}

STATIC_COMMAND_START
#if LK_DEBUGLEVEL > 0
STATIC_COMMAND("vm_object", "vm object debug commands", &cmd_vm_object)
#endif
STATIC_COMMAND_END(vm_object);
