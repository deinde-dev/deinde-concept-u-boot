.. SPDX-License-Identifier: GPL-2.0+

.. index::
   single: pmbus (command)

pmbus command
=============

Synopsis
--------

::

    pmbus list
    pmbus dev [<bus>:<addr> | <name>]
    pmbus info
    pmbus telemetry
    pmbus status
    pmbus dump
    pmbus read <reg> [b | w | s]
    pmbus write <reg> <val> [b | w]
    pmbus clear [faults]
    pmbus vout [<uV>]
    pmbus scan [<bus>]
    pmbus help
    pmbus <vendor> ...

Description
-----------

The pmbus command interrogates and controls a PMBus 1.x device, such as a
digital point-of-load regulator or a hot-swap controller. These chips sit on an
I2C bus and answer the command set which PMBus lays over SMBus, so the command
can read a chip's identification, decode its telemetry and its fault status,
read and write its registers raw, and set the voltage it puts out.

Most sub-commands work on the active device, which is chosen once with
'pmbus dev' and then used by all of them, the way the
:doc:`regulator<regulator>` command works. Selecting a device reads its MFR_ID
in both byte orders, looks the answer up in the registry which per-chip drivers
fill in as they probe, and remembers the decoding metadata of the chip it
matches. Where nothing matches, the sensors are decoded as LINEAR11 and the
output voltage as LINEAR16 scaled by the chip's own VOUT_MODE, which is what
the PMBus specification asks a compliant chip for.

See :doc:`../../develop/pmbus` for the framework behind the command: the format
decoders, the generic regulator driver and how to add a per-chip driver.

Arguments
~~~~~~~~~

bus
    I2C bus number, in decimal, as :doc:`i2c<i2c>` numbers the buses.
addr
    7-bit address of the chip on that bus, in hex, with an optional '0x'.
name
    The chip's regulator-name, as 'pmbus list' shows it, for a chip which is
    bound to a driver-model regulator.
reg
    Register to read or write. Either a PMBus command code in hex, or one of
    the names below, which are matched without regard to case.
val
    Value to write, in hex.
uV
    Output voltage in microvolts, in decimal.

The register names are PAGE, OPERATION, ON_OFF_CONFIG, CLEAR_FAULTS,
WRITE_PROTECT, CAPABILITY, VOUT_MODE, VOUT_COMMAND, VOUT_TRIM, VOUT_MAX,
VOUT_SCALE_LOOP, STATUS_BYTE, STATUS_WORD, STATUS_VOUT, STATUS_IOUT,
STATUS_INPUT, STATUS_TEMP, STATUS_CML, READ_VIN, READ_IIN, READ_VOUT,
READ_IOUT, READ_TEMP1, READ_TEMP2, READ_TEMP3, READ_DUTY, READ_FREQ,
READ_POUT, READ_PIN, REVISION, MFR_ID, MFR_MODEL and MFR_REVISION. A name wins
over a number, so a register which has one cannot be reached by its code alone.

pmbus list
~~~~~~~~~~

Lists every driver-model regulator, with the chip's I2C bus and address where
it has a parent on an I2C bus. The list is not filtered to PMBus chips, since a
chip which has not been probed yet cannot be asked what it is; a PMBus chip
shows up with the driver which binds it, such as pmbus_generic_regulator.

pmbus dev
~~~~~~~~~

With no argument, shows the active device. With one, selects it, either as
<bus>:<addr> or by regulator-name, and shows what the chip answered. Selecting
a chip is what probes it, so it is also how a per-chip driver gets to register
its vendor extensions.

An address which no chip answers is selected all the same, since the bus is not
probed for it: the identification shows as '?' and the first read of the chip
is what fails. A bus which does not exist is refused.

pmbus info
~~~~~~~~~~

Shows the identification strings, the PMBus revision the chip reports and the
decoding metadata in use for it, including the DIRECT coefficients where the
chip uses that format. MFR_REVISION is shown as text and as the raw bytes,
since a chip may pack a revision into bytes which do not print.

pmbus telemetry
~~~~~~~~~~~~~~~

Reads the input and output voltage, the input and output current and the
temperature, and shows each one raw and decoded. A sensor the chip does not
implement is reported as '(not supported)' rather than as an error.

pmbus status
~~~~~~~~~~~~

Reads STATUS_WORD and each of the per-rail status registers and names the bits
which are set. A chip with nothing wrong reports 'clean'. The sticky bits stay
set until a CLEAR_FAULTS, so this says what has gone wrong since the chip was
powered up, which may be before U-Boot started.

pmbus dump
~~~~~~~~~~

Reads every register the command knows a name for, as a byte, a word or a
string according to what that register holds. A register the chip does not
answer is left out of the listing.

pmbus read
~~~~~~~~~~

Reads one register, without decoding its value. The format selector says which
SMBus transaction to use: 'b' for a byte, 'w' for a 16-bit little-endian word
and 's' for the block read which the string registers use. It defaults to 'b',
so a word register read without one gives the low byte.

pmbus write
~~~~~~~~~~~

Writes one register, as a byte or a word. There is no string form. The value is
sent as it is given, so it must already be in the format the chip expects;
'pmbus vout' is the way to set an output voltage without encoding it by hand.

pmbus clear
~~~~~~~~~~~

Issues CLEAR_FAULTS, which clears the sticky bits of every STATUS_* register.
It does nothing to a fault which is still present, since the chip sets the bit
again at once. The word 'faults' may be given or left out; another word is
refused.

pmbus vout
~~~~~~~~~~

With no argument, reads READ_VOUT and shows it raw and in volts. With a value
in microvolts, encodes it for the format VOUT_MODE selects and writes
VOUT_COMMAND, which changes the voltage the chip puts out. LINEAR16 and DIRECT
are implemented; a chip asking for VID or IEEE754 is refused.

This is a single write, not the full automatic-voltage-scaling sequence a board
needs in production; see the develop page for how that differs.

pmbus scan
~~~~~~~~~~

Probes every address from 0x08 to 0x77 on one bus, or on every I2C bus when no
bus is given, and reports each chip which answers an MFR_ID block read with a
sensible length. This finds a chip which has no devicetree node, so it is the
way to find out what a board really carries.

pmbus help
~~~~~~~~~~

Lists the vendor extensions which are registered. These are sub-commands in the
'pmbus <vendor> ...' namespace which a per-chip driver adds for the registers
its chip has beyond the specification, so the list is empty until a chip which
has them has been probed.

Example
-------

This is on sandbox, whose emulated I2C bus carries a PMBus chip at address
0x70. The log level is raised first because the emulation reports an error for
each of the addresses which 'pmbus scan' tries and nothing answers.

Finding the chip and selecting it::

    => log level crit
    => pmbus scan 0
    pmbus scan i2c0:
      i2c0:0x70  MFR_ID="SANDBOX"
    => pmbus dev 0:70
    pmbus: active i2c0:0x70  rail="sandbox-pmbus-vout"  MFR_ID="SANDBOX"  MODEL="PMBUS-EMUL"  vendor=(generic)

The same chip is bound to a regulator, so it can be selected by name as well::

    => pmbus list
    UCLASS_REGULATOR devices (no PMBus filter):
      (non-I2C)   rail="fixed-gpio-enabled"  node=fixed-gpio-reg  driver=regulator_fixed
      (non-I2C)   rail="gpio-switched"  node=gpio-reg  driver=gpio regulator
      (non-I2C)   rail="SUPPLY_1.2V"  node=buck1  driver=sandbox_buck
      (non-I2C)   rail="SUPPLY_3.3V"  node=buck2  driver=sandbox_buck
      (non-I2C)   rail="VDD_EMMC_1.8V"  node=ldo1  driver=sandbox_ldo
      (non-I2C)   rail="VDD_LCD_3.3V"  node=ldo2  driver=sandbox_ldo
      (non-I2C)   rail="buck_SUPPLY_1.5V"  node=no_match_by_nodename  driver=sandbox_buck
      (non-I2C)   rail="SUPPLY_1.8_3.3V"  node=ldo3  driver=sandbox_ldo
      i2c0:0x70  rail="sandbox-pmbus-vout"  node=pmbus@70  driver=pmbus_generic_regulator
      (non-I2C)   rail="sandbox-voltd0"  node=reg@0  driver=scmi_regulator
      (non-I2C)   rail="sandbox-voltd1"  node=reg@1  driver=scmi_regulator
    => pmbus dev sandbox-pmbus-vout
    pmbus: active i2c0:0x70  rail="sandbox-pmbus-vout"  MFR_ID="SANDBOX"  MODEL="PMBUS-EMUL"  vendor=(generic)

What the chip says it is::

    => pmbus info
    pmbus device i2c0:0x70
      regulator-name: "sandbox-pmbus-vout"
      MFR_ID        : "SANDBOX"
      MFR_MODEL     : "PMBUS-EMUL"
      MFR_REVISION  : "1.0"  raw=0x312e30
      PMBUS_REVISION: 0x33 (PMBus 1.3)
      vendor        : (none)
      driver_info   : pages=1
        [VOLTAGE_IN  ] format=LINEAR
        [VOLTAGE_OUT ] format=LINEAR
        [CURRENT_IN  ] format=LINEAR
        [CURRENT_OUT ] format=LINEAR
        [POWER       ] format=LINEAR
        [TEMPERATURE ] format=LINEAR

Its sensors and its fault status. The emulated chip answers neither READ_IIN
nor READ_POUT::

    => pmbus telemetry
    pmbus telemetry @ i2c0:0x70
      VIN  : raw=0x0abc  1400.000V
      VOUT : raw=0x0200  2.000V
      IIN  : (not supported)
      IOUT : raw=0x0123  291.000A
      POUT : (not supported)
      TEMP : raw=0x0019  25.000C
    => pmbus status
    pmbus status @ i2c0:0x70
      STATUS_WORD    (79h) = 0x0000  [clean]
      STATUS_VOUT    (7Ah) = 0x00  [clean]
      STATUS_IOUT    (7Bh) = 0x00  [clean]
      STATUS_INPUT   (7Ch) = 0x00  [clean]
      STATUS_TEMP    (7Dh) = 0x00  [clean]
      STATUS_CML     (7Eh) = 0x00  [clean]
    => pmbus clear faults
    pmbus: CLEAR_FAULTS (03h) issued (RAM sticky STATUS_* cleared)

Every register at once, then the same thing one register at a time, by name and
by code::

    => pmbus dump
    pmbus dump @ i2c0:0x70 (registers known to <pmbus.h>)
      00h  PAGE            b=0x00
      01h  OPERATION       b=0x80
      02h  ON_OFF_CONFIG   b=0x00
      10h  WRITE_PROTECT   b=0x00
      19h  CAPABILITY      b=0x30
      20h  VOUT_MODE       b=0x18
      21h  VOUT_COMMAND    w=0x0200
      22h  VOUT_TRIM       w=0x0000
      24h  VOUT_MAX        w=0x0400
      29h  VOUT_SCALE_LOOP w=0x0000
      78h  STATUS_BYTE     b=0x00
      79h  STATUS_WORD     w=0x0000
      7ah  STATUS_VOUT     b=0x00
      7bh  STATUS_IOUT     b=0x00
      7ch  STATUS_INPUT    b=0x00
      7dh  STATUS_TEMP     b=0x00
      7eh  STATUS_CML      b=0x00
      88h  READ_VIN        w=0x0abc
      8bh  READ_VOUT       w=0x0200
      8ch  READ_IOUT       w=0x0123
      8dh  READ_TEMP1      w=0x0019
      98h  REVISION        b=0x33
      99h  MFR_ID          s="SANDBOX"
      9ah  MFR_MODEL       s="PMBUS-EMUL"
      9bh  MFR_REVISION    s="1.0"
    => pmbus read VOUT_MODE
      20h  VOUT_MODE       b=0x18
    => pmbus read 8b w
      8bh  READ_VOUT       w=0x0200
    => pmbus read MFR_ID s
      99h  MFR_ID          s="SANDBOX"

Reading the output voltage, then setting it, by hand and in microvolts.
VOUT_MODE 0x18 selects LINEAR16 with an exponent of -8, so 1.5V comes out as
0x180. The emulated chip answers READ_VOUT from a register of its own, so the
reading does not follow VOUT_COMMAND here as a real chip's would::

    => pmbus vout
    pmbus VOUT @ i2c0:0x70  raw=0x0200  2.000V
    => pmbus write VOUT_COMMAND 123 w
    pmbus: wrote 0x123 to 21h (VOUT_COMMAND)
    => pmbus read VOUT_COMMAND w
      21h  VOUT_COMMAND    w=0x0123
    => pmbus vout 1500000
    pmbus: VOUT_COMMAND <- 0x0180 (LINEAR16, target 1500000 uV)
    => pmbus read VOUT_COMMAND w
      21h  VOUT_COMMAND    w=0x0180

No chip has registered a vendor extension on sandbox::

    => pmbus help
    pmbus: no vendor extensions registered.
           Vendor handlers are registered by per chip drivers at
           probe time; trigger a probe via 'pmbus dev <name>' or a
           board hook (boot snapshot) and re run 'pmbus help'.

Asking for a chip which is not there, and a sub-command which needs one before
any has been selected::

    => pmbus dev nosuchrail
    pmbus: 'nosuchrail' is neither <bus>:<addr> nor a known regulator-name (-19)
    => pmbus info
    pmbus: no active device. Use 'pmbus dev <bus>:<addr>' first.

Configuration
-------------

The command is available if CONFIG_CMD_PMBUS=y, which requires
CONFIG_PMBUS=y. The 'list' sub-command needs CONFIG_DM_REGULATOR=y and reports
that it is missing otherwise.

Return value
------------

The return value $? is 0 (true) if the command succeeds. It is 1 (false) if the
device cannot be selected, if there is no active device or it cannot be reached
over I2C, if a read or a write of the chip fails, or if VOUT_MODE asks for an
encoding the command does not implement. A missing argument, too many
arguments, an unknown register, an unknown format, a value too large for the
format, a word other than 'faults' after 'clear' or an unknown sub-command
shows the usage message and gives 1.

Note that 'pmbus list' and 'pmbus scan' give 0 even when they find nothing, as
does 'pmbus dev' when no device is selected.

See also
--------

* :doc:`regulator<regulator>` for setting the voltage of the same chip through
  its driver, rather than by writing VOUT_COMMAND
* :doc:`i2c<i2c>` for reaching the chip's registers with no PMBus decoding at
  all, and for the bus numbering this command uses
* :doc:`pmic<pmic>` for the registers of a power-management chip which provides
  several such supplies
* :doc:`../../develop/pmbus` for the framework the command sits on and how to
  support a new chip
