// SPDX-License-Identifier: GPL-2.0+
/*
 * Tests for the imxtract command
 *
 * Copyright 2026 Simon Glass <sjg@chromium.org>
 */

#include <command.h>
#include <console.h>
#include <env.h>
#include <errno.h>
#include <gzip.h>
#include <image.h>
#include <mapmem.h>
#include <u-boot/crc.h>
#include <asm/global_data.h>
#include <linux/kernel.h>
#include <linux/libfdt.h>
#include <test/cmd.h>
#include <test/ut.h>

DECLARE_GLOBAL_DATA_PTR;

/* where the image is built */
#define XIMG_ADDR	((ulong)CONFIG_SYS_LOAD_ADDR + 0x5000)

/* where a part is extracted to */
#define XIMG_DEST	((ulong)CONFIG_SYS_LOAD_ADDR + 0x6000)

/*
 * the destination as the legacy header and the FIT record it. Both fields are
 * 32 bits, so neither can hold XIMG_DEST on a board whose load address is
 * above 4GB, as qemu-arm-sbsa has. The command takes its destination from the
 * command line and reads neither field, so the low 32 bits do
 */
#define XIMG_HDR_DEST	lower_32_bits(XIMG_DEST)

/* space the image is built in, which is cleared first */
#define XIMG_SPACE	0x200

/* lengths of the two parts of the multi-file image */
#define XIMG_LEN0	6
#define XIMG_LEN1	4

/* length of part 0 rounded up to the four-byte boundary the next part is on */
#define XIMG_PAD0	8

/* the size table holds an entry per part and a zero terminator */
#define XIMG_TABLE	(3 * sizeof(u32))

/* payload length recorded in the header: the table and both parts */
#define XIMG_DATA_LEN	(XIMG_TABLE + XIMG_PAD0 + XIMG_LEN1)

/* addresses the two parts end up at */
#define XIMG_PART0	(XIMG_ADDR + sizeof(struct legacy_img_hdr) + XIMG_TABLE)
#define XIMG_PART1	(XIMG_PART0 + XIMG_PAD0)

/* first byte of part 1, which is a ramp like part 0 but starting higher */
#define XIMG_BASE1	0x80

/* payload compressed into the single part of the gzip image */
#define XIMG_GZ_LEN	0x40

/* space allowed for the compressed form of it */
#define XIMG_GZ_MAX	0x100

/* description and image name used for the FIT */
#define XIMG_FIT_SIZE	0x400
#define XIMG_FIT_DESC	"test FIT"
#define XIMG_FIT_IMAGE	"kernel"

/*
 * ximg_can_store() - check that the addresses the tests use reach memory
 *
 * The offset is CONFIG_SYS_LOAD_ADDR, which a board with no default load
 * address leaves at 0. There is then no address known to hold usable memory,
 * as on qemu-arm-sbsa, whose memory starts above 4GB.
 */
static bool ximg_can_store(void)
{
	return gd->ram_base <= XIMG_ADDR;
}

/**
 * setup_image() - Write a multi-file legacy image to memory
 *
 * The image holds two parts, each a byte ramp so that a change to any of it
 * alters the CRC. Part 0 is six bytes, so that the test can check that the
 * next part starts at the following four-byte boundary.
 *
 * @type: image type to record, normally IH_TYPE_MULTI
 * @comp: compression type to record, normally IH_COMP_NONE
 * Return: pointer to the image header
 */
static struct legacy_img_hdr *setup_image(u8 type, u8 comp)
{
	struct legacy_img_hdr *hdr;
	u32 *size;
	u8 *data;
	int i;

	hdr = map_sysmem(XIMG_ADDR, XIMG_SPACE);
	memset(hdr, '\0', XIMG_SPACE);

	/* the payload of a multi-file image starts with a table of sizes */
	size = (u32 *)(hdr + 1);
	size[0] = cpu_to_uimage(XIMG_LEN0);
	size[1] = cpu_to_uimage(XIMG_LEN1);
	size[2] = 0;

	data = (u8 *)&size[3];
	for (i = 0; i < XIMG_LEN0; i++)
		data[i] = i;
	for (i = 0; i < XIMG_LEN1; i++)
		data[XIMG_PAD0 + i] = XIMG_BASE1 + i;

	image_set_magic(hdr, IH_MAGIC);
	image_set_time(hdr, 0);
	image_set_size(hdr, XIMG_DATA_LEN);
	image_set_load(hdr, XIMG_HDR_DEST);
	image_set_ep(hdr, XIMG_HDR_DEST);
	image_set_dcrc(hdr, crc32(0, (u8 *)size, XIMG_DATA_LEN));
	image_set_os(hdr, IH_OS_LINUX);
	image_set_arch(hdr, IH_ARCH_SANDBOX);
	image_set_type(hdr, type);
	image_set_comp(hdr, comp);
	image_set_name(hdr, "test image");

	/* the header CRC covers the header with its own CRC field left zero */
	image_set_hcrc(hdr, crc32(0, (u8 *)hdr, sizeof(*hdr)));

	return hdr;
}

/**
 * check_part() - Check that a part arrived at the destination
 *
 * @uts: Test state
 * @base: first byte of the ramp the part holds
 * @len: length of the part
 * Return: 0 if OK, -ve on error
 */
static int check_part(struct unit_test_state *uts, u8 base, int len)
{
	u8 *dest;
	int i;

	dest = map_sysmem(XIMG_DEST, len);
	for (i = 0; i < len; i++)
		ut_asserteq(base + i, dest[i]);
	unmap_sysmem(dest);

	return 0;
}

/* Test 'imxtract' on the first part of a multi-file image */
static int cmd_test_imxtract_base(struct unit_test_state *uts)
{
	struct legacy_img_hdr *hdr;
	u8 *dest;

	if (!IS_ENABLED(CONFIG_LEGACY_IMAGE_FORMAT) || !ximg_can_store())
		return -EAGAIN;

	/*
	 * An absent 'verify' asks for verification, since env_get_yesno()
	 * reports -1 for a variable which is not set
	 */
	ut_assertok(env_set("verify", NULL));
	hdr = setup_image(IH_TYPE_MULTI, IH_COMP_NONE);
	dest = map_sysmem(XIMG_DEST, XIMG_LEN0);
	memset(dest, '\0', XIMG_LEN0);

	ut_assertok(run_commandf("imxtract %lx 0 %lx", XIMG_ADDR, XIMG_DEST));
	ut_assert_nextline("## Copying part 0 from legacy image at %08lx ...",
			   XIMG_ADDR);
	ut_assert_nextline("   Verifying Checksum ... OK");
	ut_assert_nextline("   Loading part 0 ... OK");
	ut_assert_console_end();

	ut_assertok(check_part(uts, 0, XIMG_LEN0));
	ut_asserteq_64(XIMG_PART0, hextoul(env_get("fileaddr"), NULL));
	ut_asserteq(XIMG_LEN0, hextoul(env_get("filesize"), NULL));

	unmap_sysmem(dest);
	unmap_sysmem(hdr);

	return 0;
}
CMD_TEST(cmd_test_imxtract_base, UTF_CONSOLE);

/* Test 'imxtract' on the second part, which the size table locates */
static int cmd_test_imxtract_part(struct unit_test_state *uts)
{
	struct legacy_img_hdr *hdr;
	u8 *dest;

	if (!IS_ENABLED(CONFIG_LEGACY_IMAGE_FORMAT) || !ximg_can_store())
		return -EAGAIN;

	ut_assertok(env_set("verify", "yes"));
	hdr = setup_image(IH_TYPE_MULTI, IH_COMP_NONE);
	dest = map_sysmem(XIMG_DEST, XIMG_LEN1);
	memset(dest, '\0', XIMG_LEN1);

	ut_assertok(run_commandf("imxtract %lx 1 %lx", XIMG_ADDR, XIMG_DEST));
	ut_assert_nextline("## Copying part 1 from legacy image at %08lx ...",
			   XIMG_ADDR);
	ut_assert_nextline("   Verifying Checksum ... OK");
	ut_assert_nextline("   Loading part 1 ... OK");
	ut_assert_console_end();

	ut_assertok(check_part(uts, XIMG_BASE1, XIMG_LEN1));

	/* part 0 is six bytes, so part 1 starts two bytes further on */
	ut_asserteq_64(XIMG_PART1, hextoul(env_get("fileaddr"), NULL));
	ut_asserteq(XIMG_LEN1, hextoul(env_get("filesize"), NULL));

	unmap_sysmem(dest);
	unmap_sysmem(hdr);

	return 0;
}
CMD_TEST(cmd_test_imxtract_part, UTF_CONSOLE);

/* Test 'imxtract' without a destination, which copies nothing */
static int cmd_test_imxtract_nodest(struct unit_test_state *uts)
{
	struct legacy_img_hdr *hdr;
	u8 *dest;

	if (!IS_ENABLED(CONFIG_LEGACY_IMAGE_FORMAT) || !ximg_can_store())
		return -EAGAIN;

	ut_assertok(env_set("verify", "yes"));
	hdr = setup_image(IH_TYPE_MULTI, IH_COMP_NONE);
	dest = map_sysmem(XIMG_DEST, XIMG_LEN0);
	memset(dest, '\0', XIMG_LEN0);

	ut_assertok(run_commandf("imxtract %lx 0", XIMG_ADDR));
	ut_assert_nextline("## Copying part 0 from legacy image at %08lx ...",
			   XIMG_ADDR);
	ut_assert_nextline("   Verifying Checksum ... OK");
	ut_assert_console_end();

	/* the part is only located, so the destination is untouched */
	ut_asserteq(0, dest[0]);
	ut_asserteq_64(XIMG_PART0, hextoul(env_get("fileaddr"), NULL));
	ut_asserteq(XIMG_LEN0, hextoul(env_get("filesize"), NULL));

	unmap_sysmem(dest);
	unmap_sysmem(hdr);

	return 0;
}
CMD_TEST(cmd_test_imxtract_nodest, UTF_CONSOLE);

/* Test 'imxtract' with no arguments, which uses the default load address */
static int cmd_test_imxtract_default(struct unit_test_state *uts)
{
	struct legacy_img_hdr *hdr;
	ulong old;

	if (!IS_ENABLED(CONFIG_LEGACY_IMAGE_FORMAT) || !ximg_can_store())
		return -EAGAIN;

	ut_assertok(env_set("verify", "yes"));
	hdr = setup_image(IH_TYPE_MULTI, IH_COMP_NONE);

	old = image_load_addr;
	image_load_addr = XIMG_ADDR;
	ut_assertok(run_command("imxtract", 0));
	image_load_addr = old;

	ut_assert_nextline("## Copying part 0 from legacy image at %08lx ...",
			   XIMG_ADDR);
	ut_assert_nextline("   Verifying Checksum ... OK");
	ut_assert_console_end();

	ut_asserteq_64(XIMG_PART0, hextoul(env_get("fileaddr"), NULL));

	unmap_sysmem(hdr);

	return 0;
}
CMD_TEST(cmd_test_imxtract_default, UTF_CONSOLE);

/* Test that 'verify' set to no skips the checksum */
static int cmd_test_imxtract_noverify(struct unit_test_state *uts)
{
	struct legacy_img_hdr *hdr;
	u8 *data;

	if (!IS_ENABLED(CONFIG_LEGACY_IMAGE_FORMAT) || !ximg_can_store())
		return -EAGAIN;

	ut_assertok(env_set("verify", "no"));
	hdr = setup_image(IH_TYPE_MULTI, IH_COMP_NONE);

	/* damage the payload, which is then not noticed */
	data = map_sysmem(XIMG_PART0, XIMG_LEN0);
	data[0] ^= 0xff;

	ut_assertok(run_commandf("imxtract %lx 0", XIMG_ADDR));
	ut_assert_nextline("## Copying part 0 from legacy image at %08lx ...",
			   XIMG_ADDR);
	ut_assert_console_end();

	unmap_sysmem(data);
	unmap_sysmem(hdr);

	return 0;
}
CMD_TEST(cmd_test_imxtract_noverify, UTF_CONSOLE);

/* Test 'imxtract' on an image whose payload does not match its CRC */
static int cmd_test_imxtract_dcrc(struct unit_test_state *uts)
{
	struct legacy_img_hdr *hdr;
	u8 *data;

	if (!IS_ENABLED(CONFIG_LEGACY_IMAGE_FORMAT) || !ximg_can_store())
		return -EAGAIN;

	ut_assertok(env_set("verify", "yes"));
	hdr = setup_image(IH_TYPE_MULTI, IH_COMP_NONE);

	/* the header is untouched, so the damage shows up in the payload */
	data = map_sysmem(XIMG_PART0, XIMG_LEN0);
	data[0] ^= 0xff;

	ut_asserteq(1, run_commandf("imxtract %lx 0", XIMG_ADDR));
	ut_assert_nextline("## Copying part 0 from legacy image at %08lx ...",
			   XIMG_ADDR);
	ut_assert_nextline("   Verifying Checksum ... Bad Data CRC");
	ut_assert_console_end();

	unmap_sysmem(data);
	unmap_sysmem(hdr);

	return 0;
}
CMD_TEST(cmd_test_imxtract_dcrc, UTF_CONSOLE);

/* Test 'imxtract' on an image whose header has been damaged */
static int cmd_test_imxtract_hcrc(struct unit_test_state *uts)
{
	struct legacy_img_hdr *hdr;

	if (!IS_ENABLED(CONFIG_LEGACY_IMAGE_FORMAT) || !ximg_can_store())
		return -EAGAIN;

	ut_assertok(env_set("verify", "yes"));
	hdr = setup_image(IH_TYPE_MULTI, IH_COMP_NONE);

	/* change a field without updating the header CRC */
	image_set_ep(hdr, XIMG_HDR_DEST + 1);

	ut_asserteq(1, run_commandf("imxtract %lx 0", XIMG_ADDR));
	ut_assert_nextline("## Copying part 0 from legacy image at %08lx ...",
			   XIMG_ADDR);
	ut_assert_nextline("Bad Header Checksum");
	ut_assert_console_end();

	unmap_sysmem(hdr);

	return 0;
}
CMD_TEST(cmd_test_imxtract_hcrc, UTF_CONSOLE);

/* Test 'imxtract' with a part number the image does not have */
static int cmd_test_imxtract_badpart(struct unit_test_state *uts)
{
	struct legacy_img_hdr *hdr;

	if (!IS_ENABLED(CONFIG_LEGACY_IMAGE_FORMAT) || !ximg_can_store())
		return -EAGAIN;

	ut_assertok(env_set("verify", "yes"));
	hdr = setup_image(IH_TYPE_MULTI, IH_COMP_NONE);

	/* the image holds parts 0 and 1 only */
	ut_asserteq(1, run_commandf("imxtract %lx 2", XIMG_ADDR));
	ut_assert_nextline("## Copying part 2 from legacy image at %08lx ...",
			   XIMG_ADDR);
	ut_assert_nextline("   Verifying Checksum ... OK");
	ut_assert_nextline("Bad Image Part");
	ut_assert_console_end();

	unmap_sysmem(hdr);

	return 0;
}
CMD_TEST(cmd_test_imxtract_badpart, UTF_CONSOLE);

/* Test 'imxtract' on a legacy image which holds a single component */
static int cmd_test_imxtract_type(struct unit_test_state *uts)
{
	struct legacy_img_hdr *hdr;

	if (!IS_ENABLED(CONFIG_LEGACY_IMAGE_FORMAT) || !ximg_can_store())
		return -EAGAIN;

	ut_assertok(env_set("verify", "yes"));
	hdr = setup_image(IH_TYPE_KERNEL, IH_COMP_NONE);

	ut_asserteq(1, run_commandf("imxtract %lx 0", XIMG_ADDR));
	ut_assert_nextline("## Copying part 0 from legacy image at %08lx ...",
			   XIMG_ADDR);
	ut_assert_nextline("Wrong Image Type for imxtract command");
	ut_assert_console_end();

	unmap_sysmem(hdr);

	return 0;
}
CMD_TEST(cmd_test_imxtract_type, UTF_CONSOLE);

/* Test that a compressed image needs a destination */
static int cmd_test_imxtract_comp(struct unit_test_state *uts)
{
	struct legacy_img_hdr *hdr;

	if (!IS_ENABLED(CONFIG_LEGACY_IMAGE_FORMAT) || !ximg_can_store())
		return -EAGAIN;

	ut_assertok(env_set("verify", "yes"));
	hdr = setup_image(IH_TYPE_MULTI, IH_COMP_GZIP);

	ut_asserteq(1, run_commandf("imxtract %lx 0", XIMG_ADDR));
	ut_assert_nextline("## Copying part 0 from legacy image at %08lx ...",
			   XIMG_ADDR);
	ut_assert_nextline("Must specify load address for imxtract command with compressed image");
	ut_assert_console_end();

	unmap_sysmem(hdr);

	return 0;
}
CMD_TEST(cmd_test_imxtract_comp, UTF_CONSOLE);

/* Test 'imxtract' on a part compressed in a way the command cannot handle */
static int cmd_test_imxtract_unimp(struct unit_test_state *uts)
{
	struct legacy_img_hdr *hdr;

	if (!IS_ENABLED(CONFIG_LEGACY_IMAGE_FORMAT) || !ximg_can_store())
		return -EAGAIN;

	ut_assertok(env_set("verify", "yes"));
	hdr = setup_image(IH_TYPE_MULTI, IH_COMP_LZMA);

	ut_asserteq(1, run_commandf("imxtract %lx 0 %lx", XIMG_ADDR,
				    XIMG_DEST));
	ut_assert_nextline("## Copying part 0 from legacy image at %08lx ...",
			   XIMG_ADDR);
	ut_assert_nextline("   Verifying Checksum ... OK");
	ut_assert_nextline("Unimplemented compression type %d", IH_COMP_LZMA);
	ut_assert_console_end();

	unmap_sysmem(hdr);

	return 0;
}
CMD_TEST(cmd_test_imxtract_unimp, UTF_CONSOLE);

/* Test 'imxtract' on a gzipped part, which is uncompressed as it is copied */
static int cmd_test_imxtract_gzip(struct unit_test_state *uts)
{
	struct legacy_img_hdr *hdr;
	u8 plain[XIMG_GZ_LEN];
	u8 comp[XIMG_GZ_MAX];
	ulong comp_len;
	uint data_len;
	u32 *size;
	u8 *dest;
	int i;

	/* gzip() comes with CONFIG_CMD_ZIP, which not every board has */
	if (!IS_ENABLED(CONFIG_LEGACY_IMAGE_FORMAT) ||
	    !IS_ENABLED(CONFIG_GZIP) || !IS_ENABLED(CONFIG_GZIP_COMPRESSED) ||
	    !ximg_can_store())
		return -EAGAIN;

	ut_assertok(env_set("verify", "yes"));
	for (i = 0; i < XIMG_GZ_LEN; i++)
		plain[i] = i;

	comp_len = sizeof(comp);
	ut_assertok(gzip(comp, &comp_len, plain, XIMG_GZ_LEN));

	/* build a single-part image whose part holds the compressed data */
	hdr = setup_image(IH_TYPE_MULTI, IH_COMP_GZIP);
	size = (u32 *)(hdr + 1);
	size[0] = cpu_to_uimage(comp_len);
	size[1] = 0;
	memcpy(&size[2], comp, comp_len);

	data_len = 2 * sizeof(u32) + comp_len;
	image_set_size(hdr, data_len);
	image_set_dcrc(hdr, crc32(0, (u8 *)size, data_len));
	image_set_hcrc(hdr, 0);
	image_set_hcrc(hdr, crc32(0, (u8 *)hdr, sizeof(*hdr)));

	dest = map_sysmem(XIMG_DEST, XIMG_GZ_LEN);
	memset(dest, '\0', XIMG_GZ_LEN);

	ut_assertok(run_commandf("imxtract %lx 0 %lx", XIMG_ADDR, XIMG_DEST));
	ut_assert_nextline("## Copying part 0 from legacy image at %08lx ...",
			   XIMG_ADDR);
	ut_assert_nextline("   Verifying Checksum ... OK");
	ut_assert_nextline("   Uncompressing part 0 ... OK");
	ut_assert_console_end();

	ut_asserteq_mem(plain, dest, XIMG_GZ_LEN);

	/* filesize is the uncompressed length, fileaddr the compressed data */
	ut_asserteq_64(XIMG_ADDR + sizeof(*hdr) + 2 * sizeof(u32),
		       hextoul(env_get("fileaddr"), NULL));
	ut_asserteq(XIMG_GZ_LEN, hextoul(env_get("filesize"), NULL));

	unmap_sysmem(dest);
	unmap_sysmem(hdr);

	return 0;
}
CMD_TEST(cmd_test_imxtract_gzip, UTF_CONSOLE);

/* Test 'imxtract' on memory which holds no image at all */
static int cmd_test_imxtract_invalid(struct unit_test_state *uts)
{
	void *ptr;

	if (!ximg_can_store())
		return -EAGAIN;

	ptr = map_sysmem(XIMG_ADDR, XIMG_SPACE);
	memset(ptr, '\0', XIMG_SPACE);

	ut_asserteq(1, run_commandf("imxtract %lx 0", XIMG_ADDR));
	ut_assert_nextline("Invalid image type for imxtract");
	ut_assert_console_end();

	unmap_sysmem(ptr);

	return 0;
}
CMD_TEST(cmd_test_imxtract_invalid, UTF_CONSOLE);

/**
 * setup_fit() - Write a small FIT to memory
 *
 * The FIT holds a single kernel image with a payload of XIMG_GZ_LEN bytes and
 * a crc32 hash over it. It is built here with libfdt, so that the test needs
 * no image file and no mkimage
 *
 * @uts: Test state
 * @corrupt: true to damage the payload after the hash is recorded, so that
 *	verification fails
 * Return: 0 if OK, -ve on error
 */
static int setup_fit(struct unit_test_state *uts, bool corrupt)
{
	u8 data[XIMG_GZ_LEN];
	int images, node, hash;
	void *fit;
	int i;

	for (i = 0; i < XIMG_GZ_LEN; i++)
		data[i] = i;

	fit = map_sysmem(XIMG_ADDR, XIMG_FIT_SIZE);
	ut_assertok(fdt_create_empty_tree(fit, XIMG_FIT_SIZE));
	ut_assertok(fdt_setprop_string(fit, 0, FIT_DESC_PROP, XIMG_FIT_DESC));
	ut_assertok(fdt_setprop_u32(fit, 0, FIT_TIMESTAMP_PROP, 0));

	images = fdt_add_subnode(fit, 0, "images");
	ut_assert(images > 0);

	node = fdt_add_subnode(fit, images, XIMG_FIT_IMAGE);
	ut_assert(node > 0);
	ut_assertok(fdt_setprop(fit, node, FIT_DATA_PROP, data, XIMG_GZ_LEN));
	ut_assertok(fdt_setprop_string(fit, node, FIT_TYPE_PROP, "kernel"));
	ut_assertok(fdt_setprop_string(fit, node, FIT_ARCH_PROP, "sandbox"));
	ut_assertok(fdt_setprop_string(fit, node, FIT_OS_PROP, "linux"));
	ut_assertok(fdt_setprop_string(fit, node, FIT_COMP_PROP, "none"));
	ut_assertok(fdt_setprop_u32(fit, node, FIT_LOAD_PROP, XIMG_HDR_DEST));

	hash = fdt_add_subnode(fit, node, FIT_HASH_NODENAME "-1");
	ut_assert(hash > 0);
	ut_assertok(fdt_setprop_string(fit, hash, FIT_ALGO_PROP, "crc32"));
	ut_assertok(fdt_setprop_u32(fit, hash, FIT_VALUE_PROP,
				    crc32(0, data, XIMG_GZ_LEN)));

	if (corrupt) {
		u8 *ptr;
		int len;

		ptr = (u8 *)fdt_getprop_w(fit, node, FIT_DATA_PROP, &len);
		ut_assertnonnull(ptr);
		ut_asserteq(XIMG_GZ_LEN, len);
		ptr[0] ^= 0xff;
	}

	ut_assertok(fdt_pack(fit));
	unmap_sysmem(fit);

	return 0;
}

/* Test 'imxtract' on a subimage of a FIT */
static int cmd_test_imxtract_fit(struct unit_test_state *uts)
{
	u8 *dest;

	if (!IS_ENABLED(CONFIG_FIT) || !ximg_can_store())
		return -EAGAIN;

	ut_assertok(env_set("verify", "yes"));
	ut_assertok(setup_fit(uts, false));
	dest = map_sysmem(XIMG_DEST, XIMG_GZ_LEN);
	memset(dest, '\0', XIMG_GZ_LEN);

	ut_assertok(run_commandf("imxtract %lx %s %lx", XIMG_ADDR,
				 XIMG_FIT_IMAGE, XIMG_DEST));
	ut_assert_nextline("## Copying '%s' subimage from FIT image at %08lx ...",
			   XIMG_FIT_IMAGE, XIMG_ADDR);

	/* the hash result and the copy report land on the same line */
	ut_assert_nextline("crc32+    Loading part 0 ... OK");
	ut_assert_console_end();

	ut_assertok(check_part(uts, 0, XIMG_GZ_LEN));
	ut_asserteq(XIMG_GZ_LEN, hextoul(env_get("filesize"), NULL));

	unmap_sysmem(dest);

	return 0;
}
CMD_TEST(cmd_test_imxtract_fit, UTF_CONSOLE);

/* Test 'imxtract' on a FIT without naming a subimage */
static int cmd_test_imxtract_fit_noname(struct unit_test_state *uts)
{
	if (!IS_ENABLED(CONFIG_FIT) || !ximg_can_store())
		return -EAGAIN;

	ut_assertok(env_set("verify", "yes"));
	ut_assertok(setup_fit(uts, false));

	ut_asserteq(1, run_commandf("imxtract %lx", XIMG_ADDR));
	ut_assert_nextline("No FIT subimage unit name");
	ut_assert_console_end();

	return 0;
}
CMD_TEST(cmd_test_imxtract_fit_noname, UTF_CONSOLE);

/* Test 'imxtract' with the name of a subimage the FIT does not hold */
static int cmd_test_imxtract_fit_missing(struct unit_test_state *uts)
{
	if (!IS_ENABLED(CONFIG_FIT) || !ximg_can_store())
		return -EAGAIN;

	ut_assertok(env_set("verify", "yes"));
	ut_assertok(setup_fit(uts, false));

	ut_asserteq(1, run_commandf("imxtract %lx ramdisk", XIMG_ADDR));
	ut_assert_nextline("## Copying 'ramdisk' subimage from FIT image at %08lx ...",
			   XIMG_ADDR);
	ut_assert_nextline("Can't find 'ramdisk' FIT subimage");
	ut_assert_console_end();

	return 0;
}
CMD_TEST(cmd_test_imxtract_fit_missing, UTF_CONSOLE);

/* Test 'imxtract' on a FIT subimage which does not match its hash */
static int cmd_test_imxtract_fit_hash(struct unit_test_state *uts)
{
	if (!IS_ENABLED(CONFIG_FIT) || !ximg_can_store())
		return -EAGAIN;

	ut_assertok(env_set("verify", "yes"));
	ut_assertok(setup_fit(uts, true));

	ut_asserteq(1, run_commandf("imxtract %lx %s", XIMG_ADDR,
				    XIMG_FIT_IMAGE));
	ut_assert_nextline("## Copying '%s' subimage from FIT image at %08lx ...",
			   XIMG_FIT_IMAGE, XIMG_ADDR);
	ut_assert_nextline("crc32 error!");
	ut_assert_nextline("Bad hash value for 'hash-1' hash node in '%s' image node",
			   XIMG_FIT_IMAGE);
	ut_assert_nextline("Bad Data Hash");
	ut_assert_console_end();

	return 0;
}
CMD_TEST(cmd_test_imxtract_fit_hash, UTF_CONSOLE);
