# SPDX-License-Identifier: GPL-2.0
#
# (C) Copyright 2012 Stephen Warren

obj-y	:= rpi.o
obj-y	+= lowlevel_init.o

obj-$(CONFIG_GENERATE_ACPI_TABLE) += dsdt_generated.o

# The PEP device is for Windows, which defines the _GPI, _GCI and _GDI names
# and expects a _CRS, although the device has no resources. Tell the compiler
# not to warn about the names (3133) or the empty resource template (3150)
ASLFLAGS_dsdt_generated.c := -vw 3133 -vw 3150
