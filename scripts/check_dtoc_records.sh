#!/bin/sh
# SPDX-License-Identifier: GPL-2.0+
#
# Check that an of-platdata image has only the driver_info records from dtoc
#
# Copyright 2026 Simon Glass <sjg@chromium.org>
#
# With of-platdata, devices refer to each other, and to their parents, by the
# index of their driver_info record in the linker list. dtoc works out these
# indexes from the records it generates, so any other record moves them and
# breaks the references.
#
# Usage: check_dtoc_records.sh <nm> <elf> <dt-plat.c>

nm="$1"
elf="$2"
plat="$3"

# The records may all be dropped if nothing uses them, e.g. if the phase does
# not use driver model, so only look for records which dtoc did not generate
want=$(sed -n 's/^U_BOOT_DRVINFO(\([A-Za-z0-9_]*\)) = {$/\1/p' "$plat")
got=$($nm "$elf" | sed -n 's/.* _u_boot_list_2_driver_info_2_//p')
[ -z "$got" ] && exit 0

extra=$(echo "$got" | grep -vxF -e "${want:-.}")
if [ -n "$extra" ]; then
	echo "$elf: driver_info records which are not from dtoc:" >&2
	echo "$extra" | sed 's/^/   /' >&2
	echo "With of-platdata, use the devicetree to create devices" >&2
	exit 1
fi
