# SPDX-License-Identifier: GPL-2.0+
# Settings for the Avnet Ultra96-V2 in the lab
#
# The board has no TPM fitted, although the xilinx_zynqmp_virt config binds
# tpm_tis_spi, so skip the tests which need a device

env__tpm_device_test_skip = True
