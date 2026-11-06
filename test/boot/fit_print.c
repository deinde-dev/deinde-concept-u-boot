// SPDX-License-Identifier: GPL-2.0+
/*
 * Test for FIT image printing
 *
 * Copyright 2025 Canonical Ltd
 * Written by Simon Glass <simon.glass@canonical.com>
 */

#include <image.h>
#include <mapmem.h>
#include <os.h>
#include <test/ut.h>
#include <linux/libfdt.h>
#include "bootstd_common.h"

/* Test fit_print_contents() output */
static int test_fit_print_norun(struct unit_test_state *uts)
{
	char fname[256];
	void *fit;
	void *buf;
	ulong addr;
	int size;

	/* Load the FIT created by the Python test */
	ut_assertok(os_persistent_file(fname, sizeof(fname), "test-fit.fit"));
	ut_assertok(os_read_file(fname, &buf, &size));

	/* Copy to address 0x10000 and print from there */
	addr = 0x10000;
	fit = map_sysmem(addr, size);
	memcpy(fit, buf, size);

	/* Print it and check output line by line */
	console_record_reset_enable();
	fit_print_contents(fit);

	/* Check every line of output */
	ut_assert_nextline("   FIT description: Test FIT image for printing");
	ut_assert_nextline("   Created:         2009-02-13  23:31:30 UTC");
	ut_assert_nextline("    Image 0 (kernel)");
	ut_assert_nextline("     Description:   Test kernel");
	ut_assert_nextline("     Created:       2009-02-13  23:31:30 UTC");
	ut_assert_nextline("     Type:          Kernel Image");
	ut_assert_nextline("     Compression:   gzip compressed");
	ut_assert_nextline("     Data Start:    0x000100c4");
	ut_assert_nextline("     Data Size:     327 Bytes = 327 Bytes");
	ut_assert_nextline("     Architecture:  Sandbox");
	ut_assert_nextline("     OS:            Linux");
	ut_assert_nextline("     Load Address:  0x01000000");
	ut_assert_nextline("     Entry Point:   0x01000000");
	ut_assert_nextline("     Hash algo:     sha256");
	ut_assert_nextline("     Hash value:    fad998b94ef12fdac0c347915d8b9b6069a4011399e1a2097638a2cb33244cee");
	ut_assert_nextline("    Image 1 (ramdisk)");
	ut_assert_nextline("     Description:   Test ramdisk");
	ut_assert_nextline("     Created:       2009-02-13  23:31:30 UTC");
	ut_assert_nextline("     Type:          RAMDisk Image");
	ut_assert_nextline("     Compression:   uncompressed");
	ut_assert_nextline("     Data Start:    0x00010304");
	ut_assert_nextline("     Data Size:     301 Bytes = 301 Bytes");
	ut_assert_nextline("     Architecture:  Sandbox");
	ut_assert_nextline("     OS:            Linux");
	ut_assert_nextline("     Load Address:  0x02000000");
	ut_assert_nextline("     Entry Point:   unavailable");
	ut_assert_nextline("     Hash algo:     sha256");
	ut_assert_nextline("     Hash value:    53e2a65d92ad890dcd89d83a1f95ad6b8206e0e4889548b035062fc494e7f655");
	ut_assert_nextline("    Image 2 (fdt-1)");
	ut_assert_nextline("     Description:   Test FDT 1");
	ut_assert_nextline("     Created:       2009-02-13  23:31:30 UTC");
	ut_assert_nextline("     Type:          Flat Device Tree");
	ut_assert_nextline("     Compression:   uncompressed");
	ut_assert_nextline("     Data Start:    0x00010518");
	ut_assert_nextline("     Data Size:     161 Bytes = 161 Bytes");
	ut_assert_nextline("     Architecture:  Sandbox");
	ut_assert_nextline("     Hash algo:     sha256");
	ut_assert_nextline("     Hash value:    1264bc4619a1162736fdca8e63e44a1b009fbeaaa259c356b555b91186257ffb");
	ut_assert_nextline("    Image 3 (fdt-2)");
	ut_assert_nextline("     Description:   Test FDT 2");
	ut_assert_nextline("     Created:       2009-02-13  23:31:30 UTC");
	ut_assert_nextline("     Type:          Flat Device Tree");
	ut_assert_nextline("     Compression:   uncompressed");
	ut_assert_nextline("     Data Start:    0x0001067c");
	ut_assert_nextline("     Data Size:     161 Bytes = 161 Bytes");
	ut_assert_nextline("     Architecture:  Sandbox");
	ut_assert_nextline("     Hash algo:     sha256");
	ut_assert_nextline("     Hash value:    3a07e37c76dd48c2a17927981f0959758ac6fd0d649e2032143c5afeea9a98a4");
	ut_assert_nextline("    Image 4 (firmware-1)");
	ut_assert_nextline("     Description:   Test Firmware 1");
	ut_assert_nextline("     Created:       2009-02-13  23:31:30 UTC");
	ut_assert_nextline("     Type:          Firmware");
	ut_assert_nextline("     Compression:   uncompressed");
	ut_assert_nextline("     Data Start:    0x000107e8");
	ut_assert_nextline("     Data Size:     3891 Bytes = 3.8 KiB");
	ut_assert_nextline("     Architecture:  Sandbox");
	ut_assert_nextline("     OS:            Unknown OS");
	ut_assert_nextline("     Load Address:  unavailable");
	ut_assert_nextline("     Hash algo:     sha256");
	ut_assert_nextline("     Hash value:    53f1358540a556282764ceaf2912e701d2e25902a6b069b329e57e3c59148414");
	ut_assert_nextline("    Image 5 (firmware-2)");
	ut_assert_nextline("     Description:   Test Firmware 2");
	ut_assert_nextline("     Created:       2009-02-13  23:31:30 UTC");
	ut_assert_nextline("     Type:          Firmware");
	ut_assert_nextline("     Compression:   uncompressed");
	ut_assert_nextline("     Data Start:    0x000117e8");
	ut_assert_nextline("     Data Size:     3891 Bytes = 3.8 KiB");
	ut_assert_nextline("     Architecture:  Sandbox");
	ut_assert_nextline("     OS:            Unknown OS");
	ut_assert_nextline("     Load Address:  unavailable");
	ut_assert_nextline("     Hash algo:     sha256");
	ut_assert_nextline("     Hash value:    6a12ac2283f3c9605113b5c2287e983da5671d8d0015381009d75169526676f1");
	ut_assert_nextline("    Image 6 (fpga)");
	ut_assert_nextline("     Description:   Test FPGA");
	ut_assert_nextline("     Created:       2009-02-13  23:31:30 UTC");
	ut_assert_nextline("     Type:          FPGA Image");
	ut_assert_nextline("     Compression:   uncompressed");
	ut_assert_nextline("     Data Start:    0x000127e0");
	ut_assert_nextline("     Data Size:     4291 Bytes = 4.2 KiB");
	ut_assert_nextline("     Load Address:  unavailable");
	ut_assert_nextline("     Hash algo:     sha256");
	ut_assert_nextline("     Hash value:    2f588e50e95abc7f9d6afd1d5b3f2bf285cccd55efcf52f47a975dbff3265622");
	ut_assert_nextline("    Image 7 (script)");
	ut_assert_nextline("     Description:   unavailable");
	ut_assert_nextline("     Created:       2009-02-13  23:31:30 UTC");
	ut_assert_nextline("     Type:          Script");
	ut_assert_nextline("     Compression:   uncompressed");
	ut_assert_nextline("     Data Start:    0x0001394c");
	ut_assert_nextline("     Data Size:     3791 Bytes = 3.7 KiB");
	ut_assert_nextline("     Hash algo:     invalid/unsupported");
	ut_assert_nextline("    Default Configuration: 'conf-1'");
	ut_assert_nextline("    Configuration 0 (conf-1)");
	ut_assert_nextline("     Description:   Test configuration");
	ut_assert_nextline("     Kernel:        kernel");
	ut_assert_nextline("     Init Ramdisk:  ramdisk");
	ut_assert_nextline("     FDT:           fdt-1");
	ut_assert_nextline("     Compatible:    vendor,board-1.0");
	ut_assert_nextline("                    vendor,board");
	ut_assert_nextline("     Sign algo:     sha256,rsa2048:test-key");
	ut_assert_nextline("     Sign padding:  pkcs-1.5");
	ut_assert_nextlinen("     Sign value:    abbb7ab19aefac84e238");
	ut_assert_nextline("     Timestamp:     2009-02-13  23:31:30 UTC");
	ut_assert_nextline("    Configuration 1 (conf-2)");
	ut_assert_nextline("     Description:   Alternate configuration");
	ut_assert_nextline("     Kernel:        kernel");
	ut_assert_nextline("     FDT:           fdt-1");
	ut_assert_nextline("                    fdt-2");
	ut_assert_nextline("     FPGA:          fpga");
	ut_assert_nextline("     Loadables:     firmware-1");
	ut_assert_nextline("                    firmware-2");
	ut_assert_nextline("     Compatible:    vendor,board-2.0");
	ut_assert_nextline("    Configuration 2 (conf-3)");
	ut_assert_nextline("     Description:   unavailable");
	ut_assert_nextline("     Kernel:        unavailable");
	ut_assert_nextline("     Loadables:     script");
	ut_assert_console_end();

	os_free(buf);

	return 0;
}
BOOTSTD_TEST(test_fit_print_norun, UTF_CONSOLE | UTF_MANUAL);

/* Test fit_print_contents() with missing FIT description */
static int test_fit_print_no_desc_norun(struct unit_test_state *uts)
{
	char fname[256];
	void *fit;
	void *buf;
	ulong addr;
	int size;

	/* Load the FIT created by the Python test (with deleted description) */
	ut_assertok(os_persistent_file(fname, sizeof(fname),
				       "test-fit-nodesc.fit"));
	ut_assertok(os_read_file(fname, &buf, &size));

	/* Copy to address 0x10000 and print from there */
	addr = 0x10000;
	fit = map_sysmem(addr, size);
	memcpy(fit, buf, size);

	/* Print it and check just the first line */
	console_record_reset_enable();
	fit_print_contents(fit);

	/* Check the first line shows unavailable */
	ut_assert_nextline("   FIT description: unavailable");

	os_free(buf);

	return 0;
}
BOOTSTD_TEST(test_fit_print_no_desc_norun, UTF_CONSOLE | UTF_MANUAL);
