#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0+
#
# Copyright 2026 Simon Glass <sjg@chromium.org>

"""Sort the linker-list data segments of a relocatable WebAssembly object

U-Boot's linker lists put each entry in its own section, with a name like
__u_boot_list_2_driver_2_sandbox_serial, and rely on the linker script to
sort those sections by name, so that the entries of each list form an
array, in order, between its start and end markers.

wasm-ld has no linker scripts. It lays out data segments in the order in
which they appear in its input. So the sandbox WebAssembly build links all
of U-Boot into a single relocatable object first, then uses this tool to
move the linker-list segments to the end of the data section, sorted by
name. The final link then lays them out in that order.

Moving a segment changes its index, so the symbol table and any COMDAT
entries are updated to match. Relocations for the data section hold an
offset into that section, so these are updated too.
"""

import argparse
import io
import sys

# The file starts with the magic number '\0asm' and a 32-bit version
HEADER_SIZE = 8

# Section IDs
SEC_CUSTOM = 0
SEC_DATA = 11

# Flags at the start of each data segment
SEG_PASSIVE = 1         # no init expression, since it is not placed in memory
SEG_MEM_INDEX = 2       # has an explicit memory index

# Opcodes which can appear in the init expression of a data segment
OP_GLOBAL_GET = 0x23
OP_I32_CONST = 0x41
OP_I64_CONST = 0x42
OP_END = 0x0b

# Subsections of the 'linking' custom section
WASM_SEGMENT_INFO = 5
WASM_COMDAT_INFO = 7
WASM_SYMBOL_TABLE = 8

# Symbol kinds and flags
SYMTAB_FUNCTION = 0
SYMTAB_DATA = 1
SYMTAB_GLOBAL = 2
SYMTAB_SECTION = 3
SYMTAB_TAG = 4
SYMTAB_TABLE = 5
SYM_UNDEFINED = 0x10
SYM_EXPLICIT_NAME = 0x40

COMDAT_DATA = 0

# WebAssembly relocation types which have an addend. Each relocation is a
# type, an offset and an index, followed by a signed LEB128 addend for these
# types only, so the addend must be skipped when reading the others. These
# are the types which refer to a memory address or an offset into a function
# or section. The others refer to an index into a table, such as a function
# or global index, so have no addend. See relocTypeHasAddend() in LLVM's
# lib/BinaryFormat/Wasm.cpp and the list in WasmRelocs.def
RELOCS_WITH_ADDEND = {
    3,      # R_WASM_MEMORY_ADDR_LEB
    4,      # R_WASM_MEMORY_ADDR_SLEB
    5,      # R_WASM_MEMORY_ADDR_I32
    8,      # R_WASM_FUNCTION_OFFSET_I32
    9,      # R_WASM_SECTION_OFFSET_I32
    11,     # R_WASM_MEMORY_ADDR_REL_SLEB
    14,     # R_WASM_MEMORY_ADDR_LEB64
    15,     # R_WASM_MEMORY_ADDR_SLEB64
    16,     # R_WASM_MEMORY_ADDR_I64
    17,     # R_WASM_MEMORY_ADDR_REL_SLEB64
    21,     # R_WASM_MEMORY_ADDR_TLS_SLEB
    22,     # R_WASM_FUNCTION_OFFSET_I64
    23,     # R_WASM_MEMORY_ADDR_LOCREL_I32
    25,     # R_WASM_MEMORY_ADDR_TLS_SLEB64
}


class Reader(io.BytesIO):
    """Reads WebAssembly values from a bytes object"""
    def __init__(self, data):
        super().__init__(data)
        self.size = len(data)

    def byte(self):
        """Read a single byte"""
        return self.raw(1)[0]

    def uleb(self):
        """Read an unsigned LEB128 value"""
        result = 0
        shift = 0
        while True:
            val = self.byte()
            result |= (val & 0x7f) << shift
            shift += 7
            if not val & 0x80:
                return result

    def sleb(self):
        """Read a signed LEB128 value"""
        result = 0
        shift = 0
        while True:
            val = self.byte()
            result |= (val & 0x7f) << shift
            shift += 7
            if not val & 0x80:
                if val & 0x40:
                    result -= 1 << shift
                return result

    def raw(self, size):
        """Read a number of bytes"""
        val = self.read(size)
        if len(val) != size:
            raise ValueError('Unexpected end of data')
        return val

    def name(self):
        """Read a string, returned as bytes"""
        return self.raw(self.uleb())

    def done(self):
        """Check whether all the data has been read"""
        return self.tell() >= self.size


def uleb(val):
    """Encode an unsigned LEB128 value"""
    out = bytearray()
    while True:
        byte = val & 0x7f
        val >>= 7
        if val:
            out.append(byte | 0x80)
        else:
            out.append(byte)
            return bytes(out)


def sleb(val):
    """Encode a signed LEB128 value"""
    out = bytearray()
    while True:
        byte = val & 0x7f
        val >>= 7
        if (not val and not byte & 0x40) or (val == -1 and byte & 0x40):
            out.append(byte)
            return bytes(out)
        out.append(byte | 0x80)


def name(val):
    """Encode a string"""
    return uleb(len(val)) + val


def read_sections(data):
    """Split a WebAssembly module into its sections

    Returns:
        list of [id, payload]
    """
    if data[:4] != b'\0asm':
        raise ValueError('Not a WebAssembly file')
    rdr = Reader(data)
    rdr.seek(HEADER_SIZE)
    sections = []
    while not rdr.done():
        sec_id = rdr.byte()
        sections.append([sec_id, rdr.raw(rdr.uleb())])
    return sections


def custom_name(payload):
    """Get the name of a custom section and its contents"""
    rdr = Reader(payload)
    sec_name = rdr.name()
    return sec_name, payload[rdr.tell():]


def read_segments(payload):
    """Read the segments of the data section

    Returns:
        tuple:
            bytes: The count at the start of the section
            list of (offset, record): Offset of each segment record within
                the section and its bytes
    """
    rdr = Reader(payload)
    count = rdr.uleb()
    header = payload[:rdr.tell()]
    segs = []
    for _ in range(count):
        start = rdr.tell()
        flags = rdr.uleb()
        if flags & SEG_MEM_INDEX:
            rdr.uleb()
        if not flags & SEG_PASSIVE:
            # skip the init expression, which is a constant then 'end'
            opcode = rdr.byte()
            if opcode not in (OP_I32_CONST, OP_I64_CONST, OP_GLOBAL_GET):
                raise ValueError(f'Unexpected init opcode {opcode:#x}')
            rdr.sleb()
            if rdr.byte() != OP_END:
                raise ValueError('Init expression is too complex')
        rdr.raw(rdr.uleb())
        segs.append((start, payload[start:rdr.tell()]))
    return header, segs


def sort_order(names, prefix):
    """Work out the new order for the segments

    Args:
        names (list of bytes): Name of each segment
        prefix (bytes): Prefix of the segments to sort

    Returns:
        list of int: Old index of the segment to put at each new position
    """
    keep = [i for i, nam in enumerate(names) if not nam.startswith(prefix)]
    move = [i for i, nam in enumerate(names) if nam.startswith(prefix)]
    move.sort(key=lambda i: (names[i], i))
    return keep + move


def sort_segment_info(payload, prefix):
    """Reorder the segment info, putting the sorted segments at the end

    Each entry is a segment name, its alignment and its flags.

    Args:
        payload (bytes): Contents of the WASM_SEGMENT_INFO subsection
        prefix (bytes): Prefix of the segments to sort

    Returns:
        tuple:
            bytes: New contents
            list of int: Old index of the segment at each new position
    """
    rdr = Reader(payload)
    infos = []
    for _ in range(rdr.uleb()):
        seg_name = rdr.name()
        infos.append((seg_name, rdr.uleb(), rdr.uleb()))
    order = sort_order([info[0] for info in infos], prefix)
    out = bytearray(uleb(len(infos)))
    for old in order:
        seg_name, align, flags = infos[old]
        out += name(seg_name) + uleb(align) + uleb(flags)
    return bytes(out), order


def update_symbols(payload, new_index):
    """Update the segment index of each data symbol which is defined

    Each entry is a kind and flags, followed by fields which depend on the
    kind. A defined data symbol has a name, segment index, offset and size.

    Args:
        payload (bytes): Contents of the WASM_SYMBOL_TABLE subsection
        new_index (dict): New segment index for each old one

    Returns:
        bytes: New contents
    """
    rdr = Reader(payload)
    count = rdr.uleb()
    out = bytearray(uleb(count))
    for _ in range(count):
        kind = rdr.byte()
        flags = rdr.uleb()
        out += bytes([kind]) + uleb(flags)
        if kind == SYMTAB_DATA:
            out += name(rdr.name())
            if not flags & SYM_UNDEFINED:
                out += uleb(new_index[rdr.uleb()])
                out += uleb(rdr.uleb()) + uleb(rdr.uleb())
        elif kind == SYMTAB_SECTION:
            out += uleb(rdr.uleb())
        elif kind in (SYMTAB_FUNCTION, SYMTAB_GLOBAL, SYMTAB_TAG,
                      SYMTAB_TABLE):
            out += uleb(rdr.uleb())
            if not flags & SYM_UNDEFINED or flags & SYM_EXPLICIT_NAME:
                out += name(rdr.name())
        else:
            raise ValueError(f'Unknown symbol kind {kind}')
    return bytes(out)


def update_comdats(payload, new_index):
    """Update the segment indexes in each COMDAT group

    Each entry is a name and flags, followed by a list of (kind, index)
    pairs. Only the indexes of data segments are changed.

    Args:
        payload (bytes): Contents of the WASM_COMDAT_INFO subsection
        new_index (dict): New segment index for each old one

    Returns:
        bytes: New contents
    """
    rdr = Reader(payload)
    count = rdr.uleb()
    out = bytearray(uleb(count))
    for _ in range(count):
        out += name(rdr.name()) + uleb(rdr.uleb())
        num_syms = rdr.uleb()
        out += uleb(num_syms)
        for _ in range(num_syms):
            kind = rdr.byte()
            index = rdr.uleb()
            if kind == COMDAT_DATA:
                index = new_index[index]
            out += bytes([kind]) + uleb(index)
    return bytes(out)


def update_linking(body, prefix):
    """Reorder the segment info and update the users of segment indexes

    Args:
        body (bytes): Contents of the 'linking' section, after its name
        prefix (bytes): Prefix of the segments to sort

    Returns:
        tuple:
            bytes: New contents
            list of int: Old index of the segment at each new position
    """
    rdr = Reader(body)
    version = rdr.uleb()
    subs = []
    while not rdr.done():
        sub_type = rdr.byte()
        subs.append([sub_type, rdr.raw(rdr.uleb())])

    order = None
    for sub in subs:
        if sub[0] == WASM_SEGMENT_INFO:
            sub[1], order = sort_segment_info(sub[1], prefix)
    if order is None:
        raise ValueError('No segment info found')
    new_index = {old: new for new, old in enumerate(order)}

    for sub in subs:
        if sub[0] == WASM_SYMBOL_TABLE:
            sub[1] = update_symbols(sub[1], new_index)
        elif sub[0] == WASM_COMDAT_INFO:
            sub[1] = update_comdats(sub[1], new_index)

    out = bytearray(uleb(version))
    for sub_type, payload in subs:
        out += bytes([sub_type]) + uleb(len(payload)) + payload
    return bytes(out), order


def update_relocs(body, data_index, old_segs, new_starts):
    """Update the relocations for the data section

    Args:
        body (bytes): Contents of a reloc section, after its name
        data_index (int): Index of the data section
        old_segs (list of (offset, record)): Segments in their old order
        new_starts (list of int): New offset of each old segment

    Returns:
        bytes: New contents, or None if this is for another section
    """
    rdr = Reader(body)
    if rdr.uleb() != data_index:
        return None
    relocs = []
    seg = 0
    for _ in range(rdr.uleb()):
        rtype = rdr.byte()
        offset = rdr.uleb()
        index = rdr.uleb()
        addend = rdr.sleb() if rtype in RELOCS_WITH_ADDEND else None

        # relocations are in order of offset, as are the old segments
        while offset >= old_segs[seg][0] + len(old_segs[seg][1]):
            seg += 1
        offset += new_starts[seg] - old_segs[seg][0]
        relocs.append((offset, rtype, index, addend))

    # wasm-ld needs these in order of offset
    relocs.sort(key=lambda reloc: reloc[0])
    out = bytearray(uleb(data_index) + uleb(len(relocs)))
    for offset, rtype, index, addend in relocs:
        out += bytes([rtype]) + uleb(offset) + uleb(index)
        if addend is not None:
            out += sleb(addend)
    return bytes(out)


def reorder_data(payload, order):
    """Put the segments of the data section in a new order

    Args:
        payload (bytes): Contents of the data section
        order (list of int): Old index of the segment at each new position

    Returns:
        tuple:
            bytes: New contents
            list of (offset, record): Segments in their old order
            list of int: New offset of each old segment
    """
    header, old_segs = read_segments(payload)
    if len(old_segs) != len(order):
        raise ValueError('Segment info does not match the data section')
    out = bytearray(header)
    new_starts = [0] * len(old_segs)
    for old in order:
        new_starts[old] = len(out)
        out += old_segs[old][1]
    return bytes(out), old_segs, new_starts


def sort_lists(data, prefix):
    """Sort the segments with a given prefix to the end of the data section

    Args:
        data (bytes): Contents of the relocatable object
        prefix (bytes): Prefix of the segments to sort

    Returns:
        tuple:
            bytes: New contents
            int: Number of segments which were sorted
    """
    sections = read_sections(data)
    data_index = None
    order = None
    for i, (sec_id, payload) in enumerate(sections):
        if sec_id == SEC_DATA:
            data_index = i
        elif sec_id == SEC_CUSTOM:
            sec_name, body = custom_name(payload)
            if sec_name == b'linking':
                body, order = update_linking(body, prefix)
                sections[i][1] = name(sec_name) + body
    if data_index is None or order is None:
        raise ValueError('Need a data section and a linking section')

    sections[data_index][1], old_segs, new_starts = reorder_data(
        sections[data_index][1], order)

    for i, (sec_id, payload) in enumerate(sections):
        if sec_id != SEC_CUSTOM:
            continue
        sec_name, body = custom_name(payload)
        if sec_name.startswith(b'reloc.'):
            body = update_relocs(body, data_index, old_segs, new_starts)
            if body is not None:
                sections[i][1] = name(sec_name) + body

    out = bytearray(data[:HEADER_SIZE])
    for sec_id, payload in sections:
        out += bytes([sec_id]) + uleb(len(payload)) + payload
    moved = sum(1 for new, old in enumerate(order) if new != old)
    return bytes(out), moved


def main():
    """Main program"""
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument('infile', help='Relocatable object to read')
    parser.add_argument('outfile', help='Relocatable object to write')
    parser.add_argument('-p', '--prefix', default='__u_boot_list',
                        help='Prefix of the segments to sort')
    parser.add_argument('-v', '--verbose', action='store_true',
                        help='Show the number of segments moved')
    args = parser.parse_args()

    with open(args.infile, 'rb') as inf:
        data = inf.read()
    out, moved = sort_lists(data, args.prefix.encode())
    with open(args.outfile, 'wb') as outf:
        outf.write(out)
    if args.verbose:
        print(f'{moved} segments moved')
    return 0


if __name__ == '__main__':
    sys.exit(main())
