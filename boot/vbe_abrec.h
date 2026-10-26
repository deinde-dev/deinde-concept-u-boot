/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Verified Boot for Embedded (VBE) vbe-abrec common file
 *
 * Copyright 2024 Google LLC
 * Written by Simon Glass <sjg@chromium.org>
 */

#ifndef __VBE_ABREC_H
#define __VBE_ABREC_H

#include <pxe_utils.h>

#include <vbe.h>
#include <dm/ofnode_decl.h>

#include "vbe_common.h"

struct bootflow;
struct udevice;

#define VBE_STATE_FNAME		"vbe-state"
#define VBE_OEM_FIT_FNAME	"oem.fit"

/**
 * struct abrec_priv - information read from the device tree
 *
 * @area_start: Start offset of the VBE area in the device, in bytes
 * @area_size: Total size of the VBE area
 * @skip_offset: Size of an initial part of the device to skip, when using
 *	area_start. This is effectively added to area_start to calculate the
 *	actual start position on the device
 * @state_offset: Offset from area_start of the VBE state, in bytes
 * @state_size: Size of the state information
 * @version_offset: Offset from area_start of the VBE version info, used
 *	if there is no slot info
 * @version_size: Size of the version info, or 0 if none
 * @slot_a: Offset of the A firmware in the device, in bytes (U-Boot proper only)
 * @slot_b: Offset of the B firmware (U-Boot proper only)
 * @slot_recovery: Offset of the recovery firmware (U-Boot proper only)
 * @slot_info_offset: Offset of the slot info within each slot
 * @slot_info_size: Size of the slot info, or 0 if there is none
 * @has_recovery: true if there is recovery firmware (U-Boot proper only)
 * @storage: Storage device to use, in the form <uclass><devnum>, e.g. "mmc1"
 * @oem_devicetree: true if we should read an OEM devicetree
 * @ctx: PXE context for extlinux boot/read_all
 */
struct abrec_priv {
	u32 area_start;
	u32 area_size;
	u32 skip_offset;
	u32 state_offset;
	u32 state_size;
	u32 version_offset;
	u32 version_size;
	u32 slot_a;
	u32 slot_b;
	u32 slot_recovery;
	u32 slot_info_offset;
	u32 slot_info_size;
	bool has_recovery;
	const char *storage;
	bool oem_devicetree;
	struct pxe_context ctx;
};

/** struct abrec_state - state information read from media
 *
 * The state on the media is converted into this more code-friendly structure.
 *
 * @fw_version: Firmware version string
 * @fw_vernum: Firmware version number
 * @try_count: Number of times the B firmware has been tried
 * @try_b: true to try B firmware on the next boot
 * @recovery: true to enter recovery firmware on the next boot
 * @try_result: Result of trying to boot with the last firmware
 * @pick: Firmware which was chosen in this boot
 */
struct abrec_state {
	char fw_version[MAX_VERSION_LEN];
	u32 fw_vernum;
	u8 try_count;
	bool try_b;
	bool recovery;
	enum vbe_try_result try_result;
	enum vbe_pick_t pick;
};

/**
 * abrec_read_fw_bootflow() - Read a bootflow for firmware
 *
 * Locates and loads the firmware image (FIT) needed for the next phase. The FIT
 * should ideally use external data, to reduce the amount of it that needs to be
 * read.
 *
 * @bdev: bootdev device containing the firmwre
 * @bflow: Place to put the created bootflow, on success
 * @return 0 if OK, -ve on error
 */
int abrec_read_bootflow_fw(struct udevice *dev, struct bootflow *bflow);

/**
 * vbe_simple_read_state() - Read the VBE simple state information
 *
 * @dev: VBE bootmeth
 * @state: Place to put the state
 * @return 0 if OK, -ve on error
 */
int abrec_read_state(struct udevice *dev, struct abrec_state *state);

/**
 * abrec_read_nvdata() - Read non-volatile data from a block device
 *
 * Reads the ABrec VBE nvdata from a device. This function reads a single block
 * from the device, so the nvdata cannot be larger than that.
 *
 * @blk: Device to read from
 * @offset: Offset to read, in bytes
 * @size: Number of bytes to read
 * @buf: Buffer to hold the data
 * Return: 0 if OK, -E2BIG if @size > block size, -EBADF if the offset is not
 * block-aligned, -EIO if an I/O error occurred, -EPERM if the header version is
 * incorrect, the header size is invalid or the data fails its CRC check
 */
int abrec_read_nvdata(struct abrec_priv *priv, struct udevice *blk,
		      struct abrec_state *state);

/**
 * abrec_write_nvdata() - Write the state back to a block device
 *
 * Encodes the try count, flags, try result, pick and firmware version number
 * into the nvdata, then writes it with a fresh header and CRC
 *
 * @priv: Information about the VBE area
 * @blk: Device to write to
 * @state: State to write
 * Return: 0 if OK, -ve on error (see vbe_write_nvdata())
 */
int abrec_write_nvdata(struct abrec_priv *priv, struct udevice *blk,
		       const struct abrec_state *state);

/**
 * abrec_choose() - Choose which firmware to boot
 *
 * Recovery is used if requested and there is recovery firmware. Otherwise B is
 * used while it has tries left, using up one try each time and marking the
 * result as 'trying'. Once the tries run out without the OS marking B as good
 * (by clearing @try_b), B is marked bad and A is used. Otherwise A is used.
 *
 * @state: State read from the media, updated with the new state
 * @has_recovery: true if there is recovery firmware
 * @changedp: Returns true if @state changed and needs to be written back
 * Return: Firmware to boot
 */
enum vbe_pick_t abrec_choose(struct abrec_state *state, bool has_recovery,
			     bool *changedp);

/**
 * abrec_get_pick() - Find out which firmware VPL chose for this boot
 *
 * Return: Firmware picked, or VBEP_COUNT if not known
 */
enum vbe_pick_t abrec_get_pick(void);

/**
 * abrec_fallback() - Choose the firmware to use when the pick cannot be loaded
 *
 * A B which cannot be loaded is marked bad straight away, rather than using up
 * its tries, and A is used instead. An A which cannot be loaded falls back to
 * recovery, if there is any. There is nothing to fall back to from recovery.
 *
 * @state: Current state, updated with the new state
 * @failed: Firmware which could not be loaded
 * @has_recovery: true if there is recovery firmware
 * @changedp: Set to true if @state changed and needs to be written back; left
 *	alone otherwise
 * Return: Firmware to try next, or VBEP_COUNT if there is none
 */
enum vbe_pick_t abrec_fallback(struct abrec_state *state,
			       enum vbe_pick_t failed, bool has_recovery,
			       bool *changedp);

/**
 * abrec_fixup_node() - Add the current state to a node for the OS
 *
 * Writes 'cur-version' (if the version block holds a valid string),
 * 'cur-vernum', 'bootloader-version' and, if @pick is valid, 'booted-slot'
 * ("a", "b" or "recovery")
 *
 * @node: Node to update, typically /chosen/fwupd/firmware0
 * @state: Current state
 * @pick: Firmware which was booted, or VBEP_COUNT if not known
 * Return: 0 if OK, -ve on error
 */
int abrec_fixup_node(ofnode node, const struct abrec_state *state,
		     enum vbe_pick_t pick);

/**
 * abrec_add_compatible() - Give the fwupd node the board's compatible strings
 *
 * fwupd uses the 'compatible' property of /chosen/fwupd to identify the board,
 * so copy it from the root node of @tree, unless @node already has one
 *
 * @tree: Tree being fixed up
 * @node: /chosen/fwupd node in @tree
 * Return: 0 if OK, -ve on error
 */
int abrec_add_compatible(oftree tree, ofnode node);

/**
 * abrec_read_priv() - Read info from the devicetree
 *
 * @node: Node to read from
 * @priv: Information to fill in
 * Return 0 if OK, -EINVAL if something is wrong with the devicetree node
 */
int abrec_read_priv(ofnode node, struct abrec_priv *priv);

#endif /* __VBE_ABREC_H */
