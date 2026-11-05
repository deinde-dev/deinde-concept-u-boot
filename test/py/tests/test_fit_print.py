# SPDX-License-Identifier: GPL-2.0+
# Copyright 2025 Canonical Ltd
# Written by Simon Glass <simon.glass@canonical.com>

"""Test for FIT image printing"""

import os
import re
import time

import pytest

import fit_util
import utils

# ITS for testing FIT printing with hashes, ramdisk, and multiple configs
PRINT_ITS = '''
/dts-v1/;

/ {
	description = "Test FIT image for printing";
	#address-cells = <1>;

	images {
		kernel {
			description = "Test kernel";
			data = /incbin/("%(kernel)s");
			type = "kernel";
			arch = "sandbox";
			os = "linux";
			compression = "gzip";
			load = <0x1000000>;
			entry = <0x1000000>;
			hash-1 {
				algo = "sha256";
			};
		};
		ramdisk {
			description = "Test ramdisk";
			data = /incbin/("%(ramdisk)s");
			type = "ramdisk";
			arch = "sandbox";
			os = "linux";
			compression = "none";
			load = <0x2000000>;
			hash-1 {
				algo = "sha256";
			};
		};
		fdt-1 {
			description = "Test FDT 1";
			data = /incbin/("%(fdt1)s");
			type = "flat_dt";
			arch = "sandbox";
			compression = "none";
			hash-1 {
				algo = "sha256";
			};
		};
		fdt-2 {
			description = "Test FDT 2";
			data = /incbin/("%(fdt2)s");
			type = "flat_dt";
			arch = "sandbox";
			compression = "none";
			hash-1 {
				algo = "sha256";
			};
		};
		firmware-1 {
			description = "Test Firmware 1";
			data = /incbin/("%(firmware1)s");
			type = "firmware";
			arch = "sandbox";
			compression = "none";
			hash-1 {
				algo = "sha256";
			};
		};
		firmware-2 {
			description = "Test Firmware 2";
			data = /incbin/("%(firmware2)s");
			type = "firmware";
			arch = "sandbox";
			compression = "none";
			hash-1 {
				algo = "sha256";
			};
		};
		fpga {
			description = "Test FPGA";
			data = /incbin/("%(fpga)s");
			type = "fpga";
			arch = "sandbox";
			compression = "none";
			hash-1 {
				algo = "sha256";
			};
		};
		script {
			data = /incbin/("%(script)s");
			type = "script";
			compression = "none";
			hash-1 {
				algo = "sha256";
			};
		};
	};
	configurations {
		default = "conf-1";
		conf-1 {
			description = "Test configuration";
			kernel = "kernel";
			fdt = "fdt-1";
			ramdisk = "ramdisk";
			compatible = "vendor,board-1.0", "vendor,board";
			signature {
				algo = "sha256,rsa2048";
				padding = "pkcs-1.5";
				key-name-hint = "test-key";
				sign-images = "fdt-1", "kernel", "ramdisk";
			};
		};
		conf-2 {
			description = "Alternate configuration";
			kernel = "kernel";
			fdt = "fdt-1", "fdt-2";
			fpga = "fpga";
			loadables = "firmware-1", "firmware-2";
			compatible = "vendor,board-2.0";
		};
		conf-3 {
			loadables = "script";
		};
	};
};
'''

def build_test_fit(ubman, fit):
    """Build a test FIT image with all components

    Args:
        ubman (ConsoleBase): U-Boot manager object
        fit (str): Path where the FIT file should be created
    """
    # pylint: disable=too-many-locals
    mkimage = os.path.join(ubman.config.build_dir, 'tools/mkimage')

    # Create test files (make kernel ~6.3K)
    kernel = fit_util.make_kernel(ubman, 'test-kernel.bin',
                                  'kernel with some extra test data')

    # Compress the kernel (with -n to avoid timestamps for reproducibility)
    kernel_gz = kernel + '.gz'
    utils.run_and_log(ubman, ['gzip', '-f', '-n', '-k', kernel])

    fdt1 = fit_util.make_dtb(ubman, '''
/dts-v1/;
/ {
	#address-cells = <1>;
	#size-cells = <0>;
	model = "Test FDT 1";
};
''', 'test-fdt-1')
    fdt2 = fit_util.make_dtb(ubman, '''
/dts-v1/;
/ {
	#address-cells = <1>;
	#size-cells = <0>;
	model = "Test FDT 2";
};
''', 'test-fdt-2')
    firmware1 = fit_util.make_kernel(ubman, 'test-firmware-1.bin', 'firmware 1')
    firmware2 = fit_util.make_kernel(ubman, 'test-firmware-2.bin', 'firmware 2')
    fpga = fit_util.make_kernel(ubman, 'test-fpga.bin', 'fpga bitstream')
    ramdisk = fit_util.make_kernel(ubman, 'test-ramdisk.bin', 'ramdisk')
    script = fit_util.make_kernel(ubman, 'test-script.bin', 'echo test')

    # Compress the ramdisk (with -n to avoid timestamps for reproducibility)
    ramdisk_gz = ramdisk + '.gz'
    utils.run_and_log(ubman, ['gzip', '-f', '-n', '-k', ramdisk])

    # Create FIT image with fixed timestamp for reproducible output
    params = {
        'kernel': kernel_gz,
        'fdt1': fdt1,
        'fdt2': fdt2,
        'firmware1': firmware1,
        'firmware2': firmware2,
        'fpga': fpga,
        'ramdisk': ramdisk_gz,
        'script': script,
    }
    env = os.environ.copy()
    env['SOURCE_DATE_EPOCH'] = '1234567890'  # 2009-02-13 23:31:30 UTC
    its = fit_util.make_its(ubman, PRINT_ITS, params)
    utils.run_and_log(ubman, [mkimage, '-f', its, fit], env=env)

    # Use a fixed RSA key pair for reproducible signatures
    tmpdir = ubman.config.result_dir + '/'

    # Fixed 2048-bit RSA private key for testing (for reproducible signatures)
    key_pem = '''-----BEGIN PRIVATE KEY-----
MIIEvQIBADANBgkqhkiG9w0BAQEFAASCBKcwggSjAgEAAoIBAQDU18AB+xpQw+GX
mywzH4nsEIECgLVnBTNaAnE4XSIqbiviZetumBP6Ib2W+0OIOn8/hIh3UnzzyWIP
aRus94CVfFQPqwhi6/M9ptL7N7lCXq9DwQ0EY55GquwoO9jZnnDsCSU76jgKg+Nc
dsbvprfFDxBjkrLBfdEgzJtNUaJnUCd58RG8sII7EOP4JTGnXn2wVTsKYcTmr8y6
bOZTUQfsYj9BGFTbskkLYj1RJ6Dpzk4yBqyUn4fUYhfqsAHwlJs/64Byx2m7J7Ia
rfp49NkqgOFlTvDzKnecxGt4pmgEA+4MtRxUFDliZ/bG3TvG/xNlXvWaHp9DG05u
4h9jy2NPAgMBAAECggEAAMZKOheeWGXmF8WmSwdV2qiSt54dSuMvdSfmHpTkL3BY
M4o4aZ4fEH138ak3bTL9TI9gacLAlqiIdVLmGWKLMsARlD8EmEuQhoxpXyWsRGwQ
yjfVIst0A4DSvDC/kMctVQaRfp7TFmK1fJwoDC44o/xyjFI32VFqZeqotAbUhvi3
gIYvP5Q4Kvbaq9aZNURqazJHuEVD9LpwbnroUd4cBrcorstJzaDmTIyb5swLX+IX
FjMOVtHtBDKOG9Ce1wlEOXZtSsoZtAEgkd1IQYBCTBUDkxPdx+ZKPdfT4aKWX3S4
WQ65lDEGAnplMmetFRV+k9NNJvEia9JoX/SJqhUWGQKBgQD1/rffQZnFWqGM2dD1
CEkXpCN23xAEaZjQtuIhPMBWEWufAPZhyZSbq3eLjcqSS5mzU8B+n1c9Zxw6r0qM
BXlcUftreFPKvEXeyp1YWh7loxHiVVuasp2lEDx4arwUrI61XtAaixUb9Opxxj/x
UDrY5cj7BIRhrkDZtnor/EbRaQKBgQDdf9pymbbxRmHHFGERSzo6/gbr1GRK9fUA
ZNrzfBM5Sdvmm2aKgYd7hIKhOgeKIkS858gEOsRw75x6nvlrjZvFZGIfetXXxaN9
c6Uqq/f6rTRUTB9/SqvMgKZMuJ2SFms8I1nbxSE/PMD0T6TRbhjaFoZwZP42HVsM
wAN2Oiq/9wKBgQC7sQHyYkdFgYVJxtfcXdoHI8G7bS73buqeNSwMWCIYiWooA7/5
lKjCre2kmSc6wFwhq4FwG3ug6g9r51tlwrd6bUL8GO81/LkC6G1tgDWa2PVIUAB4
5FfMHbtF1Ypz68VnNVRrLDuK/S/0Z2NaZ/C+lXTnseaf8Sih9Mz6yp3uIQKBgBc4
61cuhH6hSWkM2uxsPaunrGQXPXiadthWupnifUV5V+PCkSqeT+0ERInQwq+Zzikc
B91hp+zLQlWcyzuaeiVk0+DHCRp5Lx3c/QkPRI10kVLxNDAtTPvA1S6gAG0rioyg
jDA9Z7Hwla5Hl1kZuONMj0XDYN+djkk07Gf9yzObAoGAbiS3mRID0pLFhWR1L64h
NlRJpZjsHNRPd0WFVxXnJRzZxkStoTwL2BhPtG3Xx1ReIkNVCxlu1Dk0rLLKl1nj
4B/X9Qu6aejXnOsbqp1/JBXYxD8l5B2yg5//wz18um/SOSagpAPeH4i/V3NxOup5
S0n8gbs0Ht/ZckLk8mPclbk=
-----END PRIVATE KEY-----'''

    with open(tmpdir + 'test-key.key', 'w', encoding='utf-8') as f:
        f.write(key_pem)

    utils.run_and_log(ubman,
        f'openssl req -batch -new -x509 -key {tmpdir}test-key.key '
        f'-out {tmpdir}test-key.crt')

    # Create a dummy DTB for the public key
    dtb = fit_util.make_fname(ubman, 'test-key.dtb')
    utils.run_and_log(ubman, ['dtc', '-I', 'dts', '-O', 'dtb', '-o', dtb],
                      stdin=b'/dts-v1/; / { };')

    # Sign the FIT configuration (use env for reproducible timestamp)
    utils.run_and_log(ubman, [mkimage, '-F', '-k', tmpdir, '-K', dtb,
                              '-r', fit, '-c', 'Configuration signing'],
                      env=env)

    # Delete the algo property from the hash-1 node to test invalid/unsupported
    utils.run_and_log(ubman, ['fdtput', '-d', fit, '/images/script/hash-1',
                              'algo'])


@pytest.mark.boardspec('sandbox')
@pytest.mark.buildconfigspec('fit_print')
@pytest.mark.requiredtool('dtc')
@pytest.mark.requiredtool('openssl')
def test_fit_print(ubman):
    """Test fit_print_contents() via C unit test"""
    fit = os.path.join(ubman.config.persistent_data_dir, 'test-fit.fit')
    build_test_fit(ubman, fit)

    # Run the C test which will load and verify this FIT
    ubman.run_ut('bootstd', 'test_fit_print')


@pytest.mark.boardspec('sandbox')
@pytest.mark.buildconfigspec('fit_print')
@pytest.mark.requiredtool('dtc')
@pytest.mark.requiredtool('openssl')
def test_fit_print_no_desc(ubman):
    """Test fit_print_contents() with missing FIT description"""
    fit = os.path.join(ubman.config.persistent_data_dir, 'test-fit-nodesc.fit')
    build_test_fit(ubman, fit)

    # Delete the description property
    utils.run_and_log(ubman, ['fdtput', '-d', fit, '/', 'description'])

    # Run the C test to check the missing description
    ubman.run_ut('bootstd', 'test_fit_print_no_desc')

@pytest.mark.boardspec('sandbox')
@pytest.mark.buildconfigspec('fit_print')
@pytest.mark.requiredtool('dtc')
@pytest.mark.requiredtool('openssl')
def test_fit_print_mkimage(ubman):
    """Test 'mkimage -l' output on FIT image"""
    mkimage = os.path.join(ubman.config.build_dir, 'tools/mkimage')
    fit = fit_util.make_fname(ubman, 'test-fit-mkimage.fit')
    build_test_fit(ubman, fit)

    # Run mkimage -l and capture output
    output = utils.run_and_log(ubman, [mkimage, '-l', fit])

    # Extract the actual timestamp from mkimage output to avoid timezone issues
    # mkimage uses localtime() which can vary based on system timezone
    match = re.search(r'Created:\s+(.+)', output)
    if not match:
        raise ValueError("Could not find Created: line in mkimage output")
    timestamp_str = match.group(1).strip()

    expected_timestamp = 1234567890
    # Validate timestamp is reasonable (SOURCE_DATE_EPOCH)
    parsed_time = time.strptime(timestamp_str, '%a %b %d %H:%M:%S %Y')
    parsed_timestamp = time.mktime(parsed_time)
    time_diff = abs(parsed_timestamp - expected_timestamp)

    # Check it is within 24 hours (86400 seconds)
    assert time_diff < 86400, \
        f"Timestamp {timestamp_str} is more than 24 hours from expected"

    # Expected output (complete output from mkimage -l)
    expected = f'''
FIT description: Test FIT image for printing
Created:         {timestamp_str}
 Image 0 (kernel)
  Description:   Test kernel
  Created:       {timestamp_str}
  Type:          Kernel Image
  Compression:   gzip compressed
  Data Size:     327 Bytes = 0.32 KiB = 0.00 MiB
  Architecture:  Sandbox
  OS:            Linux
  Load Address:  0x01000000
  Entry Point:   0x01000000
  Hash algo:     sha256
  Hash value:    fad998b94ef12fdac0c347915d8b9b6069a4011399e1a2097638a2cb33244cee
 Image 1 (ramdisk)
  Description:   Test ramdisk
  Created:       {timestamp_str}
  Type:          RAMDisk Image
  Compression:   uncompressed
  Data Size:     301 Bytes = 0.29 KiB = 0.00 MiB
  Architecture:  Sandbox
  OS:            Linux
  Load Address:  0x02000000
  Entry Point:   unavailable
  Hash algo:     sha256
  Hash value:    53e2a65d92ad890dcd89d83a1f95ad6b8206e0e4889548b035062fc494e7f655
 Image 2 (fdt-1)
  Description:   Test FDT 1
  Created:       {timestamp_str}
  Type:          Flat Device Tree
  Compression:   uncompressed
  Data Size:     161 Bytes = 0.16 KiB = 0.00 MiB
  Architecture:  Sandbox
  Hash algo:     sha256
  Hash value:    1264bc4619a1162736fdca8e63e44a1b009fbeaaa259c356b555b91186257ffb
 Image 3 (fdt-2)
  Description:   Test FDT 2
  Created:       {timestamp_str}
  Type:          Flat Device Tree
  Compression:   uncompressed
  Data Size:     161 Bytes = 0.16 KiB = 0.00 MiB
  Architecture:  Sandbox
  Hash algo:     sha256
  Hash value:    3a07e37c76dd48c2a17927981f0959758ac6fd0d649e2032143c5afeea9a98a4
 Image 4 (firmware-1)
  Description:   Test Firmware 1
  Created:       {timestamp_str}
  Type:          Firmware
  Compression:   uncompressed
  Data Size:     3891 Bytes = 3.80 KiB = 0.00 MiB
  Architecture:  Sandbox
  OS:            Unknown OS
  Load Address:  unavailable
  Hash algo:     sha256
  Hash value:    53f1358540a556282764ceaf2912e701d2e25902a6b069b329e57e3c59148414
 Image 5 (firmware-2)
  Description:   Test Firmware 2
  Created:       {timestamp_str}
  Type:          Firmware
  Compression:   uncompressed
  Data Size:     3891 Bytes = 3.80 KiB = 0.00 MiB
  Architecture:  Sandbox
  OS:            Unknown OS
  Load Address:  unavailable
  Hash algo:     sha256
  Hash value:    6a12ac2283f3c9605113b5c2287e983da5671d8d0015381009d75169526676f1
 Image 6 (fpga)
  Description:   Test FPGA
  Created:       {timestamp_str}
  Type:          FPGA Image
  Compression:   uncompressed
  Data Size:     4291 Bytes = 4.19 KiB = 0.00 MiB
  Load Address:  unavailable
  Hash algo:     sha256
  Hash value:    2f588e50e95abc7f9d6afd1d5b3f2bf285cccd55efcf52f47a975dbff3265622
 Image 7 (script)
  Description:   unavailable
  Created:       {timestamp_str}
  Type:          Script
  Compression:   uncompressed
  Data Size:     3791 Bytes = 3.70 KiB = 0.00 MiB
  Hash algo:     invalid/unsupported
 Default Configuration: 'conf-1'
 Configuration 0 (conf-1)
  Description:   Test configuration
  Kernel:        kernel
  Init Ramdisk:  ramdisk
  FDT:           fdt-1
  Compatible:    vendor,board-1.0
                 vendor,board
  Sign algo:     sha256,rsa2048:test-key
  Sign padding:  pkcs-1.5
  Sign value:    abbb7ab19aefac84e238edf13e87f88cf0578cf06155fe2c965a4f2385047a62
  Timestamp:     {timestamp_str}
 Configuration 1 (conf-2)
  Description:   Alternate configuration
  Kernel:        kernel
  FDT:           fdt-1
                 fdt-2
  FPGA:          fpga
  Loadables:     firmware-1
                 firmware-2
  Compatible:    vendor,board-2.0
 Configuration 2 (conf-3)
  Description:   unavailable
  Kernel:        unavailable
  Loadables:     script
'''.strip().split('\n')

    lines = output.split('\n')
    for seq, (expected, line) in enumerate(zip(expected, lines)):
        exp = expected[:80]
        act = line[:80]
        assert exp == act, f"line {seq + 1}: expect '{exp}' got '{act}'"
