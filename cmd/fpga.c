// SPDX-License-Identifier: GPL-2.0+
/*
 * (C) Copyright 2000, 2001
 * Rich Ireland, Enterasys Networks, rireland@enterasys.com.
 */

/*
 *  FPGA support
 */
#include <command.h>
#include <env.h>
#include <fpga.h>
#include <fs_legacy.h>
#include <getopt.h>
#include <gzip.h>
#include <image.h>
#include <log.h>
#include <malloc.h>

/*
 * Number of arguments the load sub-commands take. The table entry gives this
 * as its maxargs, and the sub-command passes it to do_fpga_check_params(),
 * which has no cmd_tbl of its own to read it from.
 */
#define FPGA_LOAD_ARGS		3
#define FPGA_LOADS_ARGS		6
#define FPGA_LOADFS_ARGS	7

/* message shown when a FIT subimage cannot be read, too long to write inline */
#define MSG_SUBIMAGE	"Could not load '%s' subimage (err %d)\n"

static long do_fpga_get_device(char *arg)
{
	long dev = FPGA_INVALID_DEVICE;
	char *devstr = env_get("fpga");

	if (devstr)
		/* Should be strtol to handle -1 cases */
		dev = simple_strtol(devstr, NULL, 16);

	if (dev == FPGA_INVALID_DEVICE && arg)
		dev = simple_strtol(arg, NULL, 16);

	log_debug("device = %ld\n", dev);

	return dev;
}

/**
 * do_fpga_check_params() - Read the arguments every load sub-command takes
 *
 * @dev: Returns the device to configure
 * @fpga_data: Returns the address of the bitstream
 * @data_size: Returns the size of the bitstream
 * @nargs: Number of arguments the sub-command takes, or 0 if it checks the
 *	count itself
 * @gs: getopt state of the sub-command, whose argv[0] is its name
 * Return: 0 if OK, CMD_RET_FAILURE on error
 */
static int do_fpga_check_params(long *dev, long *fpga_data, size_t *data_size,
				int nargs, struct getopt_state *gs)
{
	char *const *argv = gs->argv + 1;
	int argc = gs->argc - 1;
	size_t local_data_size;
	long local_fpga_data;

	log_debug("%d, %d\n", argc, nargs);

	if (nargs && argc != nargs) {
		log_err("Incorrect number of parameters passed\n");
		return CMD_RET_FAILURE;
	}

	*dev = do_fpga_get_device(argv[0]);

	local_fpga_data = simple_strtol(argv[1], NULL, 16);
	if (!local_fpga_data) {
		log_err("Zero fpga_data address\n");
		return CMD_RET_FAILURE;
	}
	*fpga_data = local_fpga_data;

	local_data_size = hextoul(argv[2], NULL);
	if (!local_data_size) {
		log_err("Zero size\n");
		return CMD_RET_FAILURE;
	}
	*data_size = local_data_size;

	return 0;
}

#if defined(CONFIG_CMD_FPGA_LOAD_SECURE)
static int do_fpga_loads(struct getopt_state *gs)
{
	struct fpga_secure_info fpga_sec_info;
	char *const *argv = gs->argv + 1;
	const int pos_userkey = 5;
	int argc = gs->argc - 1;
	size_t data_size = 0;
	long fpga_data, dev;
	int ret;

	memset(&fpga_sec_info, 0, sizeof(fpga_sec_info));

	if (argc < pos_userkey) {
		log_err("Too few parameters passed\n");
		return CMD_RET_FAILURE;
	}

	if (argc == pos_userkey + 1)
		fpga_sec_info.userkey_addr = (u8 *)(uintptr_t)
					      simple_strtoull(argv[pos_userkey],
							      NULL, 16);

	/*
	 * The user key is optional, so the count is 5 or 6 and the check above
	 * along with the sub-command's maxargs has already made sure of it
	 */
	ret = do_fpga_check_params(&dev, &fpga_data, &data_size, 0, gs);
	if (ret)
		return ret;

	fpga_sec_info.encflag = (u8)hextoul(argv[4], NULL);
	fpga_sec_info.authflag = (u8)hextoul(argv[3], NULL);

	if (fpga_sec_info.authflag >= FPGA_NO_ENC_OR_NO_AUTH &&
	    fpga_sec_info.encflag >= FPGA_NO_ENC_OR_NO_AUTH) {
		log_err("Use <fpga load> for NonSecure bitstream\n");
		return CMD_RET_FAILURE;
	}

	if (fpga_sec_info.encflag == FPGA_ENC_USR_KEY &&
	    !fpga_sec_info.userkey_addr) {
		log_err("User key not provided\n");
		return CMD_RET_FAILURE;
	}

	return fpga_loads(dev, (void *)fpga_data, data_size, &fpga_sec_info);
}
#endif

#if defined(CONFIG_CMD_FPGA_LOADFS)
static int do_fpga_loadfs(struct getopt_state *gs)
{
	char *const *argv = gs->argv + 1;
	size_t data_size = 0;
	long fpga_data, dev;
	int ret;
	fpga_fs_info fpga_fsinfo;

	ret = do_fpga_check_params(&dev, &fpga_data, &data_size,
				   FPGA_LOADFS_ARGS, gs);
	if (ret)
		return ret;

	fpga_fsinfo.fstype = FS_TYPE_ANY;
	fpga_fsinfo.blocksize = (unsigned int)hextoul(argv[3], NULL);
	fpga_fsinfo.interface = argv[4];
	fpga_fsinfo.dev_part = argv[5];
	fpga_fsinfo.filename = argv[6];

	return fpga_fsload(dev, (void *)fpga_data, data_size, &fpga_fsinfo);
}
#endif

static int do_fpga_info(struct getopt_state *gs)
{
	long dev = do_fpga_get_device(gs->argv[1]);

	return fpga_info(dev);
}

static int do_fpga_dump(struct getopt_state *gs)
{
	size_t data_size = 0;
	long fpga_data, dev;
	int ret;

	ret = do_fpga_check_params(&dev, &fpga_data, &data_size,
				   FPGA_LOAD_ARGS, gs);
	if (ret)
		return ret;

	return fpga_dump(dev, (void *)fpga_data, data_size);
}

static int do_fpga_load(struct getopt_state *gs)
{
	size_t data_size = 0;
	long fpga_data, dev;
	int ret;

	ret = do_fpga_check_params(&dev, &fpga_data, &data_size,
				   FPGA_LOAD_ARGS, gs);
	if (ret)
		return ret;

	return fpga_load(dev, (void *)fpga_data, data_size, BIT_FULL, 0);
}

#if defined(CONFIG_CMD_FPGA_LOADB)
static int do_fpga_loadb(struct getopt_state *gs)
{
	size_t data_size = 0;
	long fpga_data, dev;
	int ret;

	ret = do_fpga_check_params(&dev, &fpga_data, &data_size,
				   FPGA_LOAD_ARGS, gs);
	if (ret)
		return ret;

	return fpga_loadbitstream(dev, (void *)fpga_data, data_size, BIT_FULL);
}
#endif
#if defined(CONFIG_CMD_FPGA_LOADP)
static int do_fpga_loadp(struct getopt_state *gs)
{
	size_t data_size = 0;
	long fpga_data, dev;
	int ret;

	ret = do_fpga_check_params(&dev, &fpga_data, &data_size,
				   FPGA_LOAD_ARGS, gs);
	if (ret)
		return ret;

	return fpga_load(dev, (void *)fpga_data, data_size, BIT_PARTIAL, 0);
}
#endif

#if defined(CONFIG_CMD_FPGA_LOADBP)
static int do_fpga_loadbp(struct getopt_state *gs)
{
	size_t data_size = 0;
	long fpga_data, dev;
	int ret;

	ret = do_fpga_check_params(&dev, &fpga_data, &data_size,
				   FPGA_LOAD_ARGS, gs);
	if (ret)
		return ret;

	return fpga_loadbitstream(dev, (void *)fpga_data, data_size,
				  BIT_PARTIAL);
}
#endif

#if defined(CONFIG_CMD_FPGA_LOADMK)
static int do_fpga_loadmk(struct getopt_state *gs)
{
	char *const *argv = gs->argv + 1;
	int argc = gs->argc - 1;
	size_t data_size = 0;
	void *fpga_data = NULL;
#if defined(CONFIG_FIT)
	const char *fit_uname = NULL;
	ulong fit_addr;
#endif
	ulong dev = do_fpga_get_device(argv[0]);
	char *datastr = env_get("fpgadata");

	log_debug("argc %x, dev %lx, datastr %s\n", argc, dev, datastr);

	if (dev == FPGA_INVALID_DEVICE) {
		log_err("Invalid fpga device\n");
		return CMD_RET_FAILURE;
	}

	if (argc == 0 && !datastr) {
		log_err("No datastr passed\n");
		return CMD_RET_FAILURE;
	}

	if (argc == 2) {
		datastr = argv[1];
		log_debug("Full command with two args\n");
	} else if (argc == 1 && !datastr) {
		log_debug("Dev is setup - fpgadata passed\n");
		datastr = argv[0];
	}

#if defined(CONFIG_FIT)
	if (fit_parse_subimage(datastr, (ulong)fpga_data,
			       &fit_addr, &fit_uname)) {
		fpga_data = (void *)fit_addr;
		log_debug("*  fpga: subimage '%s' from FIT image ",
			  fit_uname);
		log_debug("at 0x%08lx\n", fit_addr);
	} else
#endif
	{
		fpga_data = (void *)hextoul(datastr, NULL);
		log_debug("*  fpga: cmdline image address = 0x%08lx\n",
			  (ulong)fpga_data);
	}
	log_debug("fpga_data = 0x%lx\n", (ulong)fpga_data);
	if (!fpga_data) {
		log_err("Zero fpga_data address\n");
		return CMD_RET_FAILURE;
	}

	switch (genimg_get_format(fpga_data)) {
#if defined(CONFIG_LEGACY_IMAGE_FORMAT)
	case IMAGE_FORMAT_LEGACY:
	{
		struct legacy_img_hdr *hdr = (struct legacy_img_hdr *)fpga_data;
		ulong data;
		u8 comp;

		comp = image_get_comp(hdr);
		if (comp == IH_COMP_GZIP) {
#if defined(CONFIG_GZIP)
			ulong image_buf = image_get_data(hdr);
			ulong image_size = ~0UL;

			data = image_get_load(hdr);

			if (gunzip((void *)data, ~0U, (void *)image_buf,
				   &image_size) != 0) {
				log_err("Gunzip error\n");
				return CMD_RET_FAILURE;
			}
			data_size = image_size;
#else
			log_err("Gunzip image is not supported\n");
			return CMD_RET_FAILURE;
#endif
		} else {
			data = (ulong)image_get_data(hdr);
			data_size = image_get_data_size(hdr);
		}
		return fpga_load(dev, (void *)data, data_size,
				  BIT_FULL, 0);
	}
#endif
#if defined(CONFIG_FIT)
	case IMAGE_FORMAT_FIT:
	{
		const void *fit_hdr = (const void *)fpga_data;
		int err;
		const void *fit_data;

		if (!fit_uname) {
			log_err("No FIT subimage unit name\n");
			return CMD_RET_FAILURE;
		}

		if (fit_check_format(fit_hdr, IMAGE_SIZE_INVAL)) {
			log_err("Bad FIT image format\n");
			return CMD_RET_FAILURE;
		}

		err = fit_get_data_node(fit_hdr, fit_uname, &fit_data,
					&data_size);
		if (err)
			return cmd_fail(err, MSG_SUBIMAGE, fit_uname, err);

		return fpga_load(dev, fit_data, data_size, BIT_FULL, 0);
	}
#endif
	default:
		log_err("Unknown image type\n");
		return CMD_RET_FAILURE;
	}
}
#endif

static struct cmd_tbl fpga_commands[] = {
	U_BOOT_CMD_MKENT_NOOPTS(info, 1, 1, do_fpga_info, "", ""),
	U_BOOT_CMD_MKENT_NOOPTS(dump, FPGA_LOAD_ARGS, 1, do_fpga_dump, "", ""),
	U_BOOT_CMD_MKENT_NOOPTS(load, FPGA_LOAD_ARGS, 1, do_fpga_load, "", ""),
#if defined(CONFIG_CMD_FPGA_LOADB)
	U_BOOT_CMD_MKENT_NOOPTS(loadb, FPGA_LOAD_ARGS, 1, do_fpga_loadb,
				"", ""),
#endif
#if defined(CONFIG_CMD_FPGA_LOADP)
	U_BOOT_CMD_MKENT_NOOPTS(loadp, FPGA_LOAD_ARGS, 1, do_fpga_loadp,
				"", ""),
#endif
#if defined(CONFIG_CMD_FPGA_LOADBP)
	U_BOOT_CMD_MKENT_NOOPTS(loadbp, FPGA_LOAD_ARGS, 1, do_fpga_loadbp,
				"", ""),
#endif
#if defined(CONFIG_CMD_FPGA_LOADFS)
	U_BOOT_CMD_MKENT_NOOPTS(loadfs, FPGA_LOADFS_ARGS, 1, do_fpga_loadfs,
				"", ""),
#endif
#if defined(CONFIG_CMD_FPGA_LOADMK)
	U_BOOT_CMD_MKENT_NOOPTS(loadmk, 2, 1, do_fpga_loadmk, "", ""),
#endif
#if defined(CONFIG_CMD_FPGA_LOAD_SECURE)
	U_BOOT_CMD_MKENT_NOOPTS(loads, FPGA_LOADS_ARGS, 1, do_fpga_loads,
				"", ""),
#endif
};

static int do_fpga_wrapper(struct getopt_state *gs)
{
	int argc = gs->argc;
	char *const *argv = gs->argv;
	struct cmd_tbl *fpga_cmd;
	int ret;

	if (argc < 2)
		return CMD_RET_USAGE;

	fpga_cmd = find_cmd_tbl(argv[1], fpga_commands,
				ARRAY_SIZE(fpga_commands));
	if (!fpga_cmd) {
		log_err("Non existing command\n");
		return CMD_RET_FAILURE;
	}

	if (argc - 2 > fpga_cmd->maxargs) {
		log_err("Too many parameters passed\n");
		return CMD_RET_FAILURE;
	}

	ret = cmd_invoke(fpga_cmd, gs->cmd_flag, argc - 1, argv + 1);

	return cmd_process_error(fpga_cmd, ret);
}

/* loadfs and loads take more arguments than the other sub-commands */
#if defined(CONFIG_CMD_FPGA_LOADFS) || defined(CONFIG_CMD_FPGA_LOAD_SECURE)
#define FPGA_MAXARGS	9
#else
#define FPGA_MAXARGS	6
#endif

U_BOOT_CMD_NOOPTS(fpga, FPGA_MAXARGS, 1, do_fpga_wrapper,
	 "loadable FPGA image support",
	 "info   [dev]                  List known device information\n"
	 "fpga dump   <dev> <address> <size> Load device to memory buffer\n"
	 "fpga load   <dev> <address> <size> Load device from memory buffer\n"
#if defined(CONFIG_CMD_FPGA_LOADB)
	 "fpga loadb  <dev> <address> <size> Load device from bitstream buffer\n"
#endif
#if defined(CONFIG_CMD_FPGA_LOADP)
	 "fpga loadp  <dev> <address> <size> Load device from memory buffer\n"
	 "            with partial bitstream\n"
#endif
#if defined(CONFIG_CMD_FPGA_LOADBP)
	 "fpga loadbp <dev> <address> <size> Load device from bitstream buffer\n"
	 "             with partial bitstream\n"
#endif
#if defined(CONFIG_CMD_FPGA_LOADFS)
	 "fpga loadfs <dev> <address> <size> <blocksize> <interface> [<dev[:part]>] <filename>\n"
	 "            Load device from filesystem (FAT by default)\n"
#endif
#if defined(CONFIG_CMD_FPGA_LOADMK)
	 "fpga loadmk <dev> <address>        Load device generated with mkimage\n"
#if defined(CONFIG_FIT)
	 "            NOTE: loadmk operating on FIT must include subimage unit\n"
	 "            name in the form of addr:<subimg_uname>\n"
#endif
#endif
#if defined(CONFIG_CMD_FPGA_LOAD_SECURE)
	 "fpga loads  <dev> <address> <size> <authflag> <encflag> [Userkey address]\n"
	 "            Load device from memory buffer with secure bistream\n"
	 "            (authenticated/encrypted/both)\n"
	 "            -authflag: 0 for OCM, 1 for DDR, 2 for no authentication\n"
	 "            (specifies where to perform authentication)\n"
	 "            -encflag: 0 for device key, 1 for user key, 2 for no encryption\n"
	 "            -Userkey address: address where user key is stored\n"
	 "            NOTE: secure bitstream has to be created using Xilinx bootgen tool\n"
#endif
);
