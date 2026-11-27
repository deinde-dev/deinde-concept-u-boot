/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * Stand-in for the host's <asm/byteorder.h>, see linux/types.h here
 *
 * WebAssembly is always little-endian.
 *
 * Copyright 2026 Simon Glass <sjg@chromium.org>
 */

#ifndef __WASM_ASM_BYTEORDER_H
#define __WASM_ASM_BYTEORDER_H

#include <linux/types.h>

typedef __u16 __le16;
typedef __u32 __le32;
typedef __u64 __le64;
typedef __u16 __be16;
typedef __u32 __be32;
typedef __u64 __be64;

#define __cpu_to_le16(x)	((__le16)(x))
#define __cpu_to_le32(x)	((__le32)(x))
#define __cpu_to_le64(x)	((__le64)(x))
#define __le16_to_cpu(x)	((__u16)(x))
#define __le32_to_cpu(x)	((__u32)(x))
#define __le64_to_cpu(x)	((__u64)(x))
#define __cpu_to_be16(x)	((__be16)__builtin_bswap16(x))
#define __cpu_to_be32(x)	((__be32)__builtin_bswap32(x))
#define __cpu_to_be64(x)	((__be64)__builtin_bswap64(x))
#define __be16_to_cpu(x)	__builtin_bswap16(x)
#define __be32_to_cpu(x)	__builtin_bswap32(x)
#define __be64_to_cpu(x)	__builtin_bswap64(x)

#endif
