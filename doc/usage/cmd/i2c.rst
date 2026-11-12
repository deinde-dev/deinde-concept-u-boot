.. SPDX-License-Identifier: GPL-2.0+

.. index::
   single: i2c (command)

i2c command
===========

Synopsis
--------

::

    i2c bus [n]
    i2c dev [n]
    i2c speed [speed]
    i2c probe [chip]
    i2c md <chip> <address>[.0, .1, .2] [count]
    i2c mw <chip> <address>[.0, .1, .2] <value> [count]
    i2c mm[.b, .w, .l] <chip> <address>[.0, .1, .2]
    i2c nm[.b, .w, .l] <chip> <address>[.0, .1, .2]
    i2c read <chip> <address>[.0, .1, .2] <length> <memaddr>
    i2c write <memaddr> <chip> <address>[.0, .1, .2] <length> [-s]
    i2c crc32 <chip> <address>[.0, .1, .2] <count>
    i2c flags <chip> [flags]
    i2c olen <chip> [offset_length]
    i2c loop <chip> <address>[.0, .1, .2] [count] [delay]
    i2c reset
    i2c edid <chip>
    i2c sdram <chip>

Description
-----------

The i2c command gives low-level access to the chips on an I2C bus. It talks to
a chip by its bus address, without a driver for it, so it can read and write a
device which U-Boot knows nothing about: an EEPROM, a power-management chip, a
sensor or the DDC channel of a monitor.

Every sub-command apart from 'bus' works on the current bus, which starts out
unselected and is chosen with 'i2c dev'. A chip is addressed within the bus by
its 7-bit address, and a location within the chip by an offset which the
command sends to the chip before reading or writing.

Common arguments:

chip
    Address of the chip on the bus, in hex, as the data sheet gives it and
    without the read/write bit: a chip documented as 0x50 for writing and 0x51
    for reading is 50 here.
address
    Offset within the chip, in hex. An optional '.0', '.1' or '.2' suffix says
    how many bytes of offset to send; see below.
count
    Number of objects, in hex.
memaddr
    Memory address to read the bytes into, or write them from, in hex.

The bus number of 'i2c bus' and 'i2c dev', the bus speed and the delay of
'i2c loop' are decimal. Everything else is hex.

Offset length
~~~~~~~~~~~~~

Most chips take a one-byte offset, a large EEPROM takes two bytes, and a chip
with a single register takes none at all. The suffix on the address says which
to use:

* '.0' sends no offset, so the chip's own pointer decides what is read
* '.1' sends one byte, which suits a memory or register file of up to 256
  bytes
* '.2' sends two bytes, for a chip with more than 256 locations

With driver model, an address given with no suffix uses the length the chip is
already set to, which comes from its devicetree node or from 'i2c olen'. An
offset of more than three bytes is refused with the usage message.

i2c bus
~~~~~~~

This shows the I2C buses. With no number it lists them all, each with the
number to pass to 'i2c dev', the device's driver-model name, whether it is
active and the chips the devicetree gives it, with each chip's offset length
and flags. With a number it shows that bus alone, and a number no bus has is
reported as an error.

i2c dev
~~~~~~~

With no number this shows the current bus, as -1 if none is selected yet. With
a number it selects that bus and probes it. A number no bus has is reported as
an error and the current bus is left as it was.

i2c speed
~~~~~~~~~

With no argument this shows the speed of the current bus in Hz. With one it
asks the driver to run the bus at that speed; a driver which cannot do so
reports the error and keeps the old speed.

i2c probe
~~~~~~~~~

This looks for chips on the current bus and lists the addresses which answer.
With an address it tries that one alone, which is the quickest way to check
whether a particular chip is there. A board may exclude addresses which must
not be probed, in which case they are listed separately as excluded.

i2c md
~~~~~~

This reads *count* objects from the chip, starting at *address*, and shows
them in the usual memory-dump layout, 16 bytes to a line, with the offset
within the chip in place of the address. The count defaults to 0x10.

The chip, address and count are remembered, so pressing Enter at the prompt
shows the next block.

i2c mw
~~~~~~

This writes *value*, a single byte, to the chip at *address*, repeating it
*count* times at increasing offsets. The count defaults to one. Each byte is
written in its own transaction, with a delay afterwards so that an EEPROM has
time to finish.

i2c mm
~~~~~~

This reads a location, shows it and prompts for a new value, which is written
if anything is typed. Pressing Enter leaves the location alone. The address
increases each time, so this walks through the chip until something which is
not a hex value is typed, which ends the command.

The '.b', '.w' and '.l' suffixes choose the size of each access, one, two or
four bytes; the default is a byte.

i2c nm
~~~~~~

This is 'i2c mm' without the increment: the same location is read and
rewritten until the command is ended. It suits a register which is written
again and again, such as a chip's data port.

i2c read
~~~~~~~~

This reads *length* bytes from the chip at *address* into memory at *memaddr*,
printing nothing. The whole length is read in a single transaction.

i2c write
~~~~~~~~~

This writes *length* bytes from memory at *memaddr* to the chip at *address*.
By default each byte is written in its own transaction, with the offset sent
each time and a delay afterwards, which is what an EEPROM with a slow write
cycle needs.

The '-s' option, which must come last, writes the whole length in a single
transaction instead, with no delay at the end. That is much quicker, but it is
up to the caller to keep within a page of the chip and to leave time before
the next access. Either form leaves the chip's write-address flag set the way
it used it, so 'i2c flags' shows 4 after a repeated write and 0 after '-s'.

i2c crc32
~~~~~~~~~

This reads *count* bytes from the chip, a byte at a time, and shows the CRC32
of what it read. The range is shown as well, so the output says which bytes
the checksum covers.

i2c flags
~~~~~~~~~

With no value this shows the chip's flags in hex. With one it sets them. The
bits are 1 for 10-bit addressing, 2 to send the offset before each byte read
and 4 to send it before each byte written.

i2c olen
~~~~~~~~

With no value this shows how many bytes of offset the chip is set to take.
With one it sets that length, which then applies to every access which does
not give a '.0', '.1' or '.2' suffix of its own.

i2c loop
~~~~~~~~

This reads the chip over and over, waiting *delay* microseconds between reads,
so that the traffic can be watched with a scope or a logic analyser. The count
defaults to one object and the delay to 1000us. The command never returns and
cannot be interrupted, so the board must be reset to stop it.

i2c reset
~~~~~~~~~

This asks the driver to recover a bus which a chip is holding, by clocking it
until the chip lets go. A driver which cannot do that reports that it is not
supported.

i2c edid
~~~~~~~~

This reads the EDID block of a monitor, from the DDC channel the monitor
presents as an I2C chip, and shows what it says about the manufacturer, the
product and the modes it supports. A chip which answers but holds something
else is reported as not valid EDID.

i2c sdram
~~~~~~~~~

This reads the SPD EEPROM of a memory module and shows what it says about the
type, size and timing of the module. Only a board which has such a module
needs it, so it is a separate option.

Example
-------

This is sandbox, which has an emulated I2C bus with an EEPROM on it. No bus is
selected to start with::

    => i2c dev
    No I2C bus selected
    Current bus is -1
    => i2c bus
    Bus 0:	i2c@0  (active 0)
       2c: eeprom@2c, offset len 1, flags 0
       43: rtc@43, offset len 1, flags 0
       61: rtc@61, offset len 1, flags 0
       7f: emul@7f, offset len 1, flags 0
       40: sandbox_pmic@40, offset len 1, flags 0
       41: pmic@41, offset len 1, flags 0
       70: pmbus@70, offset len 1, flags 0
    => i2c dev 0
    Setting bus to 0
    => i2c speed
    Current bus speed=100000
    => i2c probe 2c
    Valid chip addresses: 2C

Reading the first 0x20 bytes of the EEPROM, which holds a MAC address at
offset 0x18, then filling four bytes with a value::

    => i2c md 2c 0 20
    0000: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00    ................
    0010: 00 00 00 00 00 00 00 00 02 00 11 22 33 45 00 00    ..........."3E..
    => i2c mw 2c 0 55 4
    => i2c md 2c 0 10
    0000: 55 55 55 55 00 00 00 00 00 00 00 00 00 00 00 00    UUUU............

Writing four bytes from memory to offset 8 of the chip and reading them back
to a different address::

    => mw.b 1000000 ab 4
    => i2c write 1000000 2c 8 4
    => i2c md 2c 0 10
    0000: 55 55 55 55 00 00 00 00 ab ab ab ab 00 00 00 00    UUUU............
    => i2c read 2c 8 4 2000000
    => md.b 2000000 4
    02000000: ab ab ab ab                                      ....

A checksum of the first 0x10 bytes, the chip's offset length and its flags,
which the repeated write above has left with the write-address bit set::

    => i2c crc32 2c 0 10
    CRC32 for 00000000 ... 0000000f ==> 920f16e8
    => i2c olen 2c
    1
    => i2c flags 2c
    4

Changing the first two bytes by hand, ending with a value which is not hex::

    => i2c mm 2c 0
    00000000: 55 ? 11
    00000001: 55 ? 22
    00000002: 55 ? q
    => i2c md 2c 0 10
    0000: 11 22 55 55 00 00 00 00 ab ab ab ab 00 00 00 00    ."UU............

An EEPROM is not a monitor, and a bus number which does not exist is refused::

    => i2c edid 2c
    Content isn't valid EDID.
    => i2c dev 3
    Setting bus to 3
    Failure changing bus number (-19)

Configuration
-------------

The command is available if CONFIG_CMD_I2C=y, which requires CONFIG_DM_I2C=y
or the older CONFIG_SYS_I2C_LEGACY=y. The 'flags' and 'olen' sub-commands need
driver model, 'edid' needs CONFIG_I2C_EDID=y and 'sdram' needs
CONFIG_CMD_SDRAM=y.

Return value
------------

The return value $? is 0 (true) if the command succeeds. It is 1 (false) if no
bus is selected, if the bus or the chip cannot be found, if a transfer fails,
if the bus speed cannot be changed, if 'i2c reset' is not supported by the
driver, if 'i2c probe' finds no chip at all or if 'i2c edid' reads something
which is not valid EDID. A missing argument, too many arguments, an offset
length above three bytes or an unknown sub-command shows the usage message and
gives 1.

Note that 'i2c crc32' gives 0 even when it cannot read the chip, reporting the
error in its output instead.

See also
--------

* :doc:`i3c<i3c>` for the same sort of low-level access to an I3C bus
* :doc:`sspi<sspi>` for a SPI bus, where the chip is selected by a chip-select
  line rather than by an address
* :doc:`w1<w1>` for a 1-Wire bus, which carries power and data on a single wire
* :doc:`md<md>` and :doc:`mw<mw>` for the same sort of access to memory, which
  the md and mw sub-commands here are modelled on
* :doc:`crc32<crc32>` for a checksum over memory rather than over a chip
* :doc:`rtc<rtc>` for a real-time clock through its driver, rather than by
  reaching its registers over I2C
