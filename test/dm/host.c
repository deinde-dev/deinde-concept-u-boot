// SPDX-License-Identifier: GPL-2.0+ OR BSD-3-Clause
/*
 * Copyright 2022 Google LLC
 * Written by Simon Glass <sjg@chromium.org>
 */

#include <blk.h>
#include <command.h>
#include <dm.h>
#include <env.h>
#include <fs_legacy.h>
#include <mapmem.h>
#include <os.h>
#include <sandbox_host.h>
#include <asm/test.h>
#include <dm/device-internal.h>
#include <dm/test.h>
#include <test/test.h>
#include <test/ut.h>

/* Address used to save and load a file, and the number of bytes used */
#define TEST_ADDR	(CONFIG_SYS_LOAD_ADDR + 0x4000)
#define TEST_ADDR2	(CONFIG_SYS_LOAD_ADDR + 0x5000)
#define TEST_SIZE	0x20

/* Name of the file written by the 'host save' test */
#define SAVE_FNAME	"host-save.bin"

/* Last line of the usage message, shown when the arguments are wrong */
#define USAGE_LAST_LINE \
	"with standard IO commands such as fatls or ext2load"

/* Message shown when a backing file cannot be opened */
#define NO_ACCESS_LINE \
	"Failed to access host backing file '/no/such/file', trying read-only"

/**
 * check_usage() - Check that the command has shown its usage message
 *
 * @uts: Test state
 * Return: 0 if OK, other value on error
 */
static int check_usage(struct unit_test_state *uts)
{
	ut_assert_nextline("host - Miscellaneous host commands");
	ut_assert_nextline_empty();
	ut_assert_nextline("Usage:");
	ut_assert_skip_to_line(USAGE_LAST_LINE);
	ut_assert_console_end();

	return 0;
}

/**
 * get_test_dir() - Get the directory holding the test images
 *
 * This is used as a place to write a file, since it is known to exist and to
 * be writable
 *
 * @uts: Test state
 * @buf: Buffer to hold the directory name
 * @size: Size of @buf in bytes
 * Return: 0 if OK, other value on error
 */
static int get_test_dir(struct unit_test_state *uts, char *buf, int size)
{
	char *slash;

	ut_assertok(os_persistent_file(buf, size, "2MB.ext2.img"));
	slash = strrchr(buf, '/');
	ut_assertnonnull(slash);
	*slash = '\0';

	return 0;
}

/* Basic test of host interface */
static int dm_test_host_base(struct unit_test_state *uts)
{
	static char label[] = "test";
	struct udevice *dev, *part, *chk, *blk;
	struct host_sb_plat *plat;
	struct blk_desc *desc;
	char fname[256];
	ulong mem_start;
	loff_t actwrite;

	ut_asserteq(-ENODEV, uclass_first_device_err(UCLASS_HOST, &dev));
	ut_asserteq(-ENODEV, uclass_first_device_err(UCLASS_PARTITION, &part));

	mem_start = ut_check_delta(0);
	ut_assertok(host_create_device(label, true, DEFAULT_BLKSZ, &dev));

	/* Check that the plat data has been allocated */
	plat = dev_get_plat(dev);
	ut_asserteq_str("test", plat->label);
	ut_assert(label != plat->label);
	ut_asserteq(0, plat->fd);

	/* Attach a file created in test_ut_dm_init */
	ut_assertok(os_persistent_file(fname, sizeof(fname), "2MB.ext2.img"));

	ut_assertok(host_attach_file(dev, fname));
	ut_assertok(uclass_first_device_err(UCLASS_HOST, &chk));
	ut_asserteq_ptr(chk, dev);

	ut_asserteq_str(fname, plat->filename);
	ut_assert(fname != plat->filename);
	ut_assert(plat->fd != 0);

	/* Get the block device */
	ut_assertok(blk_get_from_parent(dev, &blk));
	ut_assertok(device_probe(blk));

	/* There should be no partition table in this device */
	ut_asserteq(-ENODEV, uclass_first_device_err(UCLASS_PARTITION, &part));

	/* Write to a file on the ext4 filesystem */
	desc = dev_get_uclass_plat(blk);
	ut_asserteq(true, desc->removable);
	ut_assertok(fs_set_blk_dev_with_part(desc, 0));
	ut_assertok(fs_write("/testing", 0, 0, 0x1000, &actwrite));

	ut_assertok(host_detach_file(dev));
	ut_asserteq(0, plat->fd);
	ut_asserteq(-ENODEV, blk_get_from_parent(dev, &blk));
	ut_assertok(device_unbind(dev));

	/* check there were no memory leaks */
	ut_asserteq(0, ut_check_delta(mem_start));

	return 0;
}
DM_TEST(dm_test_host_base, UTF_SCAN_FDT);

/* reusing the same label should work */
static int dm_test_host_dup(struct unit_test_state *uts)
{
	static char label[] = "test";
	struct udevice *dev, *chk;
	char fname[256];

	ut_asserteq(0, uclass_id_count(UCLASS_HOST));
	ut_assertok(host_create_device(label, true, DEFAULT_BLKSZ, &dev));

	/* Attach a file created in test_ut_dm_init */
	ut_assertok(os_persistent_file(fname, sizeof(fname), "2MB.ext2.img"));
	ut_assertok(host_attach_file(dev, fname));
	ut_assertok(uclass_first_device_err(UCLASS_HOST, &chk));
	ut_asserteq_ptr(chk, dev);
	ut_asserteq(1, uclass_id_count(UCLASS_HOST));

	/* Create another device with the same label (should remove old one) */
	ut_assertok(host_create_device(label, true, DEFAULT_BLKSZ, &dev));

	/* Attach a different file created in test_ut_dm_init */
	ut_assertok(os_persistent_file(fname, sizeof(fname), "1MB.fat32.img"));
	ut_assertok(host_attach_file(dev, fname));

	ut_assertok(uclass_first_device_err(UCLASS_HOST, &chk));
	ut_asserteq_ptr(chk, dev);

	/* Make sure there is still only one device */
	ut_asserteq(1, uclass_id_count(UCLASS_HOST));

	return 0;
}
DM_TEST(dm_test_host_dup, UTF_SCAN_FDT);

/* Basic test of 'host' command */
static int dm_test_cmd_host(struct unit_test_state *uts)
{
	struct udevice *dev, *blk;
	struct blk_desc *desc;
	char fname[256];

	/* first check 'host info' with binding */
	ut_assertok(run_command("host info", 0));
	ut_assert_nextline("dev       blocks  blksz label           path");
	ut_assert_console_end();

	ut_assertok(os_persistent_file(fname, sizeof(fname), "2MB.ext2.img"));
	ut_assertok(run_commandf("host bind -r test2 %s", fname));

	/* Check the -r flag worked */
	ut_assertok(uclass_first_device_err(UCLASS_HOST, &dev));
	ut_assertok(blk_get_from_parent(dev, &blk));
	desc = dev_get_uclass_plat(blk);
	ut_asserteq(true, desc->removable);

	ut_assertok(run_command("host info", 0));
	ut_assert_nextline("dev       blocks  blksz label           path");
	ut_assert_nextlinen("  0         4096    512 test2");
	ut_assert_console_end();

	ut_assertok(os_persistent_file(fname, sizeof(fname), "1MB.fat32.img"));
	ut_assertok(run_commandf("host bind fat %s", fname));

	/* Check it is not removable (no '-r') */
	ut_assertok(uclass_next_device_err(&dev));
	ut_assertok(blk_get_from_parent(dev, &blk));
	desc = dev_get_uclass_plat(blk);
	ut_asserteq(false, desc->removable);

	ut_assertok(run_command("host info", 0));
	ut_assert_nextline("dev       blocks  blksz label           path");
	ut_assert_nextlinen("  0         4096    512 test2");
	ut_assert_nextlinen("  1         2048    512 fat");
	ut_assert_console_end();

	ut_asserteq(1, run_command("host info test", 0));
	ut_assert_nextline("No such device 'test'");
	ut_assert_console_end();

	ut_assertok(run_command("host info fat", 0));
	ut_assert_nextline("dev       blocks  blksz label           path");
	ut_assert_nextlinen("  1         2048    512 fat");
	ut_assert_console_end();

	/* check 'host dev' */
	ut_asserteq(1, run_command("host dev", 0));
	ut_assert_nextline("No current host device");
	ut_assert_console_end();

	ut_asserteq(1, run_command("host dev missing", 0));
	ut_assert_nextline("No such device 'missing'");
	ut_assert_console_end();

	ut_assertok(run_command("host dev fat", 0));
	ut_assert_console_end();

	ut_assertok(run_command("host dev", 0));
	ut_assert_nextline("Current host device: 1: fat");
	ut_assert_console_end();

	/* Try a numerical label */
	ut_assertok(run_command("host dev 0", 0));
	ut_assert_console_end();

	ut_assertok(run_command("host dev", 0));
	ut_assert_nextline("Current host device: 0: test2");
	ut_assert_console_end();

	/* Remove one of the bindings */
	ut_assertok(run_commandf("host unbind test2"));

	/* There should now be no current device */
	ut_asserteq(1, run_command("host dev", 0));
	ut_assert_nextline("No current host device");
	ut_assert_console_end();

	ut_assertok(run_command("host info", 0));
	ut_assert_nextline("dev       blocks  blksz label           path");
	ut_assert_nextlinen("  1         2048    512 fat");
	ut_assert_console_end();

	return 0;
}
DM_TEST(dm_test_cmd_host, UTF_SCAN_FDT | UTF_CONSOLE);

/* Test the arguments of 'host bind' */
static int dm_test_cmd_host_bind(struct unit_test_state *uts)
{
	struct udevice *dev, *blk;
	struct blk_desc *desc;
	char fname[256];

	ut_assertok(os_persistent_file(fname, sizeof(fname), "1MB.fat32.img"));

	/* both a label and a filename are needed */
	ut_asserteq(1, run_command("host bind", 0));
	ut_assertok(check_usage(uts));

	ut_asserteq(1, run_command("host bind onlylabel", 0));
	ut_assertok(check_usage(uts));

	/* the block size must be at least 512 bytes and a power of two */
	ut_asserteq(1, run_commandf("host bind small %s 500", fname));
	ut_assert_nextline("blksz must be >= 512 and power of 2");
	ut_assert_console_end();

	ut_asserteq(1, run_commandf("host bind odd %s 1000", fname));
	ut_assert_nextline("blksz must be >= 512 and power of 2");
	ut_assert_console_end();

	/* a larger block size means fewer blocks for the same file */
	ut_assertok(run_commandf("host bind -r big %s 2048", fname));
	ut_assertok(run_command("host info", 0));
	ut_assert_nextline("dev       blocks  blksz label           path");
	ut_assert_nextlinen("  0          512   2048 big");
	ut_assert_console_end();

	/* the -r flag marks the device as removable */
	ut_assertok(uclass_first_device_err(UCLASS_HOST, &dev));
	ut_assertok(blk_get_from_parent(dev, &blk));
	desc = dev_get_uclass_plat(blk);
	ut_asserteq(true, desc->removable);

	/* binding a file which cannot be opened fails */
	ut_asserteq(1, run_command("host bind missing /no/such/file", 0));
	ut_assert_nextline(NO_ACCESS_LINE);
	ut_assert_nextline("- still failed");
	ut_assert_nextline("Cannot create device / bind file");
	ut_assert_console_end();

	return 0;
}
DM_TEST(dm_test_cmd_host_bind, UTF_SCAN_FDT | UTF_CONSOLE);

/* Test 'host info' writing the device number to a variable */
static int dm_test_cmd_host_info_var(struct unit_test_state *uts)
{
	char fname[256];

	ut_assertok(os_persistent_file(fname, sizeof(fname), "1MB.fat32.img"));
	ut_assertok(run_commandf("host bind first %s", fname));
	ut_assertok(run_commandf("host bind second %s", fname));

	/* with a variable name nothing is shown, only the variable is set */
	ut_assertok(run_command("host info second devnum", 0));
	ut_assert_console_end();
	ut_asserteq(1, env_get_hex("devnum", 0x7f));

	ut_assertok(run_command("host info first devnum", 0));
	ut_assert_console_end();
	ut_asserteq(0, env_get_hex("devnum", 0x7f));

	ut_assertok(env_set("devnum", NULL));

	return 0;
}
DM_TEST(dm_test_cmd_host_info_var, UTF_SCAN_FDT | UTF_CONSOLE);

/* Test the sub-commands which read and write files on the host */
static int dm_test_cmd_host_fs(struct unit_test_state *uts)
{
	char dirname[256], fname[300];
	u8 *buf;
	int i;

	ut_assertok(get_test_dir(uts, dirname, sizeof(dirname)));
	snprintf(fname, sizeof(fname), "%s/%s", dirname, SAVE_FNAME);

	/* write a known pattern to memory and save it to a file */
	ut_assertok(run_commandf("mw.b %lx 41 %x", (ulong)TEST_ADDR,
				 TEST_SIZE));
	ut_assertok(run_commandf("host save hostfs - %lx %s %x",
				 (ulong)TEST_ADDR, fname, TEST_SIZE));
	ut_assert_nextlinen("%d bytes written in ", TEST_SIZE);
	ut_assert_console_end();

	/* 'host size' reports the size in the filesize variable */
	ut_assertok(run_commandf("host size hostfs - %s", fname));
	ut_assert_console_end();
	ut_asserteq(TEST_SIZE, env_get_hex("filesize", 0));

	/* read the file back and check that the pattern arrived */
	ut_assertok(run_commandf("mw.b %lx 0 %x", (ulong)TEST_ADDR2,
				 TEST_SIZE));
	ut_assertok(run_commandf("host load hostfs - %lx %s",
				 (ulong)TEST_ADDR2, fname));
	ut_assert_nextlinen("%d bytes read in ", TEST_SIZE);
	ut_assert_console_end();

	buf = map_sysmem(TEST_ADDR2, TEST_SIZE);
	for (i = 0; i < TEST_SIZE; i++)
		ut_asserteq(0x41, buf[i]);
	unmap_sysmem(buf);

	/* a byte count and an offset read only part of the file */
	ut_assertok(run_commandf("mw.b %lx 0 %x", (ulong)TEST_ADDR2,
				 TEST_SIZE));
	ut_assertok(run_commandf("host load hostfs - %lx %s 4 2",
				 (ulong)TEST_ADDR2, fname));
	ut_assert_nextlinen("4 bytes read in ");
	ut_assert_console_end();

	buf = map_sysmem(TEST_ADDR2, TEST_SIZE);
	ut_asserteq(0x41, buf[3]);
	ut_asserteq(0, buf[4]);
	unmap_sysmem(buf);

	/* the file shows up in a listing of its directory */
	ut_assertok(run_commandf("host ls hostfs - %s", dirname));
	ut_assert_skip_to_line("%14d %s", TEST_SIZE, SAVE_FNAME);
	console_record_reset();

	/* a file which does not exist has no size */
	ut_asserteq(1, run_commandf("host size hostfs - %s/missing", dirname));
	ut_assert_console_end();

	return 0;
}
DM_TEST(dm_test_cmd_host_fs, UTF_SCAN_FDT | UTF_CONSOLE);

/* Test the usage message shown for a missing or unknown sub-command */
static int dm_test_cmd_host_usage(struct unit_test_state *uts)
{
	/* with no sub-command at all */
	ut_asserteq(1, run_command("host", 0));
	ut_assertok(check_usage(uts));

	/* with a sub-command which does not exist */
	ut_asserteq(1, run_command("host nosuch", 0));
	ut_assertok(check_usage(uts));

	/* 'host unbind' needs a label */
	ut_asserteq(1, run_command("host unbind", 0));
	ut_assertok(check_usage(uts));

	/* an unknown label is reported */
	ut_asserteq(1, run_command("host unbind nosuch", 0));
	ut_assert_nextline("No such device 'nosuch'");
	ut_assert_console_end();

	return 0;
}
DM_TEST(dm_test_cmd_host_usage, UTF_SCAN_FDT | UTF_CONSOLE);
