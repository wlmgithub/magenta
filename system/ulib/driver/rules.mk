# Copyright 2016 The Fuchsia Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

LOCAL_DIR := $(GET_LOCAL_DIR)

MODULE := system/ulib/driver

MODULE_NAME := driver

MODULE_TYPE := userlib

MODULE_EXPORT := so
MODULE_SO_NAME := driver

MODULE_COMPILEFLAGS := -fvisibility=hidden

MODULE_SRCS := $(LOCAL_DIR)/driver-api.c

include make/module.mk
