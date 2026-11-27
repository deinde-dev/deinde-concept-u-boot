// SPDX-License-Identifier: GPL-2.0+
/*
 * Tests for the bootm command
 *
 * Copyright 2026 Simon Glass <sjg@chromium.org>
 */

#include <command.h>
#include <env.h>
#include <errno.h>
#include <image.h>
#include <mapmem.h>
#include <u-boot/crc.h>
#include <test/cmd.h>
#include <test/ut.h>

/* Size of the kernel payload, a multiple of four */
#define KERN_LEN	0x40

/* Name recorded in the image header, so the test can check what is shown */
#define IMAGE_NAME	"bootm test"

/*
 * Where the image is built and where its header asks for the kernel to go.
 * The command reserves the image where it finds it, so both must be in the
 * region the board keeps for loading images rather than on the heap, and
 * clear of the memory which the test devicetree reserves at 1MB
 */
#define IMAGE_ADDR	((ulong)CONFIG_SYS_LOAD_ADDR + 0x200000)
#define KERN_LOAD	((ulong)CONFIG_SYS_LOAD_ADDR + 0x300000)

/*
 * The tests which boot ask for a driver-model state of their own. The boot
 * measures the image, which needs a TPM that has come up in the state the test
 * is running in; one left behind by an earlier UTF_DM test crashes in
 * tcg2_measurement_init(). See the note in test_pre_run() about TCG
 * measurement needing a proper init.
 */
#define BOOT_FLAGS	(UTF_CONSOLE | UTF_DM | UTF_SCAN_FDT)

/**
 * setup_image() - Write a legacy kernel image to memory
 *
 * The payload is a byte ramp, so that the copy made by the loados state can be
 * checked against it
 *
 * Return: the image, which the caller unmaps
 */
static struct legacy_img_hdr *setup_image(void)
{
	struct legacy_img_hdr *hdr;
	u8 *data;
	int i;

	hdr = map_sysmem(IMAGE_ADDR, sizeof(*hdr) + KERN_LEN);
	memset(hdr, '\0', sizeof(*hdr) + KERN_LEN);

	data = (u8 *)(hdr + 1);
	for (i = 0; i < KERN_LEN; i++)
		data[i] = i;

	image_set_magic(hdr, IH_MAGIC);
	image_set_time(hdr, 0);
	image_set_size(hdr, KERN_LEN);
	image_set_load(hdr, KERN_LOAD);
	image_set_ep(hdr, KERN_LOAD + 4);
	image_set_dcrc(hdr, crc32(0, data, KERN_LEN));
	image_set_os(hdr, IH_OS_LINUX);
	image_set_arch(hdr, IH_ARCH_SANDBOX);
	image_set_type(hdr, IH_TYPE_KERNEL);
	image_set_comp(hdr, IH_COMP_NONE);
	image_set_name(hdr, IMAGE_NAME);

	/* the header CRC covers the header with its own CRC field left zero */
	image_set_hcrc(hdr, crc32(0, (u8 *)hdr, sizeof(*hdr)));

	return hdr;
}

/**
 * check_header() - Check the image information which the start state shows
 *
 * The command logs a line about the pre-load signature check before this, so
 * the caller skips to the first line rather than asserting on it
 *
 * @uts: Test state
 * Return: 0 if OK, -ve on error
 */
static int check_header(struct unit_test_state *uts)
{
	ut_assert_skip_to_line("## Booting kernel from Legacy Image at %08lx ...",
			       IMAGE_ADDR);
	ut_assert_nextline("   Image Name:   " IMAGE_NAME);
	if (IMAGE_ENABLE_TIMESTAMP)
		ut_assert_nextlinen("   Created:      ");
	ut_assert_nextline("   Image Type:   Sandbox Linux Kernel Image (uncompressed)");
	ut_assert_nextline("   Data Size:    64 Bytes = 64 Bytes");
	ut_assert_nextline("   Load Address: %08lx", KERN_LOAD);
	ut_assert_nextline("   Entry Point:  %08lx", KERN_LOAD + 4);
	ut_assert_nextline("   Verifying Checksum ... OK");

	return 0;
}

/**
 * check_boot() - Check the output of the go state, which starts the kernel
 *
 * The bootstage report and the measurement messages come between the two
 * lines asserted here, so this skips rather than reading line by line
 *
 * @uts: Test state
 * Return: 0 if OK, -ve on error
 */
static int check_boot(struct unit_test_state *uts)
{
	ut_assert_skip_to_line("Starting kernel ...");
	ut_assert_skip_to_line("## Transferring control to Linux (at address %08lx)...",
			       KERN_LOAD + 4);
	ut_assert_nextline("sandbox: continuing, as we cannot run Linux");
	ut_assert_console_end();

	return 0;
}

/* Test booting a legacy kernel image, which is as far as sandbox can take it */
static int cmd_test_bootm_base(struct unit_test_state *uts)
{
	struct legacy_img_hdr *hdr;
	u8 *load;

	if (!IS_ENABLED(CONFIG_LEGACY_IMAGE_FORMAT))
		return -EAGAIN;

	hdr = setup_image();
	load = map_sysmem(KERN_LOAD, KERN_LEN);
	memset(load, '\0', KERN_LEN);

	/* the image has no device tree, which a Linux kernel may do without */
	ut_assertok(run_commandf("bootm %lx", IMAGE_ADDR));
	ut_assertok(check_header(uts));

	/* the loados state copies the kernel out of the image */
	ut_assert_skip_to_line("   Loading Kernel Image to %lx", KERN_LOAD);
	ut_assertok(check_boot(uts));

	ut_asserteq_mem(hdr + 1, load, KERN_LEN);

	unmap_sysmem(load);
	unmap_sysmem(hdr);

	return 0;
}
CMD_TEST(cmd_test_bootm_base, BOOT_FLAGS);

/* Test that a boot with no address uses the current image address */
static int cmd_test_bootm_default(struct unit_test_state *uts)
{
	struct legacy_img_hdr *hdr;
	ulong old;

	if (!IS_ENABLED(CONFIG_LEGACY_IMAGE_FORMAT))
		return -EAGAIN;

	hdr = setup_image();

	old = image_load_addr;
	image_load_addr = IMAGE_ADDR;
	ut_assertok(run_command("bootm", 0));
	image_load_addr = old;

	ut_assertok(check_header(uts));
	ut_assert_skip_to_line("   Loading Kernel Image to %lx", KERN_LOAD);
	ut_assertok(check_boot(uts));

	unmap_sysmem(hdr);

	return 0;
}
CMD_TEST(cmd_test_bootm_default, BOOT_FLAGS);

/* Test that a hyphen in place of the ramdisk leaves the kernel on its own */
static int cmd_test_bootm_no_ramdisk(struct unit_test_state *uts)
{
	struct legacy_img_hdr *hdr;

	if (!IS_ENABLED(CONFIG_LEGACY_IMAGE_FORMAT))
		return -EAGAIN;

	hdr = setup_image();

	ut_assertok(run_commandf("bootm %lx - ", IMAGE_ADDR));
	ut_assertok(check_header(uts));
	ut_assert_skip_to_line("   Loading Kernel Image to %lx", KERN_LOAD);
	ut_assertok(check_boot(uts));

	unmap_sysmem(hdr);

	return 0;
}
CMD_TEST(cmd_test_bootm_no_ramdisk, BOOT_FLAGS);

/* Test running the boot one state at a time */
static int cmd_test_bootm_states(struct unit_test_state *uts)
{
	struct legacy_img_hdr *hdr;
	u8 *load;

	if (!IS_ENABLED(CONFIG_LEGACY_IMAGE_FORMAT))
		return -EAGAIN;

	hdr = setup_image();
	load = map_sysmem(KERN_LOAD, KERN_LEN);
	memset(load, '\0', KERN_LEN);

	/* the start state records the image but loads nothing */
	ut_assertok(run_commandf("bootm start %lx", IMAGE_ADDR));
	ut_assertok(check_header(uts));
	ut_assert_console_end();
	ut_asserteq(IMAGE_ADDR + sizeof(*hdr), images.os.image_start);
	ut_asserteq(KERN_LEN, images.os.image_len);

	/* now the kernel is copied to where its header asks for it */
	ut_assertok(run_command("bootm loados", 0));
	ut_assert_nextline("   Loading Kernel Image to %lx", KERN_LOAD);
	ut_assert_console_end();
	ut_asserteq_mem(hdr + 1, load, KERN_LEN);

	/* these states set things up for the OS and show nothing */
	ut_assertok(run_command("bootm cmdline", 0));
	ut_assertok(run_command("bootm bdt", 0));
	ut_assertok(run_command("bootm prep", 0));
	ut_assert_console_end();

	ut_assertok(run_command("bootm go", 0));
	ut_assertok(check_boot(uts));

	unmap_sysmem(load);
	unmap_sysmem(hdr);

	return 0;
}
CMD_TEST(cmd_test_bootm_states, BOOT_FLAGS);

/* Test that a state cannot be run twice, since the states have an order */
static int cmd_test_bootm_order(struct unit_test_state *uts)
{
	struct legacy_img_hdr *hdr;

	if (!IS_ENABLED(CONFIG_LEGACY_IMAGE_FORMAT))
		return -EAGAIN;

	hdr = setup_image();

	ut_assertok(run_commandf("bootm start %lx", IMAGE_ADDR));
	ut_assertok(run_command("bootm loados", 0));
	console_record_reset();

	/* the second loados asks for a state which the boot has passed */
	ut_asserteq(1, run_command("bootm loados", 0));
	ut_assert_nextline("Trying to execute a command out of order");
	ut_assert_nextline("bootm - boot application image from memory");
	ut_assert_nextline_empty();

	/* the rest of the message is there only where the long help is */
	if (IS_ENABLED(CONFIG_SYS_LONGHELP)) {
		ut_assert_nextline("Usage:");
		ut_assert_nextline("bootm [addr [arg ...]]");
	}

	/* the remaining lines of the help text depend on the configuration */
	console_record_reset();

	/* leave the state where the next test expects to find it */
	images.state = 0;

	unmap_sysmem(hdr);

	return 0;
}
CMD_TEST(cmd_test_bootm_order, UTF_CONSOLE);

/* Test that an unknown sub-command shows the usage message */
static int cmd_test_bootm_unknown(struct unit_test_state *uts)
{
	/*
	 * anything which is not a number and not a FIT specification is taken
	 * as a sub-command, so this reaches neither the boot nor an address
	 */
	ut_asserteq(1, run_command("bootm nosuchstate", 0));
	ut_assert_nextline("bootm - boot application image from memory");
	ut_assert_nextline_empty();

	if (IS_ENABLED(CONFIG_SYS_LONGHELP)) {
		ut_assert_nextline("Usage:");
		ut_assert_nextline("bootm [addr [arg ...]]");
	}

	console_record_reset();

	return 0;
}
CMD_TEST(cmd_test_bootm_unknown, UTF_CONSOLE);

/* Test that the preload state records the address it has checked */
static int cmd_test_bootm_preload(struct unit_test_state *uts)
{
	struct legacy_img_hdr *hdr;

	if (!IS_ENABLED(CONFIG_LEGACY_IMAGE_FORMAT) ||
	    !IS_ENABLED(CONFIG_CMD_BOOTM_PRE_LOAD))
		return -EAGAIN;

	hdr = setup_image();

	ut_assertok(run_commandf("bootm preload %lx", IMAGE_ADDR));
	console_record_reset();
	ut_asserteq(IMAGE_ADDR, env_get_hex("loadaddr_verified", 0));

	/*
	 * the variable is not in the default environment, so removing it puts
	 * things back as they were
	 */
	ut_assertok(env_set("loadaddr_verified", NULL));
	images.state = 0;

	unmap_sysmem(hdr);

	return 0;
}
CMD_TEST(cmd_test_bootm_preload, UTF_CONSOLE);
