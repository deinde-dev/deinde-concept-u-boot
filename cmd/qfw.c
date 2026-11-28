// SPDX-License-Identifier: GPL-2.0+
/*
 * (C) Copyright 2015 Miao Yan <yanmiaobest@gmail.com>
 * Copyright 2025 Simon Glass <sjg@chromium.org>
 */

#include <abuf.h>
#include <command.h>
#include <display_options.h>
#include <env.h>
#include <errno.h>
#include <getopt.h>
#include <qfw.h>
#include <dm.h>
#include <u-boot/uuid.h>

static struct udevice *qfw_dev;

static int qemu_fwcfg_cmd_list_firmware(void)
{
	int ret;
	struct fw_cfg_file_iter iter;
	struct fw_file *file;

	/* make sure fw_list is loaded */
	ret = qfw_read_firmware_list(qfw_dev);
	if (ret)
		return ret;

	printf("    Addr     Size Sel Name\n");
	printf("-------- -------- --- ------------\n");
	for (file = qfw_file_iter_init(qfw_dev, &iter);
	     !qfw_file_iter_end(&iter);
	     file = qfw_file_iter_next(&iter)) {
		printf("%8lx %8x %3x %-56s\n", file->addr,
		       be32_to_cpu(file->cfg.size),
		       be16_to_cpu(file->cfg.select), file->cfg.name);
	}

	return 0;
}

static int qemu_fwcfg_do_list(struct getopt_state *gs)
{
	if (qemu_fwcfg_cmd_list_firmware() < 0)
		return CMD_RET_FAILURE;

	return 0;
}

static int qemu_fwcfg_do_cpus(struct getopt_state *gs)
{
	printf("%d cpu(s) online\n", qfw_online_cpus(qfw_dev));
	return 0;
}

static int qemu_fwcfg_do_load(struct getopt_state *gs)
{
	int argc = gs->argc;
	char *const *argv = gs->argv;
	char *env;
	ulong load_addr;
	ulong initrd_addr;

	env = env_get("loadaddr");
	load_addr = env ?
		hextoul(env, NULL) :
		CONFIG_SYS_LOAD_ADDR;

	env = env_get("ramdiskaddr");
	initrd_addr = env ?
		hextoul(env, NULL) :
#ifdef CFG_RAMDISK_ADDR
		CFG_RAMDISK_ADDR;
#else
		0;
#endif

	if (argc == 3) {
		load_addr = hextoul(argv[1], NULL);
		initrd_addr = hextoul(argv[2], NULL);
	} else if (argc == 2) {
		load_addr = hextoul(argv[1], NULL);
	}

	if (!load_addr || !initrd_addr) {
		printf("missing load or initrd address\n");
		return CMD_RET_FAILURE;
	}

	return qemu_fwcfg_setup_kernel(qfw_dev, load_addr, initrd_addr);
}

static uint get_val(enum fw_cfg_selector sel)
{
	u32 val;

	qfw_read_entry(qfw_dev, sel, sizeof(val), &val);

	return val;
}

static int do_dump(struct getopt_state *gs)
{
	char uuid_str[UUID_STR_LEN + 1];
	struct abuf buf;
	struct uuid uuid;
	u64 ramsize;
	char sig[5];

	qfw_read_entry(qfw_dev, FW_CFG_SIGNATURE, 4, sig);
	sig[4] = '\0';
	lprint_str("signature", sig);
	lprint_num_32("id", get_val(FW_CFG_ID));

	qfw_read_entry(qfw_dev, FW_CFG_UUID, sizeof(uuid), &uuid);
	uuid_bin_to_str((u8 *)&uuid, uuid_str, 0);
	lprint_str("uuid", uuid_str);

	qfw_read_entry(qfw_dev, FW_CFG_RAM_SIZE, sizeof(ramsize), &ramsize);
	lprint_num_ll("ram_size", ramsize);

	lprint_num_32("nographic", get_val(FW_CFG_NOGRAPHIC));
	lprint_num_32("nb cpus", get_val(FW_CFG_NB_CPUS));
	lprint_num_32("machine id", get_val(FW_CFG_MACHINE_ID));
	lprint_num_32("kernel addr", get_val(FW_CFG_KERNEL_ADDR));
	lprint_num_32("kernel size", get_val(FW_CFG_KERNEL_SIZE));

	lprint_num_32("kernel cmdl", get_val(FW_CFG_KERNEL_CMDLINE));
	lprint_num_32("initrd addr", get_val(FW_CFG_INITRD_ADDR));
	lprint_num_32("initrd size", get_val(FW_CFG_INITRD_SIZE));
	lprint_num_32("boot device", get_val(FW_CFG_BOOT_DEVICE));
	lprint_num_32("numa", get_val(FW_CFG_NUMA));
	lprint_num_32("boot menu", get_val(FW_CFG_BOOT_MENU));
	lprint_num_32("max cpus", get_val(FW_CFG_MAX_CPUS));
	lprint_num_32("kernel entry", get_val(FW_CFG_KERNEL_ENTRY));

	if (!abuf_init_size(&buf, get_val(FW_CFG_CMDLINE_SIZE)))
		goto nomem;
	qfw_read_entry(qfw_dev, FW_CFG_CMDLINE_DATA, buf.size, buf.data);

	lprint_num_32("cmdline addr", get_val(FW_CFG_CMDLINE_ADDR));
	lprint_num_32("cmdline size", get_val(FW_CFG_CMDLINE_SIZE));
	lprint_str("cmdline data", (const char *)buf.data);
	lprint_num_32("setup addr", get_val(FW_CFG_SETUP_ADDR));
	lprint_num_32("setup size", get_val(FW_CFG_SETUP_SIZE));

	/* convert the number of files to little-endian */
	lprint_num_32("file dir le",
			    be32_to_cpu(get_val(FW_CFG_FILE_DIR)));
	abuf_uninit(&buf);

	return 0;

nomem:
	printf("Out of memory\n");

	return CMD_RET_FAILURE;
}

static int do_table(struct getopt_state *gs)
{
	struct bios_linker_entry *entry, *end;
	struct abuf loader;
	int ret, i;

	ret = qfw_get_table_loader(qfw_dev, &loader);
	if (ret) {
		printf("Error %dE\n", ret);
		return CMD_RET_FAILURE;
	}

	for (entry = loader.data, end = loader.data + loader.size, i = 0;
	     entry < end; entry++, i++) {
		int cmd = le32_to_cpu(entry->command);
		const char *zone_name;

		if (!cmd)
			continue;
		printf("%3d ", i);
		switch (cmd) {
		case BIOS_LINKER_LOADER_COMMAND_ALLOCATE:
			zone_name = entry->alloc.zone == 2 ? "fseg" :
				 entry->alloc.zone == 1 ? "high" : "?";
			printf("alloc: align %x zone %s name '%s'\n",
			       entry->alloc.align, zone_name,
			       entry->alloc.file);
			break;
		case BIOS_LINKER_LOADER_COMMAND_ADD_POINTER:
			printf("add-ptr offset %x size %x dest '%s' src '%s'\n",
			       entry->pointer.offset, entry->pointer.size,
			       entry->pointer.dest_file,
			       entry->pointer.src_file);
			break;
		case BIOS_LINKER_LOADER_COMMAND_ADD_CHECKSUM:
			printf("add-chksum offset %x start %x length %x name '%s'\n",
			       entry->cksum.offset, entry->cksum.start,
			       entry->cksum.length,  entry->cksum.file);
			break;
		}
	}

	return 0;
}

static int do_arch(struct getopt_state *gs)
{
	if (!IS_ENABLED(CONFIG_X86))
		return 0;

	lprint_num_32("acpi tables", get_val(FW_CFG_ACPI_TABLES));
	lprint_num_32("smbios entrs", get_val(FW_CFG_SMBIOS_ENTRIES));
	lprint_num_32("irq0 overr", get_val(FW_CFG_IRQ0_OVERRIDE));
	lprint_num_32("hpet", get_val(FW_CFG_HPET));

	return 0;
}

static int do_read(struct getopt_state *gs)
{
	char *const *argv = gs->argv;
	int ret;

	if (gs->argc < 3)
		return CMD_RET_USAGE;

	ret = qfw_load_file(qfw_dev, argv[2], hextoul(argv[1], NULL));
	if (ret)
		return CMD_RET_FAILURE;

	return 0;
}

static int do_e820(struct getopt_state *gs)
{
	if (!IS_ENABLED(CONFIG_X86)) {
		printf("Not supported on this architecture\n");
		return CMD_RET_FAILURE;
	}

	cmd_qfw_e820(qfw_dev);

	return 0;
}

static struct cmd_tbl fwcfg_commands[] = {
	U_BOOT_CMD_MKENT_NOOPTS(list, 0, 1, qemu_fwcfg_do_list, "", ""),
	U_BOOT_CMD_MKENT_NOOPTS(cpus, 0, 1, qemu_fwcfg_do_cpus, "", ""),
	U_BOOT_CMD_MKENT_NOOPTS(load, 2, 1, qemu_fwcfg_do_load, "", ""),
	U_BOOT_CMD_MKENT_NOOPTS(dump, 0, 1, do_dump, "", ""),
	U_BOOT_CMD_MKENT_NOOPTS(table, 0, 1, do_table, "", ""),
	U_BOOT_CMD_MKENT_NOOPTS(arch, 0, 1, do_arch, "", ""),
	U_BOOT_CMD_MKENT_NOOPTS(read, 2, 1, do_read, "", ""),
	U_BOOT_CMD_MKENT_NOOPTS(e820, 0, 1, do_e820, "", ""),
};

static int do_qemu_fw(struct getopt_state *gs)
{
	int argc = gs->argc;
	char *const *argv = gs->argv;
	struct cmd_tbl *fwcfg_cmd;
	int ret;

	ret = qfw_get_dev(&qfw_dev);
	if (ret) {
		cmd_err("QEMU fw_cfg interface not found\n");
		return CMD_RET_USAGE;
	}

	fwcfg_cmd = find_cmd_tbl(argv[1], fwcfg_commands,
				 ARRAY_SIZE(fwcfg_commands));
	if (!fwcfg_cmd || argc - 2 > fwcfg_cmd->maxargs)
		return CMD_RET_USAGE;

	ret = cmd_invoke(fwcfg_cmd, gs->cmd_flag, argc - 1, argv + 1);

	return cmd_process_error(fwcfg_cmd, ret);
}

U_BOOT_CMD_NOOPTS(
	qfw,	4,	1,	do_qemu_fw,
	"QEMU firmware interface",
	"<command>\n"
	"    - dump                             : dump out all values\n"
	"    - list                             : print firmware(s) currently loaded\n"
	"    - cpus                             : print online cpu number\n"
	"    - load <kernel addr> <initrd addr> : load kernel and initrd (if any), and setup for zboot\n"
	"    - table                            : show /etc/table-loader\n"
	"    - arch                             : show arch-specific data\n"
	"    - read <addr> <filename>           : read a file into memory\n"
	"    - e820                             : show QEMU e820 table");
