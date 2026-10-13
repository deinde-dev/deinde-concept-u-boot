// SPDX-License-Identifier: GPL-2.0+
/*
 * Tests for the bootelf command
 *
 * Copyright 2026 Simon Glass <sjg@chromium.org>
 *
 * The command needs a real ELF image, so each test builds one in memory with
 * make_elf(). The image carries a program header and a section header for the
 * same bytes but pointing at different destinations, so that the loader which
 * ran can be told from where the payload lands
 */

#include <command.h>
#include <console.h>
#include <elf.h>
#include <env.h>
#include <errno.h>
#include <image.h>
#include <malloc.h>
#include <mapmem.h>
#include <stdio.h>
#include <version.h>
#include <dm/of.h>
#include <linux/libfdt.h>
#include <test/cmd.h>
#include <test/ut.h>

/* Number of payload bytes the image carries */
#define PAYLOAD_SIZE	16

/* Offset of each table within the image, in the order they are written */
#define ELF_PHOFF	sizeof(Elf64_Ehdr)
#define ELF_SHOFF	(ELF_PHOFF + sizeof(Elf64_Phdr))
#define ELF_DATA_OFF	(ELF_SHOFF + 2 * sizeof(Elf64_Shdr))
#define ELF_SIZE	(ELF_DATA_OFF + PAYLOAD_SIZE)

/*
 * Size of the devicetree the '-d' tests build, and of the buffer holding it.
 * The command pads what it is given by CONFIG_SYS_FDT_PAD before setting it
 * up, so the buffer has to allow for that.
 */
#define FDT_SIZE	0x400
#define FDT_BUF_SIZE	(FDT_SIZE + CONFIG_SYS_FDT_PAD)

/*
 * Name of the function which logs the invalid-image message, right-aligned in
 * CONFIG_LOGF_FUNC_PAD (20) characters as the log emits it
 */
#define ELF_FUNC	"          do_bootelf"

/* The two long lines of the usage message */
#define LOAD_LINE \
	"\t- load ELF image at [address] via program headers (-p)"
#define FDT_LINE	"\t- setup FDT image at [fdt_addr_r] (-d)"

/* Value which the stand-in image returns; set before each test */
static ulong elf_result;

/**
 * struct test_elf - An image built for a test, and where its payload may land
 *
 * @image: Buffer holding the ELF image itself
 * @phdr_buf: Buffer the program header asks the payload to be loaded at
 * @shdr_buf: Buffer the section header asks the payload to be loaded at
 * @addr: Address of @image, which is what the command is given
 * @saved: Value of the 'autostart' variable before the test
 */
struct test_elf {
	u8 *image;
	u8 *phdr_buf;
	u8 *shdr_buf;
	ulong addr;
	char *saved;
};

/**
 * test_elf() - Stand-in for the ELF image which the command starts
 *
 * bootelf_exec() jumps straight to the entry point in the ELF header, so this
 * has the signature it calls. Unlike a real image it returns, so that the test
 * can carry on afterwards.
 *
 * @argc: Number of arguments left on the command line
 * @argv: Those arguments
 * Return: value chosen by the test
 */
static ulong test_elf(int argc, char *const argv[])
{
	int i;

	printf("elf:");
	for (i = 0; i < argc; i++)
		printf(" %s", argv[i]);
	printf("\n");

	return elf_result;
}

/**
 * make_elf() - Build an ELF image which the command can load
 *
 * The payload is a byte ramp, written once into the image and described twice:
 * as a loadable segment landing at @phdr_dest and as an allocated section
 * landing at @shdr_dest.
 *
 * The image is ELFCLASS64 whatever the host is, since the entry point is a
 * pointer to a function inside U-Boot and sandbox addresses do not fit in the
 * 32-bit field.
 *
 * @buf: Buffer of at least ELF_SIZE bytes to build the image in
 * @entry: Entry point to record in the header
 * @phdr_dest: Address the loadable segment asks to be loaded at
 * @shdr_dest: Address the allocated section asks to be loaded at
 */
static void make_elf(void *buf, ulong entry, ulong phdr_dest, ulong shdr_dest)
{
	Elf64_Ehdr *ehdr = buf;
	Elf64_Phdr *phdr = buf + ELF_PHOFF;
	Elf64_Shdr *shdr = buf + ELF_SHOFF;
	u8 *data = buf + ELF_DATA_OFF;
	uint i;

	memset(buf, '\0', ELF_SIZE);

	/*
	 * The loaders check the magic number and the class and then read each
	 * field natively, so the rest of e_ident is left at zero
	 */
	memcpy(ehdr->e_ident, ELFMAG, SELFMAG);
	ehdr->e_ident[EI_CLASS] = ELFCLASS64;
	ehdr->e_type = ET_EXEC;
	ehdr->e_version = EV_CURRENT;
	ehdr->e_entry = entry;
	ehdr->e_phoff = ELF_PHOFF;
	ehdr->e_shoff = ELF_SHOFF;
	ehdr->e_ehsize = sizeof(*ehdr);
	ehdr->e_phentsize = sizeof(*phdr);
	ehdr->e_phnum = 1;
	ehdr->e_shentsize = sizeof(*shdr);
	ehdr->e_shnum = 2;

	/* the segment which '-p' follows */
	phdr->p_type = PT_LOAD;
	phdr->p_flags = PF_R | PF_X;
	phdr->p_offset = ELF_DATA_OFF;
	phdr->p_vaddr = phdr_dest;
	phdr->p_paddr = phdr_dest;
	phdr->p_filesz = PAYLOAD_SIZE;
	phdr->p_memsz = PAYLOAD_SIZE;

	/*
	 * The section which '-s' follows. Section 0 is left as SHT_NULL, which
	 * the loader skips, and is also the string-table index, so no section
	 * name is looked up.
	 */
	shdr[1].sh_type = SHT_PROGBITS;
	shdr[1].sh_flags = SHF_ALLOC;
	shdr[1].sh_addr = shdr_dest;
	shdr[1].sh_offset = ELF_DATA_OFF;
	shdr[1].sh_size = PAYLOAD_SIZE;

	for (i = 0; i < PAYLOAD_SIZE; i++)
		data[i] = i;
}

/**
 * check_payload() - Check whether the payload landed somewhere
 *
 * @uts: Test state
 * @buf: Destination buffer to look at
 * @loaded: true if the byte ramp should be there, false if the buffer should
 *	still be untouched
 */
static int check_payload(struct unit_test_state *uts, const u8 *buf,
			 bool loaded)
{
	uint i;

	for (i = 0; i < PAYLOAD_SIZE; i++)
		ut_asserteq(loaded ? i : 0xff, buf[i]);

	return 0;
}

/**
 * make_fdt() - Build a devicetree with nothing but a root node in it
 *
 * The setup the command does needs only a valid devicetree with room to grow,
 * so this is cheaper than copying the control one and does not depend on it.
 *
 * @uts: Test state
 * @fdt: Buffer of FDT_BUF_SIZE bytes to build the devicetree in
 */
static int make_fdt(struct unit_test_state *uts, void *fdt)
{
	ut_assertok(fdt_create(fdt, FDT_SIZE));
	ut_assertok(fdt_finish_reservemap(fdt));
	ut_assert(fdt_begin_node(fdt, "") >= 0);
	ut_assertok(fdt_end_node(fdt));
	ut_assertok(fdt_finish(fdt));

	/* the version is added by the setup, so it is not there yet */
	ut_asserteq(-FDT_ERR_NOTFOUND, fdt_path_offset(fdt, "/chosen"));

	return 0;
}

/**
 * check_log() - Check the next console line, which the command logs
 *
 * The log format shows the function name only when CONFIG_LOGF_FUNC is
 * enabled. That is true of sandbox but of none of the other sandbox variants,
 * so the expected line differs between them.
 *
 * @uts: Test state
 * @msg: Message the command logs
 */
static int check_log(struct unit_test_state *uts, const char *msg)
{
	if (IS_ENABLED(CONFIG_LOGF_FUNC))
		ut_assert_nextline("%s() %s", ELF_FUNC, msg);
	else
		ut_assert_nextline("%s", msg);

	return 0;
}

/**
 * check_usage() - Check the usage message which the command prints
 *
 * The '-d' option appears in it only where CONFIG_CMD_ELF_FDT_SETUP is
 * enabled, which is true of sandbox but of no other sandbox variant. Without
 * it the help text ends in a newline, so there is a blank line after the last
 * one.
 *
 * @uts: Test state
 */
static int check_usage(struct unit_test_state *uts)
{
	ut_assert_nextline("bootelf - Boot from an ELF image in memory");
	ut_assert_nextline_empty();
	ut_assert_nextline("Usage:");
	if (IS_ENABLED(CONFIG_CMD_ELF_FDT_SETUP))
		ut_assert_nextline("bootelf [-p|-s] [-d fdt_addr_r] [address]");
	else
		ut_assert_nextline("bootelf [-p|-s] [address]");
	ut_assert_nextline("%s", LOAD_LINE);
	ut_assert_nextline("\t  or via section headers (-s)");
	if (IS_ENABLED(CONFIG_CMD_ELF_FDT_SETUP))
		ut_assert_nextline("%s", FDT_LINE);
	else
		ut_assert_nextline_empty();	/* the text ends in a newline */
	ut_assert_console_end();

	return 0;
}

/**
 * set_autostart() - Set the 'autostart' variable, remembering what it was
 *
 * Sandbox leaves this unset, but another test may have set it, so each test
 * says what it wants and puts the old value back afterwards.
 *
 * @uts: Test state
 * @val: Value to set, or NULL to unset it
 * @savedp: Returns a copy of the old value, or NULL where it is unset; the
 *	caller must free() it
 */
static int set_autostart(struct unit_test_state *uts, const char *val,
			 char **savedp)
{
	const char *old = env_get("autostart");

	*savedp = old ? strdup(old) : NULL;
	ut_assertok(env_set("autostart", val));

	return 0;
}

/**
 * restore_autostart() - Put the 'autostart' variable back as it was
 *
 * @uts: Test state
 * @saved: Value before the test, as set_autostart() returned it
 */
static int restore_autostart(struct unit_test_state *uts, char *saved)
{
	ut_assertok(env_set("autostart", saved));
	free(saved);

	return 0;
}

/**
 * setup() - Build an image and say what the 'autostart' variable should be
 *
 * The two destination buffers start out filled with 0xff, so check_payload()
 * can tell a buffer which was written from one which was left alone.
 *
 * @uts: Test state
 * @elf: Returns the image and its buffers
 * @autostart: Value to give the variable, or NULL to unset it
 */
static int setup(struct unit_test_state *uts, struct test_elf *elf,
		 const char *autostart)
{
	elf->image = malloc(ELF_SIZE);
	elf->phdr_buf = malloc(PAYLOAD_SIZE);
	elf->shdr_buf = malloc(PAYLOAD_SIZE);
	ut_assertnonnull(elf->image);
	ut_assertnonnull(elf->phdr_buf);
	ut_assertnonnull(elf->shdr_buf);
	memset(elf->phdr_buf, 0xff, PAYLOAD_SIZE);
	memset(elf->shdr_buf, 0xff, PAYLOAD_SIZE);

	elf->addr = map_to_sysmem(elf->image);
	make_elf(elf->image, (ulong)test_elf, map_to_sysmem(elf->phdr_buf),
		 map_to_sysmem(elf->shdr_buf));

	ut_assertok(set_autostart(uts, autostart, &elf->saved));

	return 0;
}

/**
 * teardown() - Free the image and put 'autostart' back as it was
 *
 * @uts: Test state
 * @elf: Image and buffers which setup() returned
 */
static int teardown(struct unit_test_state *uts, struct test_elf *elf)
{
	ut_assertok(restore_autostart(uts, elf->saved));
	free(elf->shdr_buf);
	free(elf->phdr_buf);
	free(elf->image);

	return 0;
}

/* Test 'bootelf' loading an image through its section headers */
static int cmd_test_bootelf_base(struct unit_test_state *uts)
{
	struct test_elf elf;

	/* with 'autostart' unset the image is loaded but not started */
	ut_assertok(setup(uts, &elf, NULL));
	ut_assertok(run_commandf("bootelf %lx", elf.addr));
	ut_assert_console_end();

	/* section headers are the default, so only that destination is written */
	ut_assertok(check_payload(uts, elf.shdr_buf, true));
	ut_assertok(check_payload(uts, elf.phdr_buf, false));

	ut_assertok(teardown(uts, &elf));

	return 0;
}
CMD_TEST(cmd_test_bootelf_base, UTF_CONSOLE);

/* Test 'bootelf -p' loading an image through its program headers */
static int cmd_test_bootelf_phdr(struct unit_test_state *uts)
{
	struct test_elf elf;

	ut_assertok(setup(uts, &elf, NULL));
	ut_assertok(run_commandf("bootelf -p %lx", elf.addr));
	ut_assert_console_end();

	ut_assertok(check_payload(uts, elf.phdr_buf, true));
	ut_assertok(check_payload(uts, elf.shdr_buf, false));

	/* '-s' asks for the default, so it must behave as the bare command */
	ut_assertok(run_commandf("bootelf -s %lx", elf.addr));
	ut_assert_console_end();
	ut_assertok(check_payload(uts, elf.shdr_buf, true));

	ut_assertok(teardown(uts, &elf));

	return 0;
}
CMD_TEST(cmd_test_bootelf_phdr, UTF_CONSOLE);

/* Test 'bootelf' starting the image it has loaded */
static int cmd_test_bootelf_start(struct unit_test_state *uts)
{
	struct test_elf elf;

	ut_assertok(setup(uts, &elf, "y"));

	/* what is left of the command line is passed on to the image */
	elf_result = 0;
	ut_assertok(run_commandf("bootelf -p %lx hello world", elf.addr));
	ut_assert_nextline("elf: hello world");
	ut_assert_console_end();

	/* with nothing left, the image gets a single empty argument */
	ut_assertok(run_commandf("bootelf -p %lx", elf.addr));
	ut_assert_nextline("elf:");
	ut_assert_console_end();

	/* whatever the image returns is reported as the command's result */
	elf_result = 0x1234;
	ut_asserteq(1, run_commandf("bootelf -p %lx", elf.addr));
	ut_assert_nextline("elf:");
	ut_assert_console_end();

	ut_assertok(teardown(uts, &elf));

	return 0;
}
CMD_TEST(cmd_test_bootelf_start, UTF_CONSOLE);

/* Test 'bootelf' with no image where it is looking */
static int cmd_test_bootelf_noelf(struct unit_test_state *uts)
{
	struct test_elf elf;

	/* nothing but zeros where the header should be */
	ut_assertok(setup(uts, &elf, NULL));
	memset(elf.image, '\0', ELF_SIZE);

	ut_asserteq(1, run_commandf("bootelf %lx", elf.addr));
	ut_assert_nextline("## No elf image at address 0x%08lx", elf.addr);
	ut_assert_console_end();

	/* with 'autostart' set the command says so a second time */
	ut_assertok(env_set("autostart", "y"));
	ut_asserteq(1, run_commandf("bootelf %lx", elf.addr));
	ut_assert_nextline("## No elf image at address 0x%08lx", elf.addr);
	ut_assertok(check_log(uts, "Invalid ELF image"));
	ut_assert_console_end();

	ut_assertok(teardown(uts, &elf));

	return 0;
}
CMD_TEST(cmd_test_bootelf_noelf, UTF_CONSOLE);

/* Test that an image which is not executable is refused */
static int cmd_test_bootelf_notexec(struct unit_test_state *uts)
{
	struct test_elf elf;
	Elf64_Ehdr *ehdr;

	ut_assertok(setup(uts, &elf, NULL));

	/* a relocatable file has the right magic number but cannot be run */
	ehdr = (Elf64_Ehdr *)elf.image;
	ehdr->e_type = ET_REL;

	ut_asserteq(1, run_commandf("bootelf %lx", elf.addr));
	ut_assert_nextline("## Not a 32-bit elf image at address 0x%08lx",
			   elf.addr);
	ut_assert_console_end();
	ut_assertok(check_payload(uts, elf.shdr_buf, false));

	ut_assertok(teardown(uts, &elf));

	return 0;
}
CMD_TEST(cmd_test_bootelf_notexec, UTF_CONSOLE);

/* Test 'bootelf -d' setting up a devicetree before the image runs */
static int cmd_test_bootelf_fdt(struct unit_test_state *uts)
{
	struct test_elf elf;
	ulong fdt_addr;
	void *fdt;
	int node;

	if (!IS_ENABLED(CONFIG_CMD_ELF_FDT_SETUP))
		return -EAGAIN;

	/*
	 * Without a live tree the setup emits an EVT_FT_FIXUP event, which
	 * reaches the VBE code and reads its state from a block device that
	 * only test.py provides. That is beside the point here, so leave the
	 * flat-tree case alone.
	 */
	if (!of_live_active())
		return -EAGAIN;

	fdt = malloc(FDT_BUF_SIZE);
	ut_assertnonnull(fdt);
	ut_assertok(make_fdt(uts, fdt));
	fdt_addr = map_to_sysmem(fdt);

	ut_assertok(setup(uts, &elf, NULL));
	ut_assertok(run_commandf("bootelf -p -d %lx %lx", fdt_addr, elf.addr));
	ut_assert_console_end();

	/*
	 * The image is loaded as usual and the devicetree is set up: the
	 * version is one of the things fdt_chosen() records there
	 */
	ut_assertok(check_payload(uts, elf.phdr_buf, true));
	node = fdt_path_offset(fdt, "/chosen");
	ut_assert(node >= 0);
	ut_asserteq_str(PLAIN_VERSION,
			fdt_getprop(fdt, node, "u-boot,version", NULL));

	ut_assertok(teardown(uts, &elf));
	free(fdt);

	return 0;
}
CMD_TEST(cmd_test_bootelf_fdt, UTF_CONSOLE);

/* Test 'bootelf -d' with something which is not a devicetree */
static int cmd_test_bootelf_badfdt(struct unit_test_state *uts)
{
	ulong fdt_addr;
	void *fdt;
	char *saved;

	if (!IS_ENABLED(CONFIG_CMD_ELF_FDT_SETUP))
		return -EAGAIN;

	/*
	 * The address must be mapped before it is read, or sandbox dies here
	 * rather than reporting the problem
	 */
	fdt = malloc(FDT_BUF_SIZE);
	ut_assertnonnull(fdt);
	memset(fdt, '\0', FDT_BUF_SIZE);
	fdt_addr = map_to_sysmem(fdt);

	ut_assertok(set_autostart(uts, NULL, &saved));
	ut_asserteq(1, run_commandf("bootelf -d %lx 0", fdt_addr));
	ut_assert_nextline("fdt_root: FDT_ERR_BADMAGIC");
	ut_assert_nextline("ERROR: root node setup failed");
	ut_assert_nextline(" - must RESET the board to recover.");
	ut_assert_nextline_empty();
	ut_assert_console_end();

	ut_assertok(restore_autostart(uts, saved));
	free(fdt);

	return 0;
}
CMD_TEST(cmd_test_bootelf_badfdt, UTF_CONSOLE);

/* Test 'bootelf -d' with no address for the devicetree */
static int cmd_test_bootelf_nofdt(struct unit_test_state *uts)
{
	char *saved;

	if (!IS_ENABLED(CONFIG_CMD_ELF_FDT_SETUP))
		return -EAGAIN;

	/* the option needs an argument, which is missing here */
	ut_assertok(set_autostart(uts, NULL, &saved));
	ut_asserteq(1, run_command("bootelf -d", 0));
	ut_assertok(check_usage(uts));

	ut_assertok(restore_autostart(uts, saved));

	return 0;
}
CMD_TEST(cmd_test_bootelf_nofdt, UTF_CONSOLE);

/* Test that the command refuses an option it does not have */
static int cmd_test_bootelf_badopt(struct unit_test_state *uts)
{
	char *saved;

	ut_assertok(set_autostart(uts, NULL, &saved));
	ut_asserteq(1, run_command("bootelf -x 1000", 0));
	ut_assertok(check_usage(uts));

	/* an option letter is a whole argument, so '-px' is not '-p' either */
	ut_asserteq(1, run_command("bootelf -px 1000", 0));
	ut_assertok(check_usage(uts));

	ut_assertok(restore_autostart(uts, saved));

	return 0;
}
CMD_TEST(cmd_test_bootelf_badopt, UTF_CONSOLE);
