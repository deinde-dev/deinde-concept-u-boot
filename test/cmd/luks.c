// SPDX-License-Identifier: GPL-2.0+
/*
 * Tests for the luks command
 *
 * Copyright 2026 Simon Glass <sjg@chromium.org>
 */

#include <blk.h>
#include <blkmap.h>
#include <command.h>
#include <dm.h>
#include <luks.h>
#include <malloc.h>
#include <test/cmd.h>
#include <test/ut.h>
#include <asm/unaligned.h>

/* Block size of a blkmap device */
#define BLKSZ		512

/*
 * Blocks in the device each test creates. The LUKS2 header below is 8KB and
 * 'luks info' reads all of it, so the device must be at least that large
 */
#define DEV_BLOCKS	64

/* Size of the LUKS2 header, binary part plus JSON area */
#define LUKS2_HDR_SIZE	0x2000

/* Offset of the JSON area within that header */
#define LUKS2_JSON_OFFSET	4096

/* JSON metadata written into the synthetic LUKS2 header */
#define LUKS2_JSON	"{\"segments\":{\"0\":{\"type\":\"crypt\"}}}"

/* what 'luks detect' reports when it finds no header at all */
#define DETECT_NONE \
	"Not a LUKS partition (error -2: No such file or directory)"

/* ...and when it finds one whose version it does not know */
#define DETECT_VERSION \
	"Not a LUKS partition (error -93: Protocol not supported)"

/* what 'luks unlock' reports when it finds no key slot to try */
#define UNLOCK_ENOENT \
	"Failed to unlock LUKS partition (err -2: No such file or directory)"

/*
 * Every test here carries UTF_DM, since a blkmap device is bound into the
 * driver-model tree: without it the test runs against whatever tree the last
 * test left behind, as test/cmd/clone.c explains
 */

/**
 * struct luks_dev - A memory-backed block device for the tests
 *
 * @dev: blkmap device
 * @mem: Memory holding the contents of the device
 * @seq: Device number, which is what the luks command takes as its <dev>
 *	argument
 */
struct luks_dev {
	struct udevice *dev;
	void *mem;
	int seq;
};

/**
 * make_dev() - Create a block device backed by a fresh piece of memory
 *
 * The memory starts out zeroed, which is not a LUKS header, so a test must
 * write one itself before the command finds anything
 *
 * @uts: Test state
 * @ld: Device to fill in
 * @label: blkmap label, which must not be in use
 * Return: 0 if OK, other value on error
 */
static int make_dev(struct unit_test_state *uts, struct luks_dev *ld,
		    const char *label)
{
	ld->mem = calloc(1, DEV_BLOCKS * BLKSZ);
	ut_assertnonnull(ld->mem);

	ut_assertok(blkmap_create(label, &ld->dev));
	ut_assertok(blkmap_map_mem(ld->dev, 0, DEV_BLOCKS, ld->mem));
	ld->seq = dev_seq(ld->dev);

	return 0;
}

/**
 * free_dev() - Drop a device made by make_dev()
 *
 * @uts: Test state
 * @ld: Device to drop
 * Return: 0 if OK, other value on error
 */
static int free_dev(struct unit_test_state *uts, struct luks_dev *ld)
{
	ut_assertok(blkmap_destroy(ld->dev));
	free(ld->mem);

	return 0;
}

/**
 * make_luks1() - Write a LUKS1 header at the start of a device
 *
 * Only the fields which 'luks info' shows are filled in; the key slots are
 * left disabled, since no test here unlocks the device
 *
 * @mem: Memory backing the device
 * @version: Version to claim in the header, normally LUKS_VERSION_1
 */
static void make_luks1(void *mem, uint version)
{
	struct luks1_phdr *hdr = mem;

	memcpy(hdr->magic, LUKS_MAGIC, LUKS_MAGIC_LEN);
	put_unaligned_be16(version, &hdr->version);
	strlcpy(hdr->cipher_name, "aes", sizeof(hdr->cipher_name));
	strlcpy(hdr->cipher_mode, "cbc-essiv:sha256",
		sizeof(hdr->cipher_mode));
	strlcpy(hdr->hash_spec, "sha256", sizeof(hdr->hash_spec));
	put_unaligned_be32(4096, &hdr->payload_offset);
	put_unaligned_be32(32, &hdr->key_bytes);
}

/**
 * make_luks2() - Write a LUKS2 header at the start of a device
 *
 * @mem: Memory backing the device
 */
static void make_luks2(void *mem)
{
	struct luks2_hdr *hdr = mem;

	memcpy(hdr->magic, LUKS_MAGIC, LUKS_MAGIC_LEN);
	put_unaligned_be16(LUKS_VERSION_2, &hdr->version);
	put_unaligned_be64(LUKS2_HDR_SIZE, &hdr->hdr_size);
	put_unaligned_be64(7, &hdr->seqid);
	strlcpy(hdr->label, "test-label", sizeof(hdr->label));
	strlcpy(hdr->csum_alg, "sha256", sizeof(hdr->csum_alg));
	strlcpy(hdr->uuid, "d2bb4bb4-b3a0-4d2e-9d47-2b1a1f4e6c8a",
		sizeof(hdr->uuid));
	memcpy((char *)mem + LUKS2_JSON_OFFSET, LUKS2_JSON,
	       sizeof(LUKS2_JSON));
}

/* Test detecting a LUKS1 partition and showing its header */
static int cmd_test_luks_base(struct unit_test_state *uts)
{
	struct luks_dev ld;

	ut_assertok(make_dev(uts, &ld, "luks-base"));
	make_luks1(ld.mem, LUKS_VERSION_1);

	ut_assertok(run_commandf("luks detect blkmap %d", ld.seq));
	ut_assert_nextline("LUKS1 encrypted partition detected");
	ut_assert_console_end();

	ut_assertok(run_commandf("luks info blkmap %d", ld.seq));
	ut_assert_nextline("Version:        1");
	ut_assert_nextline("Cipher name:    aes");
	ut_assert_nextline("Cipher mode:    cbc-essiv:sha256");
	ut_assert_nextline("Hash spec:      sha256");
	ut_assert_nextline("Payload offset: 4096 sectors");
	ut_assert_nextline("Key bytes:      32");
	ut_assert_console_end();

	ut_assertok(free_dev(uts, &ld));

	return 0;
}
CMD_TEST(cmd_test_luks_base,
	 UTF_CONSOLE | UTF_DM | UTF_SCAN_PDATA | UTF_SCAN_FDT);

/* Test detecting a LUKS2 partition and showing its header */
static int cmd_test_luks_luks2(struct unit_test_state *uts)
{
	struct luks_dev ld;

	ut_assertok(make_dev(uts, &ld, "luks-luks2"));
	make_luks2(ld.mem);

	ut_assertok(run_commandf("luks detect blkmap %d", ld.seq));
	ut_assert_nextline("LUKS2 encrypted partition detected");
	ut_assert_console_end();

	ut_assertok(run_commandf("luks info blkmap %d", ld.seq));
	ut_assert_nextline("Version:        2");
	ut_assert_nextline("Header size:    8192 bytes");
	ut_assert_nextline("Sequence ID:    7");
	ut_assert_nextline("UUID:           d2bb4bb4-b3a0-4d2e-9d47-2b1a1f4e6c8a");
	ut_assert_nextline("Label:          test-label");
	ut_assert_nextline("Checksum alg:   sha256");
	ut_assert_nextline_empty();
	ut_assert_nextline("JSON metadata (4096 bytes):");
	ut_assert_nextline("{");
	ut_assert_nextline("  \"segments\": {");
	ut_assert_nextline("    \"0\": {");
	ut_assert_nextline("      \"type\": \"crypt\"");
	ut_assert_nextline("    }");
	ut_assert_nextline("  }");
	ut_assert_nextline("}");
	ut_assert_console_end();

	ut_assertok(free_dev(uts, &ld));

	return 0;
}
CMD_TEST(cmd_test_luks_luks2,
	 UTF_CONSOLE | UTF_DM | UTF_SCAN_PDATA | UTF_SCAN_FDT);

/* Test what each sub-command says about a device holding no LUKS header */
static int cmd_test_luks_absent(struct unit_test_state *uts)
{
	struct luks_dev ld;

	ut_assertok(make_dev(uts, &ld, "luks-absent"));

	ut_asserteq(1, run_commandf("luks detect blkmap %d", ld.seq));
	ut_assert_nextline(DETECT_NONE);
	ut_assert_console_end();

	ut_asserteq(1, run_commandf("luks info blkmap %d", ld.seq));
	ut_assert_nextline("Not a LUKS partition");
	ut_assert_console_end();

	ut_asserteq(1, run_commandf("luks unlock blkmap %d pass", ld.seq));
	ut_assert_nextline("Not a LUKS partition");
	ut_assert_console_end();

	ut_assertok(free_dev(uts, &ld));

	return 0;
}
CMD_TEST(cmd_test_luks_absent,
	 UTF_CONSOLE | UTF_DM | UTF_SCAN_PDATA | UTF_SCAN_FDT);

/* Test a header whose version is neither 1 nor 2 */
static int cmd_test_luks_version(struct unit_test_state *uts)
{
	struct luks_dev ld;

	ut_assertok(make_dev(uts, &ld, "luks-version"));
	make_luks1(ld.mem, 3);

	ut_asserteq(1, run_commandf("luks detect blkmap %d", ld.seq));
	ut_assert_nextline(DETECT_VERSION);
	ut_assert_console_end();

	/* info shows the version it found before giving up on it */
	ut_asserteq(1, run_commandf("luks info blkmap %d", ld.seq));
	ut_assert_nextline("Version:        3");
	ut_assert_nextline("Unknown LUKS version");
	ut_assert_console_end();

	ut_asserteq(1, run_commandf("luks unlock blkmap %d pass", ld.seq));
	ut_assert_nextline("Not a LUKS partition");
	ut_assert_console_end();

	ut_assertok(free_dev(uts, &ld));

	return 0;
}
CMD_TEST(cmd_test_luks_version,
	 UTF_CONSOLE | UTF_DM | UTF_SCAN_PDATA | UTF_SCAN_FDT);

/* Test the options which unlock takes */
static int cmd_test_luks_options(struct unit_test_state *uts)
{
	struct luks_dev bare, ld;

	ut_assertok(make_dev(uts, &bare, "luks-bare"));
	ut_assertok(make_dev(uts, &ld, "luks-options"));
	make_luks1(ld.mem, LUKS_VERSION_1);

	/*
	 * Both options are accepted. What -t does next depends on whether the
	 * host has a TKey attached, so stop the command at the version check
	 * by giving it the device with no header
	 */
	ut_asserteq(1, run_commandf("luks unlock -t blkmap %d pass", bare.seq));
	ut_assert_nextline("Not a LUKS partition");
	ut_assert_console_end();

	ut_asserteq(1, run_commandf("luks unlock -p blkmap %d 00", bare.seq));
	ut_assert_nextline("Not a LUKS partition");
	ut_assert_console_end();

	/* -p wants a hex string and says so when it does not get one */
	ut_asserteq(1, run_commandf("luks unlock -p blkmap %d zz", ld.seq));
	ut_assert_nextline("Unlocking LUKS1 partition...");
	ut_assert_nextline("Invalid hex key");
	ut_assert_console_end();

	/* no key slot is enabled, so the passphrase matches nothing */
	ut_asserteq(1, run_commandf("luks unlock blkmap %d pass", ld.seq));
	ut_assert_nextline("Unlocking LUKS1 partition...");
	ut_assert_nextline(UNLOCK_ENOENT);
	ut_assert_console_end();

	ut_assertok(free_dev(uts, &ld));
	ut_assertok(free_dev(uts, &bare));

	return 0;
}
CMD_TEST(cmd_test_luks_options,
	 UTF_CONSOLE | UTF_DM | UTF_SCAN_PDATA | UTF_SCAN_FDT);

/* Test the devices each sub-command refuses */
static int cmd_test_luks_baddev(struct unit_test_state *uts)
{
	ut_asserteq(1, run_command("luks detect wibble 0", 0));
	ut_assert_nextline("** Bad device specification wibble 0 **");
	ut_assert_console_end();

	ut_asserteq(1, run_command("luks info wibble 0", 0));
	ut_assert_nextline("** Bad device specification wibble 0 **");
	ut_assert_console_end();

	ut_asserteq(1, run_command("luks unlock wibble 0 pass", 0));
	ut_assert_nextline("** Bad device specification wibble 0 **");
	ut_assert_console_end();

	return 0;
}
CMD_TEST(cmd_test_luks_baddev, UTF_CONSOLE);

/**
 * expect_usage() - Run a command which shows the usage message
 *
 * @uts: Test state
 * @cmd: Command to run
 * Return: 0 if OK, 1 on failure
 */
static int expect_usage(struct unit_test_state *uts, const char *cmd)
{
	ut_asserteq(1, run_command(cmd, 0));
	ut_assert_nextline("luks - LUKS (Linux Unified Key Setup) operations");

	/* the rest is the usage text, which this test does not check */
	console_record_reset();

	return 0;
}

/* Test the arguments which show the usage message */
static int cmd_test_luks_usage(struct unit_test_state *uts)
{
	/* a sub-command is required, and must be one of those on offer */
	ut_assertok(expect_usage(uts, "luks"));
	ut_assertok(expect_usage(uts, "luks other"));

	/* detect and info want an interface and a device, and nothing else */
	ut_assertok(expect_usage(uts, "luks detect"));
	ut_assertok(expect_usage(uts, "luks detect mmc"));
	ut_assertok(expect_usage(uts, "luks info"));
	ut_assertok(expect_usage(uts, "luks info mmc"));

	/* unlock wants a passphrase as well */
	ut_assertok(expect_usage(uts, "luks unlock"));
	ut_assertok(expect_usage(uts, "luks unlock mmc"));
	ut_assertok(expect_usage(uts, "luks unlock mmc 0:1"));

	/* nor does any of them take an extra argument */
	ut_assertok(expect_usage(uts, "luks detect mmc 0:1 extra"));
	ut_assertok(expect_usage(uts, "luks info mmc 0:1 extra"));
	ut_assertok(expect_usage(uts, "luks unlock mmc 0:1 pass extra"));

	/* only -t and -p are on offer */
	ut_assertok(expect_usage(uts, "luks unlock -x mmc 0:1 pass"));

	/* each option says where the key comes from, so only one may be given */
	ut_assertok(expect_usage(uts, "luks unlock -t -p mmc 0:1 pass"));
	ut_assertok(expect_usage(uts, "luks unlock -tp mmc 0:1 pass"));

	/* detect and info take no option, rather than one as an interface */
	ut_assertok(expect_usage(uts, "luks detect -x 0:1"));
	ut_assertok(expect_usage(uts, "luks info -x 0:1"));

	return 0;
}
CMD_TEST(cmd_test_luks_usage, UTF_CONSOLE);
