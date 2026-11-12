// SPDX-License-Identifier: GPL-2.0+

#include <command.h>
#include <display_options.h>
#include <dm.h>
#include <getopt.h>
#include <hexdump.h>
#include <i2c.h>
#include <mapmem.h>
#include <rtc.h>

#define MAX_RTC_BYTES 32

static int do_rtc_read(struct udevice *dev, int argc, char * const argv[])
{
	u8 buf[MAX_RTC_BYTES];
	int reg, len, ret, r;

	if (argc < 2 || argc > 3)
		return CMD_RET_USAGE;

	reg = hextoul(argv[0], NULL);
	len = hextoul(argv[1], NULL);

	if (argc == 3) {
		u8 *addr;

		addr = map_sysmem(hextoul(argv[2], NULL), len);
		ret = dm_rtc_read(dev, reg, addr, len);
		unmap_sysmem(addr);
		if (ret)
			return cmd_fail(ret, "dm_rtc_read() failed: %d\n", ret);
		return CMD_RET_SUCCESS;
	}

	while (len) {
		r = min_t(int, len, sizeof(buf));
		ret = dm_rtc_read(dev, reg, buf, r);
		if (ret)
			return cmd_fail(ret, "dm_rtc_read() failed: %d\n", ret);
		print_buffer(reg, buf, 1, r, 0);
		len -= r;
		reg += r;
	}

	return CMD_RET_SUCCESS;
}

static int do_rtc_write(struct udevice *dev, int argc, char * const argv[])
{
	u8 buf[MAX_RTC_BYTES];
	int reg, len, ret;
	const char *s;
	int slen;

	if (argc < 2 || argc > 3)
		return CMD_RET_USAGE;

	reg = hextoul(argv[0], NULL);

	if (argc == 3) {
		u8 *addr;

		len = hextoul(argv[1], NULL);
		addr = map_sysmem(hextoul(argv[2], NULL), len);
		ret = dm_rtc_write(dev, reg, addr, len);
		unmap_sysmem(addr);
		if (ret)
			return cmd_fail(ret, "dm_rtc_write() failed: %d\n", ret);
		return CMD_RET_SUCCESS;
	}

	s = argv[1];
	slen = strlen(s);

	if (slen % 2)
		return cmd_fail(0, "invalid hex string\n");

	while (slen) {
		len = min_t(int, slen / 2, sizeof(buf));
		if (hex2bin(buf, s, len))
			return cmd_fail(0, "invalid hex string\n");

		ret = dm_rtc_write(dev, reg, buf, len);
		if (ret)
			return cmd_fail(ret, "dm_rtc_write() failed: %d\n", ret);
		s += 2 * len;
		slen -= 2 * len;
	}

	return CMD_RET_SUCCESS;
}

int do_rtc(struct getopt_state *gs)
{
	static int curr_rtc;
	char *const *argv = gs->argv;
	int argc = gs->argc;
	struct udevice *dev;
	int ret, idx;

	if (argc < 2)
		return CMD_RET_USAGE;

	argc--;
	argv++;

	if (!strcmp(argv[0], "list")) {
		struct uclass *uc;

		idx = 0;
		uclass_id_foreach_dev(UCLASS_RTC, dev, uc) {
			printf("RTC #%d - %s\n", idx++, dev->name);
		}
		if (!idx)
			return cmd_fail(0, "*** no RTC devices available ***\n");
		return CMD_RET_SUCCESS;
	}

	idx = curr_rtc;
	if (!strcmp(argv[0], "dev") && argc >= 2)
		idx = dectoul(argv[1], NULL);

	ret = uclass_get_device(UCLASS_RTC, idx, &dev);
	if (ret)
		return cmd_fail(ret, "Cannot find RTC #%d: err=%d\n", idx, ret);

	if (!strcmp(argv[0], "dev")) {
		/* Show the existing or newly selected RTC */
		if (argc >= 2)
			curr_rtc = idx;
		printf("RTC #%d - %s\n", idx, dev->name);
		return CMD_RET_SUCCESS;
	}

	if (!strcmp(argv[0], "read"))
		return do_rtc_read(dev, argc - 1, argv + 1);

	if (!strcmp(argv[0], "write"))
		return do_rtc_write(dev, argc - 1, argv + 1);

	return CMD_RET_USAGE;
}

U_BOOT_CMD_NOOPTS(
	rtc,	5,	0,	do_rtc,
	"RTC subsystem",
	"list                        - show available rtc devices\n"
	"rtc dev [n]                     - show or set current rtc device\n"
	"rtc read <reg> <count>          - read and display 8-bit registers starting at <reg>\n"
	"rtc read <reg> <count> <addr>   - read 8-bit registers starting at <reg> to memory <addr>\n"
	"rtc write <reg> <hexstring>     - write 8-bit registers starting at <reg>\n"
	"rtc write <reg> <count> <addr>  - write from memory <addr> to 8-bit registers starting at <reg>\n"
);
