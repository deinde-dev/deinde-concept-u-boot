// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright 2008 - 2009 Windriver, <www.windriver.com>
 * Author: Tom Rix <Tom.Rix@windriver.com>
 *
 * (C) Copyright 2014 Linaro, Ltd.
 * Rob Herring <robh@kernel.org>
 */
#include <command.h>
#include <fastboot.h>
#include <getopt.h>
#include <net.h>
#include <string.h>
#include <vsprintf.h>
#include <linux/printk.h>
#include <linux/stringify.h>

#if IS_ENABLED(CONFIG_NET_LEGACY)
static int do_fastboot_udp(int argc, char *const argv[],
			   uintptr_t buf_addr, size_t buf_size)
{
	int err;

	if (!IS_ENABLED(CONFIG_UDP_FUNCTION_FASTBOOT)) {
		pr_err("Fastboot UDP not enabled\n");
		return CMD_RET_FAILURE;
	}

	err = net_loop(FASTBOOT_UDP);

	if (err < 0) {
		printf("fastboot udp error: %d\n", err);
		return CMD_RET_FAILURE;
	}

	return CMD_RET_SUCCESS;
}

static int do_fastboot_tcp(int argc, char *const argv[],
			   uintptr_t buf_addr, size_t buf_size)
{
	int err;

	if (!IS_ENABLED(CONFIG_TCP_FUNCTION_FASTBOOT)) {
		pr_err("Fastboot TCP not enabled\n");
		return CMD_RET_FAILURE;
	}

	err = net_loop(FASTBOOT_TCP);

	if (err < 0) {
		printf("fastboot tcp error: %d\n", err);
		return CMD_RET_FAILURE;
	}

	return CMD_RET_SUCCESS;
}
#endif

static int do_fastboot_usb(int argc, char *const argv[],
			   uintptr_t buf_addr, size_t buf_size)
{
	int controller_index;
	char *usb_controller;
	char *endp;
	int ret;

	if (!IS_ENABLED(CONFIG_USB_FUNCTION_FASTBOOT)) {
		pr_err("Fastboot USB not enabled\n");
		return CMD_RET_FAILURE;
	}

	if (argc < 2)
		return CMD_RET_USAGE;

	usb_controller = argv[1];
	controller_index = simple_strtoul(usb_controller, &endp, 0);
	if (*endp != '\0') {
		pr_err("Error: Wrong USB controller index format\n");
		return CMD_RET_FAILURE;
	}

	ret = fastboot_usb_run(controller_index, (void *)buf_addr, buf_size);

	return ret ? CMD_RET_FAILURE : CMD_RET_SUCCESS;
}

static int do_fastboot(struct getopt_state *gs)
{
	uintptr_t buf_addr = (uintptr_t)NULL;
	size_t buf_size = 0;
	int argc;
	char *const *argv;
	int opt;

	if (gs->argc < 2)
		return CMD_RET_USAGE;

	while ((opt = getopt(gs, "+l:s:")) > 0) {
		switch (opt) {
		case 'l':
			buf_addr = hextoul(gs->arg, NULL);
			break;
		case 's':
			buf_size = hextoul(gs->arg, NULL);
			break;
		default:
			return CMD_RET_USAGE;
		}
	}

	/* Step past the parsed options to the transport/controller args */
	argc = gs->argc - (gs->index - 1);
	argv = &gs->argv[gs->index - 1];

	/* Handle case when USB controller param is just '-' */
	if (argc == 1) {
		pr_err("Error: Incorrect USB controller index\n");
		return CMD_RET_USAGE;
	}

	fastboot_init((void *)buf_addr, buf_size);

#if IS_ENABLED(CONFIG_NET_LEGACY)
	if (!strcmp(argv[1], "udp"))
		return do_fastboot_udp(argc, argv, buf_addr, buf_size);
	if (!strcmp(argv[1], "tcp"))
		return do_fastboot_tcp(argc, argv, buf_addr, buf_size);
#endif
	if (!strcmp(argv[1], "usb")) {
		argv++;
		argc--;
	}

	return do_fastboot_usb(argc, argv, buf_addr, buf_size);
}

U_BOOT_CMD_GETOPT(
	fastboot, CONFIG_SYS_MAXARGS, 1, do_fastboot,
	"run as a fastboot usb or udp device",
	"[-l addr] [-s size] usb <controller> | udp\n"
	"\taddr - address of buffer used during data transfers ("
	__stringify(CONFIG_FASTBOOT_BUF_ADDR) ")\n"
	"\tsize - size of buffer used during data transfers ("
	__stringify(CONFIG_FASTBOOT_BUF_SIZE) ")"
);
