/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * Linker script for MON, which stays in memory at a fixed address
 *
 * Copyright 2026 Simon Glass <sjg@chromium.org>
 */

#include <asm/psci.h>

MEMORY { .mon_mem : ORIGIN = IMAGE_TEXT_BASE, LENGTH = IMAGE_MAX_SIZE }

OUTPUT_FORMAT("elf64-littleaarch64", "elf64-littleaarch64",
	      "elf64-littleaarch64")
OUTPUT_ARCH(aarch64)
ENTRY(_start)
SECTIONS
{
	.text : {
		KEEP(*(.text.mon_start))
		*(.text*)
		*(._secure.text)
	} > .mon_mem

	.rodata : {
		. = ALIGN(8);
		*(SORT_BY_ALIGNMENT(SORT_BY_NAME(.rodata*)))
	} > .mon_mem

	.data : {
		. = ALIGN(8);
		*(.data*)
		*(._secure.data)
	} > .mon_mem

	/* SiP services, see DECLARE_SECURE_SVC() */
	._secure_svc_tbl_entries : {
		. = ALIGN(8);
		__secure_svc_tbl_start = .;
		KEEP(*(._secure_svc_tbl_entries))
		__secure_svc_tbl_end = .;
	} > .mon_mem

	.bss (NOLOAD) : {
		. = ALIGN(8);
		__bss_start = .;
		*(.bss*)
		*(COMMON)
		. = ALIGN(8);
		__bss_end = .;
	} > .mon_mem

	/* A stack for each CPU, used while handling its SMCs */
	.stacks (NOLOAD) : {
		. = ALIGN(16);
		__secure_stack_start = .;
		. += CONFIG_MON_CPUS << ARM_PSCI_STACK_SHIFT;
		__secure_stack_end = .;
	} > .mon_mem

	/DISCARD/ : {
		*(.rela*)
		*(.dynsym)
		*(.dynstr)
		*(.dynamic)
		*(.gnu.hash)
		*(.interp)
		*(.eh_frame)
	}
}
