.. SPDX-License-Identifier: GPL-2.0+:

.. index::
   single: vbe (command)

vbe command
===========

Synopsis
--------

::

    vbe list
    vbe select [<bootmeth>]
    vbe info
    vbe state
    vbe recovery [on|off]

Description
-----------

The `vbe` command looks at and controls Verified Boot for Embedded (VBE). See
:doc:`/develop/vbe` for more information about VBE.

vbe list
~~~~~~~~

Lists the VBE bootmeths. The 'Sel' column shows the one selected by
`vbe select`, if any.

vbe select
~~~~~~~~~~

Selects a VBE bootmeth, by name or sequence number, for use by the `vbe info`
and `vbe recovery` subcommands. With no argument, the selection is cleared.

vbe info
~~~~~~~~

Shows information about the selected VBE bootmeth, such as the version of the
firmware which it boots.

For the A/B/recovery method, the version is that of the firmware which booted,
taken from the slot info at the end of its slot if the board has slot info.
This also shows the state of an update:

Try B
    whether B is to be tried, i.e. an update is waiting to be confirmed, and how
    many tries it has left

Last result
    the result of the last try of B: `unknown`, `trying`, `ok` or `bad`

Recovery
    whether the recovery firmware is requested

Picked
    which firmware VPL picked on this boot: `A`, `B` or `Recovery`

vbe state
~~~~~~~~~

Shows which phases of U-Boot used VBE to load the next phase. For each phase
which relocated itself to make room for the next one, the margin left in its
memory is shown too.

vbe recovery
~~~~~~~~~~~~

Shows whether the next boot uses the recovery firmware, or changes it. This is
only supported by the VBE A/B/recovery method (`fwupd,vbe-abrec`), which keeps
A, B and recovery copies of the firmware.

on
    Request the recovery firmware. VPL then boots it every time, until the
    request is turned off.

off
    Stop requesting the recovery firmware, so that VPL goes back to booting A,
    or B while an update is being tried.

The selected VBE bootmeth is used. If none is selected, the first one with
recovery firmware is used.

Note that VPL also falls back to the recovery firmware by itself, if neither A
nor B can be loaded.

Example
-------

This shows the VBE bootmeths on sandbox::

    => vbe list
      #  Sel  Device           Driver          Description
    ---  ---  --------------   --------------  -----------
      2       vbe              vbe_abrec_os    VBE A/B/recovery for OS
      3       firmware0        vbe_simple      VBE simple
    ---  ---  --------------   --------------  -----------

This requests the recovery firmware on a board using the A/B/recovery method,
then goes back to normal::

    => vbe recovery
    Recovery: off
    => vbe recovery on
    Recovery: on
    => vbe recovery off
    Recovery: off

This shows an update being tried in B, which has two more tries if it does not
reach the OS::

    => vbe info
    Version: U-Boot v2026.10.4
    Vernum: 1/2
    Try B: yes, 2 tries left
    Last result: trying
    Recovery: off
    Picked: B

The vbe-simple method has no recovery firmware::

    => vbe select firmware0
    => vbe recovery
    VBE bootmeth 'firmware0' has no recovery firmware

Configuration
-------------

The `vbe` command is enabled by CONFIG_CMD_VBE and `vbe recovery` also needs
CONFIG_BOOTMETH_VBE_ABREC

Return value
------------

The return value $? is 0 (true) on success, 1 (false) on failure.
