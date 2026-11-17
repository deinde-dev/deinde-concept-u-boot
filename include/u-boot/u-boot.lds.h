/*
 * Linker script helper macros
 *
 * Copyright (c) 2009 Analog Devices Inc.
 *
 * Licensed under the GPL-2 or later.
 */

#ifndef __U_BOOT_LDS__
#define __U_BOOT_LDS__

/* See if the linker version is at least the specified version */
#define LD_AT_LEAST(major, minor) \
	((major > LD_MAJOR) || (major == LD_MAJOR && minor <= LD_MINOR))

/*
 * Linker versions prior to 2.16 don't understand the builtin
 * functions SORT_BY_ALIGNMENT() and SORT_BY_NAME(), so disable these
 */
#if !LD_AT_LEAST(2, 16)
# define SORT_BY_ALIGNMENT(x) x
# define SORT_BY_NAME(x) x
#endif

/*
 * U_BOOT_LISTS - Input sections for the linker lists
 *
 * Every entry is kept, since nothing refers to most of them directly. The
 * exception is a phase which uses of-platdata without a devicetree: each
 * device there refers to its driver, so a driver which nothing refers to is
 * left out.
 *
 * KEEP() applies to every section which its pattern matches, even one which an
 * earlier statement has already placed, and a pattern cannot exclude a name. So
 * in that case the driver list comes first, without KEEP(), followed by
 * patterns which between them match every list except 'driver'. Each list is
 * still contiguous and sorted, which is all that the code relies on.
 */
#if defined(CONFIG_OF_PLATDATA) && !defined(CONFIG_OF_REAL)
#define U_BOOT_LISTS \
	*(SORT(__u_boot_list_2_driver_[123]*)) \
	KEEP(*(SORT(__u_boot_list_2_[!d]*))) \
	KEEP(*(SORT(__u_boot_list_2_d[!r]*))) \
	KEEP(*(SORT(__u_boot_list_2_dr[!i]*))) \
	KEEP(*(SORT(__u_boot_list_2_dri[!v]*))) \
	KEEP(*(SORT(__u_boot_list_2_driv[!e]*))) \
	KEEP(*(SORT(__u_boot_list_2_drive[!r]*))) \
	KEEP(*(SORT(__u_boot_list_2_driver[!_]*))) \
	KEEP(*(SORT(__u_boot_list_2_driver_[!123]*)))
#else
#define U_BOOT_LISTS	KEEP(*(SORT(__u_boot_list*)))
#endif

#endif
