// SPDX-License-Identifier: GPL-2.0+
/*
 * (C) Copyright 2000-2009
 * Wolfgang Denk, DENX Software Engineering, wd@denx.de.
 */

#include <bootm.h>
#include <command.h>
#include <env.h>
#include <getopt.h>
#include <image.h>
#include <irq_func.h>
#include <lmb.h>
#include <log.h>
#include <mapmem.h>
#include <asm/global_data.h>
#include <linux/kernel.h>
#include <linux/sizes.h>

DECLARE_GLOBAL_DATA_PTR;
/**
 * decomp_direct() - Try to decompress an Image straight to its final address
 *
 * This works when the compressed data records its uncompressed size, so that
 * the space can be reserved before decompressing, avoiding any later copy.
 *
 * @ctype: Compression type (IH_COMP_...)
 * @ld: Address of the compressed Image
 * @comp_len: Length of the compressed data
 * @destp: Returns the address of the decompressed Image
 * @sizep: Returns the uncompressed size, as passed to booti_alloc()
 * Return: 0 if OK, -ve if the size is not known, no space could be found or
 * decompression failed, in which case kernel_comp_addr_r should be used
 */
static int decomp_direct(int ctype, ulong ld, ulong comp_len, ulong *destp,
			 ulong *sizep)
{
	ulong dest, size, end;
	int ret;

	ret = image_decomp_size(ctype, map_sysmem(ld, 0), comp_len, &size);
	if (ret)
		return ret;
	ret = booti_alloc(size, &dest);
	if (ret)
		return ret;
	ret = image_decomp(ctype, dest, ld, IH_TYPE_KERNEL,
			   map_sysmem(dest, size), map_sysmem(ld, 0), comp_len,
			   size, &end);
	if (ret) {
		lmb_free(dest, size);
		return ret;
	}
	*destp = dest;
	*sizep = size;

	return 0;
}

/*
 * Image booting support
 */
static int booti_start(struct bootm_info *bmi)
{
	struct bootm_headers *images = bmi->images;
	int ret;
	ulong ld;
	ulong relocated_addr;
	ulong image_size;
	uint8_t *temp;
	ulong dest;
	ulong dest_end;
	unsigned long comp_len;
	unsigned long decomp_len;
	bool placed = false;
	int ctype;

	ret = bootm_run_states(bmi, BOOTM_STATE_START);

	/* Setup Linux kernel Image entry point */
	if (!bmi->addr_img) {
		ld = image_load_addr;
		debug("*  kernel: default image load address = 0x%08lx\n",
				image_load_addr);
	} else {
		ld = hextoul(bmi->addr_img, NULL);
		debug("*  kernel: cmdline image address = 0x%08lx\n", ld);
	}

	temp = map_sysmem(ld, 0);
	ctype = image_decomp_type(temp, 2);
	if (ctype > 0) {
		comp_len = env_get_ulong("kernel_comp_size", 16, 0);
		if (!comp_len) {
			puts("kernel_comp_size is not provided!\n");
			return -EINVAL;
		}

		/*
		 * Put the Image straight where it will run from if possible,
		 * otherwise decompress it to kernel_comp_addr_r and let
		 * booti_setup() move it if it needs to
		 */
		if (!decomp_direct(ctype, ld, comp_len, &dest, &decomp_len)) {
			placed = true;
		} else {
			dest = env_get_ulong("kernel_comp_addr_r", 16, 0);
			if (!dest) {
				puts("kernel_comp_addr_r is not provided!\n");
				return -EINVAL;
			}
			if (dest < gd->ram_base || dest > gd->ram_top) {
				puts("kernel_comp_addr_r is outside of DRAM range!\n");
				return -EINVAL;
			}

			debug("kernel image compression type %d size = 0x%08lx address = 0x%08lx\n",
			      ctype, comp_len, (ulong)dest);
			decomp_len = comp_len * 10;
			ret = image_decomp(ctype, dest, ld, IH_TYPE_KERNEL,
					   (void *)dest, (void *)ld, comp_len,
					   decomp_len, &dest_end);
			if (ret)
				return ret;
		}
		ld = dest;
	}
	unmap_sysmem(temp);

	if (placed)
		ret = booti_check(ld, decomp_len, &relocated_addr, &image_size);
	else
		ret = booti_setup(ld, &relocated_addr, &image_size, false);
	if (ret)
		return 1;

	/* Handle BOOTM_STATE_LOADOS */
	if (relocated_addr != ld) {
		printf("Moving Image from %lx to %lx, end %lx\n", ld,
		       relocated_addr, relocated_addr + image_size);
		memmove((void *)relocated_addr, (void *)ld, image_size);
	}

	images->ep = relocated_addr;
	images->os.start = relocated_addr;
	images->os.end = relocated_addr + image_size;

	lmb_reserve(images->ep, le32_to_cpu(image_size), LMB_NONE);

	/*
	 * Handle the BOOTM_STATE_FINDOTHER state ourselves as we do not
	 * have a header that provide this informaiton.
	 */
	if (bootm_find_images(image_load_addr, bmi->conf_ramdisk, bmi->conf_fdt,
			      relocated_addr, image_size))
		return 1;

	return 0;
}

int do_booti(struct getopt_state *gs)
{
	int argc = gs->argc;
	char *const *argv = gs->argv;
	struct bootm_info bmi;
	int states;
	int ret;

	/* Consume 'booti' */
	argc--; argv++;

	bootm_init(&bmi);
	bootm_read_env(&bmi);
	if (argc)
		bmi.addr_img = argv[0];
	if (argc > 1)
		bmi.conf_ramdisk = argv[1];
	if (argc > 2)
		bmi.conf_fdt = argv[2];
	bmi.boot_progress = true;
	bmi.cmd_name = "booti";
	bmi.ignore_bootm_len = true;
	/* do not set up argc and argv[] since nothing uses them */

	if (booti_start(&bmi))
		return 1;

	/*
	 * We are doing the BOOTM_STATE_LOADOS state ourselves, so must
	 * disable interrupts ourselves
	 */
	bootm_disable_interrupts();

	images.os.os = IH_OS_LINUX;
	if (IS_ENABLED(CONFIG_RISCV_SMODE))
		images.os.arch = IH_ARCH_RISCV;
	else if (IS_ENABLED(CONFIG_ARM64))
		images.os.arch = IH_ARCH_ARM64;

	states = BOOTM_STATE_MEASURE | BOOTM_STATE_OS_PREP |
		BOOTM_STATE_OS_FAKE_GO | BOOTM_STATE_OS_GO;
	if (IS_ENABLED(CONFIG_SYS_BOOT_RAMDISK_HIGH))
		states |= BOOTM_STATE_RAMDISK;

	ret = bootm_run_states(&bmi, states);

	return ret;
}

U_BOOT_LONGHELP(booti,
	"[addr [initrd[:size]] [fdt]]\n"
	"    - boot Linux flat or compressed 'Image' stored at 'addr'\n"
	"\tThe argument 'initrd' is optional and specifies the address\n"
	"\tof an initrd in memory. The optional parameter ':size' allows\n"
	"\tspecifying the size of a RAW initrd.\n"
	"\tCurrently only booting from gz, bz2, lzma and lz4 compression\n"
	"\ttypes are supported. In order to boot from any of these compressed\n"
	"\timages, user have to set kernel_comp_addr_r and kernel_comp_size environment\n"
	"\tvariables beforehand.\n"
#if defined(CONFIG_OF_LIBFDT)
	"\tSince booting a Linux kernel requires a flat device-tree, a\n"
	"\tthird argument providing the address of the device-tree blob\n"
	"\tis required. To boot a kernel with a device-tree blob but\n"
	"\twithout an initrd image, use a '-' for the initrd argument.\n"
#endif
	);

U_BOOT_CMD_NOOPTS(
	booti,	CONFIG_SYS_MAXARGS,	1,	do_booti,
	"boot Linux kernel 'Image' format from memory", booti_help_text
);
