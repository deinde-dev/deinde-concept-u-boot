// SPDX-License-Identifier: GPL-2.0+
/*
 * Tests for the nboot command
 *
 * Copyright 2026 Simon Glass <sjg@chromium.org>
 */

#include <env.h>
#include <image.h>
#include <malloc.h>
#include <mapmem.h>
#include <u-boot/crc.h>
#include <test/cmd.h>
#include <test/ut.h>

/* Geometry of nand0, the smaller of the two emulated sandbox chips */
#define NAND0_ERASE	0x2000
#define NAND0_PAGE	0x200

/* Payload of the image, chosen so that the size is printed as 64 Bytes */
#define NB_DATA_LEN	0x40

/* Values recorded in the header, so that the test can check what is printed */
#define NB_LOAD		0x1000000
#define NB_ENTRY	0x1000004
#define NB_NAME		"nboot test image"

/* Block of nand0 which cmd_test_nboot_offset() loads from */
#define NB_OFFSET	0x2000

/* Partition table used by cmd_test_nboot_part(), with kernel at 1MB */
#define TEST_PARTS	"nand0:1m(boot),2m(kernel),-(rootfs)"
#define NB_PART_OFFSET	0x100000

/**
 * setup_nand() - Make nand0 current and drop any partition table
 *
 * The current device is a global which every sub-command reads, and the
 * mtdparts tests leave a table in the environment, so put both back to a
 * known state before each test.
 *
 * @uts: Test state
 * Return: 0 if OK, other value on error
 */
static int setup_nand(struct unit_test_state *uts)
{
	ut_assertok(env_set("mtdids", NULL));
	ut_assertok(env_set("mtdparts", NULL));
	ut_assertok(env_set("bootdevice", NULL));
	ut_assertok(run_command("nand device 0", 0));
	console_record_reset();

	return 0;
}

/**
 * make_image() - Build a page holding a valid legacy image
 *
 * The payload is a byte ramp, so that a change to any of it alters the CRC.
 * The rest of the page is zeroed, since that is what is written to the chip.
 *
 * Return: pointer to the page, or NULL if out of memory
 */
static void *make_image(void)
{
	struct legacy_img_hdr *hdr;
	u8 *data;
	int i;

	hdr = malloc(NAND0_PAGE);
	if (!hdr)
		return NULL;
	memset(hdr, '\0', NAND0_PAGE);

	data = (u8 *)(hdr + 1);
	for (i = 0; i < NB_DATA_LEN; i++)
		data[i] = i;

	image_set_magic(hdr, IH_MAGIC);
	image_set_time(hdr, 0);
	image_set_size(hdr, NB_DATA_LEN);
	image_set_load(hdr, NB_LOAD);
	image_set_ep(hdr, NB_ENTRY);
	image_set_dcrc(hdr, crc32(0, data, NB_DATA_LEN));
	image_set_os(hdr, IH_OS_LINUX);
	image_set_arch(hdr, IH_ARCH_SANDBOX);
	image_set_type(hdr, IH_TYPE_KERNEL);
	image_set_comp(hdr, IH_COMP_NONE);
	image_set_name(hdr, NB_NAME);

	/* the header CRC covers the header with its own CRC field left zero */
	image_set_hcrc(hdr, crc32(0, (u8 *)hdr, sizeof(*hdr)));

	return hdr;
}

/**
 * setup_test() - Set up the chip, an image and a buffer to load it into
 *
 * @uts: Test state
 * @imgp: Returns a page holding the image, to be freed by the caller
 * @loadp: Returns the buffer to load into, to be freed by the caller
 * @addrp: Returns the address of that buffer, as the command takes it
 * Return: 0 if OK, other value on error
 */
static int setup_test(struct unit_test_state *uts, void **imgp, void **loadp,
		      ulong *addrp)
{
	void *img, *load;

	ut_assertok(setup_nand(uts));

	img = make_image();
	ut_assertnonnull(img);
	load = malloc(NAND0_PAGE);
	ut_assertnonnull(load);
	memset(load, '\0', NAND0_PAGE);

	*imgp = img;
	*loadp = load;
	*addrp = map_to_sysmem(load);

	return 0;
}

/**
 * write_image() - Write an image to a block of nand0
 *
 * The block is erased first, since a page can only be written once, and the
 * progress reports of both operations are thrown away.
 *
 * @uts: Test state
 * @img: Page holding the image
 * @off: Offset of the block to write it to
 * Return: 0 if OK, other value on error
 */
static int write_image(struct unit_test_state *uts, void *img, uint off)
{
	ut_assertok(run_commandf("nand erase %x %x", off, NAND0_ERASE));
	ut_assertok(run_commandf("nand write %lx %x %x",
				 (ulong)map_to_sysmem(img), off, NAND0_PAGE));
	console_record_reset();

	return 0;
}

/**
 * erase_block() - Erase a block of nand0 and swallow the progress report
 *
 * An erased block holds nothing which looks like an image, whatever another
 * test has written there.
 *
 * @uts: Test state
 * @off: Offset of the block
 * Return: 0 if OK, other value on error
 */
static int erase_block(struct unit_test_state *uts, uint off)
{
	ut_assertok(run_commandf("nand erase %x %x", off, NAND0_ERASE));
	console_record_reset();

	return 0;
}

/**
 * check_load() - Check the report of a successful load
 *
 * @uts: Test state
 * @off: Offset the image was loaded from
 * Return: 0 if OK, other value on error
 */
static int check_load(struct unit_test_state *uts, uint off)
{
	ut_assert_nextline_empty();
	ut_assert_nextline("Loading from nand0, offset 0x%x", off);
	ut_assert_nextline("   Image Name:   " NB_NAME);
	if (IMAGE_ENABLE_TIMESTAMP)
		ut_assert_nextlinen("   Created:      ");
	ut_assert_nextline("   Image Type:   Sandbox Linux Kernel Image (uncompressed)");
	ut_assert_nextline("   Data Size:    64 Bytes = 64 Bytes");
	ut_assert_nextline("   Load Address: %08x", NB_LOAD);
	ut_assert_nextline("   Entry Point:  %08x", NB_ENTRY);
	ut_assert_console_end();

	return 0;
}

/**
 * check_usage() - Check the usage message
 *
 * @uts: Test state
 * Return: 0 if OK, other value on error
 */
static int check_usage(struct unit_test_state *uts)
{
	ut_assert_nextline("nboot - boot from NAND device");
	ut_assert_nextline_empty();
	ut_assert_nextline("Usage:");
	ut_assert_nextline("nboot [partition] | [[[loadAddr] dev] offset]");
	ut_assert_console_end();

	return 0;
}

/* Test loading an image from a chip given by number */
static int cmd_test_nboot_base(struct unit_test_state *uts)
{
	void *img, *load;
	ulong addr, old;

	ut_assertok(setup_test(uts, &img, &load, &addr));
	ut_assertok(write_image(uts, img, 0));

	old = image_load_addr;
	ut_assertok(run_commandf("nboot %lx 0", addr));
	ut_assertok(check_load(uts, 0));

	/* the image is in memory, header and payload both */
	ut_asserteq_mem(img, load, sizeof(struct legacy_img_hdr) +
			NB_DATA_LEN);

	/* and its address is the default for a command which needs one */
	ut_asserteq(addr, image_load_addr);
	image_load_addr = old;

	free(load);
	free(img);

	return 0;
}
CMD_TEST(cmd_test_nboot_base, UTF_CONSOLE);

/* Test the bootdevice variable, which names the chip when nothing else does */
static int cmd_test_nboot_device(struct unit_test_state *uts)
{
	void *img, *load;
	ulong addr, old;

	ut_assertok(setup_test(uts, &img, &load, &addr));
	ut_assertok(write_image(uts, img, 0));
	old = image_load_addr;

	/* with the variable unset there is nowhere to load from */
	ut_asserteq(1, run_commandf("nboot %lx", addr));
	ut_assert_nextline_empty();
	ut_assert_nextline("** No boot device **");
	ut_assert_console_end();

	/* a chip which is not there is refused before any transfer */
	ut_assertok(env_set("bootdevice", "9"));
	ut_asserteq(1, run_commandf("nboot %lx", addr));
	ut_assert_nextline_empty();
	ut_assert_nextline("** Device 9 not available");
	ut_assert_console_end();

	/* the variable is read as hex, so chip 0 is where this loads from */
	ut_assertok(env_set("bootdevice", "0"));
	ut_assertok(run_commandf("nboot %lx", addr));
	ut_assertok(check_load(uts, 0));

	image_load_addr = old;
	ut_assertok(env_set("bootdevice", NULL));
	free(load);
	free(img);

	return 0;
}
CMD_TEST(cmd_test_nboot_device, UTF_CONSOLE);

/* Test loading from an offset within the chip */
static int cmd_test_nboot_offset(struct unit_test_state *uts)
{
	void *img, *load;
	ulong addr, old;

	ut_assertok(setup_test(uts, &img, &load, &addr));
	ut_assertok(write_image(uts, img, NB_OFFSET));
	old = image_load_addr;

	ut_assertok(run_commandf("nboot %lx 0 %x", addr, NB_OFFSET));
	ut_assertok(check_load(uts, NB_OFFSET));

	/* the offset defaults to 0, so an erased block 0 is what is read */
	ut_assertok(erase_block(uts, 0));
	ut_asserteq(1, run_commandf("nboot %lx 0", addr));
	ut_assert_nextline_empty();
	ut_assert_nextline("Loading from nand0, offset 0x0");
	ut_assert_nextline("** Unknown image type");
	ut_assert_console_end();

	image_load_addr = old;
	free(load);
	free(img);

	return 0;
}
CMD_TEST(cmd_test_nboot_offset, UTF_CONSOLE);

/* Test naming an mtdparts partition instead of a chip and an offset */
static int cmd_test_nboot_part(struct unit_test_state *uts)
{
	void *img, *load;
	ulong addr, old;

	ut_assertok(setup_test(uts, &img, &load, &addr));
	ut_assertok(write_image(uts, img, NB_PART_OFFSET));
	ut_assertok(env_set("mtdids", "nand0=nand0"));
	ut_assertok(env_set("mtdparts", TEST_PARTS));
	old = image_load_addr;

	/* the partition supplies both the chip and the offset */
	ut_assertok(run_commandf("nboot %lx kernel", addr));
	ut_assertok(check_load(uts, NB_PART_OFFSET));

	/*
	 * A name which is not a partition is read as a chip number instead,
	 * and hextoul() makes that 0, so the load comes from the start of
	 * nand0 rather than being refused
	 */
	ut_assertok(erase_block(uts, 0));
	ut_asserteq(1, run_commandf("nboot %lx nosuchpart", addr));
	ut_assert_nextline("incorrect device type in nosuchpart");
	ut_assert_nextline_empty();
	ut_assert_nextline("Loading from nand0, offset 0x0");
	ut_assert_nextline("** Unknown image type");
	ut_assert_console_end();

	/* a partition name takes no further argument */
	ut_asserteq(1, run_commandf("nboot %lx kernel 0", addr));
	ut_assertok(check_usage(uts));

	image_load_addr = old;
	ut_assertok(setup_nand(uts));
	free(load);
	free(img);

	return 0;
}
CMD_TEST(cmd_test_nboot_part, UTF_CONSOLE);

/* Test the suffixes the command accepts and the ones it does not */
static int cmd_test_nboot_suffix(struct unit_test_state *uts)
{
	void *img, *load;
	ulong addr, old;

	ut_assertok(setup_test(uts, &img, &load, &addr));
	ut_assertok(write_image(uts, img, 0));
	old = image_load_addr;

	/* the three suffixes of 'nand read' are accepted and ignored */
	ut_assertok(run_commandf("nboot.jffs2 %lx 0", addr));
	ut_assertok(check_load(uts, 0));
	ut_assertok(run_commandf("nboot.e %lx 0", addr));
	ut_assertok(check_load(uts, 0));
	ut_assertok(run_commandf("nboot.i %lx 0", addr));
	ut_assertok(check_load(uts, 0));

	/* anything else is refused before the chip is read */
	ut_asserteq(1, run_commandf("nboot.foo %lx 0", addr));
	ut_assert_nextline("Unknown nand load suffix '.foo'");
	ut_assert_console_end();

	image_load_addr = old;
	free(load);
	free(img);

	return 0;
}
CMD_TEST(cmd_test_nboot_suffix, UTF_CONSOLE);

/* Test the arguments which produce the usage message */
static int cmd_test_nboot_usage(struct unit_test_state *uts)
{
	ut_assertok(setup_nand(uts));

	/* the command takes three arguments at most */
	ut_asserteq(1, run_command("nboot 1000 0 0 0", 0));
	ut_assertok(check_usage(uts));

	return 0;
}
CMD_TEST(cmd_test_nboot_usage, UTF_CONSOLE);
