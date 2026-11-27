/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * Stand-in for the host's <linux/types.h>, for files built with system headers
 *
 * The Emscripten sysroot has no Linux UAPI headers, so without this the
 * sandbox files which use the C library would pick up U-Boot's own
 * linux/types.h, which conflicts with the C library's types.
 *
 * Copyright 2026 Simon Glass <sjg@chromium.org>
 */

#ifndef __WASM_LINUX_TYPES_H
#define __WASM_LINUX_TYPES_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>

typedef uint8_t __u8;
typedef uint16_t __u16;
typedef uint32_t __u32;
typedef uint64_t __u64;
typedef int8_t __s8;
typedef int16_t __s16;
typedef int32_t __s32;
typedef int64_t __s64;

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef unsigned long ulong;
typedef unsigned int uint;
typedef unsigned short ushort;
typedef unsigned char uchar;

#endif
