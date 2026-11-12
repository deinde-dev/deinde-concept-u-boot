.. SPDX-License-Identifier: GPL-2.0+

.. index::
   single: rtc (command)

rtc command
===========

Synopsis
--------

::

    rtc list
    rtc dev [n]
    rtc read <reg> <count>
    rtc read <reg> <count> <addr>
    rtc write <reg> <hexstring>
    rtc write <reg> <count> <addr>

Description
-----------

The rtc command gives low-level access to the registers of the real-time
clocks which driver model knows about. It reads and writes the bytes of an RTC
chip directly rather than the time of day, so it reaches the registers which
the driver does not use as well as those it does: the alarm, the control bits
and any spare bytes of battery-backed memory the chip provides.

A board may have more than one RTC. Every sub-command apart from 'list' works
on the current device, which starts out as device 0 and is changed with
'rtc dev'. The :doc:`date<date>` command always uses the first RTC device, so
any other one can only be reached from here.

Register numbers, counts, memory addresses and the bytes of a hex string are
all in hex. The device number of 'rtc dev' is in decimal.

Common arguments:

reg
    Number of the first register to read or write. What the registers mean is
    up to the chip; the driver is not consulted, so a register the driver knows
    nothing about can be read and written like any other.
count
    Number of registers, each one byte.
addr
    Memory address to read the bytes into, or write them from.

rtc list
~~~~~~~~

This shows each RTC device, with the number to pass to 'rtc dev' and the
device's driver-model name. The devices are not probed, so a device which is
present but broken is still listed.

rtc dev
~~~~~~~

With no number this shows the current device. With a number it selects that
device and shows it. The device is probed as it is selected, so a number no
device has, or a device which cannot be probed, is reported as an error and
the current device is left as it was.

rtc read
~~~~~~~~

This reads *count* registers starting at *reg*. Without an address the bytes
are shown in the usual memory-dump layout, 16 to a line, with the register
number in place of the address. With an address they are written to memory
there and nothing is printed.

The display form reads at most 0x20 registers per call into the driver,
repeating until it has them all, while the memory form asks for the whole
count in one call.

rtc write
~~~~~~~~~

This writes registers starting at *reg*, taking the bytes either from a hex
string on the command line or from memory.

The hex string holds two hex digits per register and so must have an even
number of digits. It is written 0x20 registers at a time, as 'rtc read'
displays them; the memory form writes the whole count in one call.

Arguments:

hexstring
    Bytes to write, as hex digit pairs with no separators, such as 'cafe' for
    the two bytes 0xca and 0xfe.

Example
-------

This is sandbox, which has two emulated I2C real-time clocks::

    => rtc list
    RTC #0 - rtc@43
    RTC #1 - rtc@61
    => rtc dev
    RTC #0 - rtc@43

Reading the time registers of the first clock. The sandbox RTC keeps the
seconds in register 5 and then the minutes, hours, day of the month, month,
years since 1900 and day of the week, each as a plain binary value, so these
seven bytes are the time which the date command shows::

    => date
    Date: 2026-11-12 (Thursday)    Time:  8:51:33
    => rtc read 5 7
    00000005: 21 33 08 0c 0b 7e 04                             !3...~.

The same registers read into memory at 0x1000000, which prints nothing::

    => rtc read 5 7 1000000
    => md.b 1000000 7
    01000000: 21 33 08 0c 0b 7e 04                             !3...~.

Writing two of the four spare registers which the sandbox RTC has at 0x30,
first from a hex string and then from memory::

    => rtc write 0x30 cafe
    => rtc read 0x30 4
    00000030: ca fe 00 00                                      ....
    => mw.b 1000000 55 2
    => rtc write 0x30 2 1000000
    => rtc read 0x30 4
    00000030: 55 55 00 00                                      UU..

Each device has its own registers, so the spare bytes of the second clock are
still zero::

    => rtc dev 1
    RTC #1 - rtc@61
    => rtc read 0x30 4
    00000030: 00 00 00 00                                      ....

A device number which does not exist is refused and the current device stays
as it is::

    => rtc dev 3
    Cannot find RTC #3: err=-19
    => rtc dev
    RTC #1 - rtc@61

A hex string with an odd number of digits is refused before anything is
written::

    => rtc write 0x30 abc
    invalid hex string

Configuration
-------------

The command is available if CONFIG_CMD_RTC=y, which requires
CONFIG_DM_RTC=y and is the default on x86.

Return value
------------

The return value $? is 0 (true) if the command succeeds. It is 1 (false) if
no RTC device is listed, if the current device cannot be found or probed, if a
register cannot be read or written, or if a hex string is not valid hex or has
an odd number of digits. A missing argument, too many arguments or an unknown
sub-command shows the usage message and gives 1.

See also
--------

* :doc:`date<date>` for reading and setting the time of day, which is what
  most boards need
* *i2c* for reaching the registers of an I2C real-time clock which has no
  driver at all
* :doc:`dm<dm>` for the RTC devices listed along with the rest of the
  driver-model tree
