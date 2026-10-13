// SPDX-License-Identifier: GPL-2.0+
/*
 * Tests for booting legacy images with bootm
 *
 * Copyright 2026 Simon Glass <sjg@chromium.org>
 */

#include <command.h>
#include <image.h>
#include <malloc.h>
#include <mapmem.h>
#include <u-boot/crc.h>
#include <test/test.h>
#include <test/ut.h>

#define BOOTM_TEST(_name, _flags)	UNIT_TEST(_name, _flags, bootm)

/* where the kernel is loaded and entered, as recorded in the header */
#define KERN_LOAD	0x1000000
#define KERN_ENTRY	(KERN_LOAD + 4)

/* what bootm prints on finding the ramdisk in a multi-component image */
#define RD_MSG \
	"## Loading init Ramdisk from multi component Legacy Image at %08lx ..."

/* sizes of the kernel and the ramdisk, both multiples of four */
#define KERN_LEN	0x40
#define RD_LEN		0x20

/**
 * fill() - Put a pattern in a buffer, so that a copy of it can be checked
 *
 * @buf: Buffer to fill
 * @len: Number of bytes
 * @seed: Value of the first byte, so two buffers can differ
 */
static void fill(u8 *buf, uint len, u8 seed)
{
	uint i;

	for (i = 0; i < len; i++)
		buf[i] = seed + i;
}

/**
 * make_image() - Build a legacy image with a header for sandbox
 *
 * @type: Image type, IH_TYPE_KERNEL or IH_TYPE_MULTI
 * @data: Data which follows the header
 * @len: Length of @data
 * Return: the image, which the caller frees, or NULL if out of memory
 */
static struct legacy_img_hdr *make_image(int type, const void *data, uint len)
{
	struct legacy_img_hdr *hdr;

	hdr = calloc(1, sizeof(*hdr) + len);
	if (!hdr)
		return NULL;
	memcpy(hdr + 1, data, len);

	image_set_magic(hdr, IH_MAGIC);
	image_set_size(hdr, len);
	image_set_load(hdr, KERN_LOAD);
	image_set_ep(hdr, KERN_ENTRY);
	image_set_dcrc(hdr, crc32(0, data, len));
	image_set_os(hdr, IH_OS_LINUX);
	image_set_arch(hdr, IH_ARCH_SANDBOX);
	image_set_type(hdr, type);
	image_set_comp(hdr, IH_COMP_NONE);
	image_set_name(hdr, "legacy test");

	/* the header CRC covers the header with its own CRC field left zero */
	image_set_hcrc(hdr, crc32(0, (u8 *)hdr, sizeof(*hdr)));

	return hdr;
}

/* Test booting a legacy kernel image */
static int bootm_test_legacy_kernel(struct unit_test_state *uts)
{
	struct legacy_img_hdr *hdr;
	u8 kern[KERN_LEN], *load;
	ulong addr;

	if (!IS_ENABLED(CONFIG_CMD_BOOTM) ||
	    !IS_ENABLED(CONFIG_LEGACY_IMAGE_FORMAT))
		return -EAGAIN;

	fill(kern, KERN_LEN, 0x10);
	hdr = make_image(IH_TYPE_KERNEL, kern, KERN_LEN);
	ut_assertnonnull(hdr);
	addr = map_to_sysmem(hdr);

	load = map_sysmem(KERN_LOAD, KERN_LEN);
	memset(load, '\0', KERN_LEN);

	/* the image has no device tree, which a Linux kernel may do without */
	ut_assertok(run_commandf("bootm %lx", addr));
	ut_assert_skip_to_line("## Booting kernel from Legacy Image at %08lx ...",
			       addr);
	ut_assert_skip_to_line("sandbox: continuing, as we cannot run Linux");
	console_record_reset();

	/* the kernel was copied from the image to its load address */
	ut_asserteq_mem(kern, load, KERN_LEN);

	unmap_sysmem(load);
	free(hdr);

	return 0;
}
BOOTM_TEST(bootm_test_legacy_kernel, UTF_CONSOLE);

/* Test that bootm finds the parts of a multi-component legacy image */
static int bootm_test_legacy_multi(struct unit_test_state *uts)
{
	/* table of the two component sizes and a zero to end it, then both */
	struct {
		__be32 sizes[3];
		u8 kern[KERN_LEN];
		u8 rd[RD_LEN];
	} data;
	struct legacy_img_hdr *hdr;
	ulong addr, base;

	if (!IS_ENABLED(CONFIG_CMD_BOOTM) ||
	    !IS_ENABLED(CONFIG_LEGACY_IMAGE_FORMAT))
		return -EAGAIN;

	data.sizes[0] = cpu_to_be32(KERN_LEN);
	data.sizes[1] = cpu_to_be32(RD_LEN);
	data.sizes[2] = 0;
	fill(data.kern, KERN_LEN, 0x10);
	fill(data.rd, RD_LEN, 0x80);
	hdr = make_image(IH_TYPE_MULTI, &data, sizeof(data));
	ut_assertnonnull(hdr);
	addr = map_to_sysmem(hdr);

	/* find the kernel and the ramdisk, without loading or booting */
	ut_assertok(run_commandf("bootm start %lx", addr));
	ut_assert_skip_to_line("## Booting kernel from Legacy Image at %08lx ...",
			       addr);
	ut_assert_skip_to_line(RD_MSG, addr);
	console_record_reset();

	/* both parts are given as U-Boot addresses inside the image */
	base = addr + sizeof(*hdr) + sizeof(data.sizes);
	ut_asserteq(base, images.os.image_start);
	ut_asserteq(KERN_LEN, images.os.image_len);
	ut_asserteq(base + KERN_LEN, images.rd_start);
	ut_asserteq(base + KERN_LEN + RD_LEN, images.rd_end);

	free(hdr);

	return 0;
}
BOOTM_TEST(bootm_test_legacy_multi, UTF_CONSOLE);
