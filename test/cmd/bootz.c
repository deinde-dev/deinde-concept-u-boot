// SPDX-License-Identifier: GPL-2.0+
/*
 * Tests for the bootz command
 *
 * Copyright 2026 Simon Glass <sjg@chromium.org>
 */

#include <command.h>
#include <errno.h>
#include <image.h>
#include <mapmem.h>
#include <asm/unaligned.h>
#include <test/cmd.h>
#include <test/ut.h>

/* Magic which marks a Linux ARM zImage */
#define ARM_ZIMAGE_MAGIC	0x016f2818

/* Offsets in an x86 zImage of its signature, setup-sector count and size */
#define X86_HDR_OFF		0x202
#define X86_SETUP_SECTS_OFF	0x1f1
#define X86_SYSSIZE_OFF		0x1f4

/* Values the x86 header carries, and the length which follows from them */
#define X86_SETUP_SECTS		1
#define X86_SYSSIZE		0x20
#define X86_ZI_LEN		((X86_SETUP_SECTS + 1) * 512 + X86_SYSSIZE * 16)

/* Size of the image, enough for either header */
#define IMAGE_SIZE		0x400

/*
 * Where the image is built. The command reserves the image where it finds it,
 * so this must be in the region the board keeps for loading images rather
 * than on the heap
 */
#define IMAGE_ADDR		((ulong)CONFIG_SYS_LOAD_ADDR + 0x100000)

/**
 * struct arm_z_header - The part of an ARM zImage header which bootz reads
 *
 * @code: Instructions which a real zImage starts with
 * @zi_magic: Magic number marking the image as a zImage
 * @zi_start: Offset of the start of the image
 * @zi_end: Offset of the end of the image
 */
struct arm_z_header {
	u32 code[9];
	u32 zi_magic;
	u32 zi_start;
	u32 zi_end;
};

/*
 * The two tests which boot ask for a driver-model state of their own. The
 * boot measures the Image, which needs a TPM that has come up in the state
 * the test is running in; one left behind by an earlier UTF_DM test crashes
 * in tcg2_measurement_init(). See the note in test_pre_run() about TCG
 * measurement needing a proper init.
 */
#define BOOT_FLAGS	(UTF_CONSOLE | UTF_DM | UTF_SCAN_FDT)

/* Test booting an ARM zImage, which is as far as sandbox can take it */
static int cmd_test_bootz_base(struct unit_test_state *uts)
{
	struct arm_z_header *hdr;

	hdr = map_sysmem(IMAGE_ADDR, IMAGE_SIZE);
	memset(hdr, '\0', IMAGE_SIZE);
	hdr->zi_magic = cpu_to_le32(ARM_ZIMAGE_MAGIC);
	hdr->zi_end = cpu_to_le32(IMAGE_SIZE);

	/*
	 * The image has no device tree, which a Linux kernel may do without.
	 * Sandbox returns from the handover instead of starting the kernel, so
	 * the command comes back and reports success
	 */
	ut_assertok(run_commandf("bootz %lx", IMAGE_ADDR));
	ut_assert_nextline("setting up ARM zImage [ 0 - %d ]", IMAGE_SIZE);
	ut_assert_skip_to_line("sandbox: continuing, as we cannot run Linux");
	ut_assert_console_end();

	unmap_sysmem(hdr);

	return 0;
}
CMD_TEST(cmd_test_bootz_base, BOOT_FLAGS);

/* Test booting an x86 zImage, the other format sandbox recognises */
static int cmd_test_bootz_x86(struct unit_test_state *uts)
{
	u8 *buf;

	buf = map_sysmem(IMAGE_ADDR, IMAGE_SIZE);
	memset(buf, '\0', IMAGE_SIZE);
	memcpy(buf + X86_HDR_OFF, "HdrS", 4);
	buf[X86_SETUP_SECTS_OFF] = X86_SETUP_SECTS;
	put_unaligned_le32(X86_SYSSIZE, buf + X86_SYSSIZE_OFF);

	/* the length comes from the setup sectors and the size of the kernel */
	ut_assertok(run_commandf("bootz %lx", IMAGE_ADDR));
	ut_assert_nextline("setting up X86 zImage [ 0 - %d ]", X86_ZI_LEN);
	ut_assert_skip_to_line("sandbox: continuing, as we cannot run Linux");
	ut_assert_console_end();

	unmap_sysmem(buf);

	return 0;
}
CMD_TEST(cmd_test_bootz_x86, BOOT_FLAGS);

/* Test that the command refuses an option rather than taking it as an address */
static int cmd_test_bootz_usage(struct unit_test_state *uts)
{
	ut_asserteq(1, run_command("bootz -x", 0));
	ut_assert_nextline("bootz - boot Linux zImage image from memory");
	ut_assert_nextline_empty();

	/* the rest of the message is there only where the long help is */
	if (IS_ENABLED(CONFIG_SYS_LONGHELP)) {
		ut_assert_nextline("Usage:");
		ut_assert_nextline("bootz [addr [initrd[:size]] [fdt]]");
	}

	/* the remaining lines of the help text depend on the configuration */
	console_record_reset();

	return 0;
}
CMD_TEST(cmd_test_bootz_usage, UTF_CONSOLE);

/* Test that the command refuses data which is not a zImage */
static int cmd_test_bootz_bad(struct unit_test_state *uts)
{
	u8 *buf;

	buf = map_sysmem(IMAGE_ADDR, IMAGE_SIZE);
	memset(buf, '\0', IMAGE_SIZE);

	/* neither header is there, so there is nothing to boot */
	ut_asserteq(1, run_commandf("bootz %lx", IMAGE_ADDR));
	ut_assert_nextline("Unrecognized zImage");
	ut_assert_console_end();

	unmap_sysmem(buf);

	return 0;
}
CMD_TEST(cmd_test_bootz_bad, UTF_CONSOLE);
