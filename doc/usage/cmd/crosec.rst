.. SPDX-License-Identifier: GPL-2.0+:

.. index::
   single: crosec (command)

crosec command
==============

Synopsis
--------

::

    crosec init
    crosec id
    crosec info
    crosec version
    crosec features
    crosec switches
    crosec events
    crosec clrevents [<mask>]
    crosec curimage
    crosec hash
    crosec reboot [ro | rw | cold]
    crosec regioninfo <ro|rw>
    crosec flashinfo
    crosec erase <ro|rw>
    crosec read <ro|rw> <addr> [<size>]
    crosec write <ro|rw> <addr> [<size>]
    crosec vbnvcontext [<hexstring>]
    crosec ldo <idx> [<state>]
    crosec sku
    crosec test

Description
-----------

The crosec command talks to the Chromium OS Embedded Controller (EC), a small
microcontroller which Chromebooks use for the keyboard, the battery charger,
the fans and the switches which select recovery and developer mode. U-Boot
reaches it over I2C, SPI or the LPC bus, depending on the board, so the
command needs CONFIG_CROS_EC, a driver for the transport and
CONFIG_CMD_CROS_EC.

The EC holds two firmware images in its own flash: a read-only one which the
board verifies at reset, and a read-write one which the RO image jumps to.
Several sub-commands work on that flash, which is separate from the flash the
AP itself boots from.

Every sub-command uses the first EC device, which U-Boot probes at startup.

crosec init
~~~~~~~~~~~

This removes the EC device and probes it again, which re-opens the transport
and asks the EC for its protocol version. Startup does this already, so the
sub-command is useful only after the EC has been rebooted or reflashed.

crosec id, crosec version and crosec info
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

'id' shows the version string of the image the EC is running. 'version' shows
the version strings of both images, which of them is running, and the build
information the EC reports. 'info' shows the size of the keyboard matrix the
EC scans, as a number of rows and columns.

crosec features and crosec switches
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

'features' lists the features the EC reports, one per line, naming each bit it
has set. A bit which this version of U-Boot has no name for is shown as
'unknown' with its bit number.

'switches' lists the switches the EC holds in its state: the lid, the power
button, the write-protect screw and the dedicated recovery button. Only the
switches which are on are shown, so nothing is printed where none are. This
needs a driver which reads them, which the LPC and sandbox drivers do.

crosec events and crosec clrevents
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

'events' shows the host-event mask as eight hex digits, then names each event
which is set. It reads the second copy of the mask, which the EC keeps for the
firmware, since the operating system uses the first one.

'clrevents' clears events from that copy. The mask is a number in any base
U-Boot understands and defaults to 0x7fffffff, which clears everything.

crosec curimage, crosec hash and crosec reboot
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

'curimage' shows which image is running, as 0 for unknown, 1 for RO and 2 for
RW.

'hash' shows the hash the EC holds for its active image, as a type, the offset
and size of the region it covers and the digest itself. Where the EC has no
valid hash it is asked to work one out first, which takes a moment.

'reboot' restarts the EC, which also resets the devices it controls. 'ro' and
'rw' make it jump to that image, while 'cold' powers it down and up again.
Note that the AP is not reset, so :doc:`reset<reset>` is the way to restart
U-Boot itself.

crosec regioninfo, crosec flashinfo, crosec erase, crosec read and crosec write
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

These work on the EC's flash, naming the region as 'ro' or 'rw'.

'regioninfo' shows the offset and size of a region in hex and 'flashinfo'
shows the size of the whole flash along with the write and erase block sizes,
in decimal.

'erase' erases a whole region. 'read' and 'write' copy a region to and from
memory, where the address is a U-Boot memory address. The size is a hex number
of bytes and defaults to the size of the region.

crosec vbnvcontext
~~~~~~~~~~~~~~~~~~

This reads the 16-byte non-volatile context which verified boot keeps in the
EC, showing it as hex digits. Given a hex string it writes it instead, padding
with zero bytes where the string is shorter than the block.

crosec ldo
~~~~~~~~~~

This reads an LDO, or a FET, which the EC switches on boards where it controls
power rails, naming it by index. With a state of 1 it switches the rail on and
with 0 it switches it off.

crosec sku and crosec test
~~~~~~~~~~~~~~~~~~~~~~~~~~

'sku' shows the SKU ID the EC reports for the board, which tells the firmware
which of a family of mainboards it is running on.

'test' exchanges a handshake with the EC and checks the value which comes
back, which shows that the transport works in both directions.

Example
-------

This runs the sub-commands which sandbox's emulated EC answers::

    => crosec id
    sandbox_ro
    => crosec curimage
    1
    => crosec sku
    1234
    => crosec features
    flash
    i2c
    vstore
    unified_wake_masks
    ish
    => crosec events
    00000000
    => crosec test
    Test passed

The flash sub-commands work on the emulated EC's flash. The emulator prints a
line of its own for each region it is asked about::

    => crosec regioninfo rw
    EC flash region 1: offset=0x10000, size=0x10000
    Region: RW
    Offset: 10000
    Size:   10000
    => crosec erase rw
    EC flash region 1: offset=0x10000, size=0x10000
    => crosec write rw 1000 2
    EC flash region 1: offset=0x10000, size=0x10000
    => crosec hash
    type:    SHA-256
    offset:  0x00010000
    size:    0x00000002
    digest:  d021c38903eefed7e83e17fe8b952f0102a1109da26f83d16f90c2bb219114f4

Sandbox's EC has no keyboard matrix, no flash geometry, no LDOs and no build
information, so 'info', 'flashinfo', 'read', 'ldo' and 'version' report an
error there. On a Chromebook, whose keyboard is 8 rows of 13 columns, 'info'
shows the matrix instead::

    => crosec info
    rows     = 8
    cols     = 13

Return value
------------

The return value $? is 0 (true) if the sub-command does what it is asked.

It is 1 (false) if the EC cannot be reached, if it refuses the command or if
it reports an error, which is shown along with the error number. The command
shows its usage message where the sub-command is missing, is not one it has,
or does not have the arguments it needs.

See also
--------

* :doc:`coninfo<coninfo>` for the keyboard which the EC provides
* :doc:`pwm<pwm>` for the PWM channels which the EC provides
* :doc:`reset<reset>` for restarting the board rather than the EC
* :doc:`../../arch/sandbox/sandbox` for the emulated EC which the example uses
* *i2c* for the I2C buses which tunnel through the EC
