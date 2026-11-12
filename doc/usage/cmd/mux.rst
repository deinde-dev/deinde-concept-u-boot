.. SPDX-License-Identifier: GPL-2.0+

.. index::
   single: mux (command)

mux command
===========

Synopsis
--------

::

    mux list
    mux select <chip> <id> <state>
    mux deselect <chip> <id>

Description
-----------

The mux command shows and changes the state of the multiplexer controllers
which driver model knows about. A mux controller is a piece of hardware which
connects a shared line to one of several destinations: an analogue switch, a
register field choosing which peripheral owns a pin, or an emulation of
either. Drivers normally ask for the state they need through the mux API, so
this command is for driving a mux by hand while bringing a board up.

A mux chip is a device holding one or more controllers, numbered from zero.
Each controller has a number of states, a state it returns to when nobody is
using it (its idle state) and a flag saying whether something holds it at the
moment.

Common arguments:

chip
    Name of the mux device, as 'mux list' shows it.
id
    Number of the controller within the chip, in decimal.

mux list
~~~~~~~~

This shows each mux chip, with a line for each of its controllers:

ID
    Number of the controller within the chip.
Selected
    'yes' if something holds the controller, else 'no'.
Current State
    State the controller is in, in hex, or 'unknown' where it has not been set
    to a known state yet.
Idle State
    State the controller returns to when it is deselected, in hex, or 'as-is'
    where it keeps whatever state it is in, or 'disconnect' where it is left
    connected to nothing.
Num States
    Number of states the controller has, in hex. The valid states run from 0
    to one less than this.

mux select
~~~~~~~~~~

This sets a controller to a state and holds it there, so that nothing else can
change it until it is deselected. A controller which something already holds
cannot be selected again.

Arguments:

state
    State to select, in hex. It must be lower than the controller's number of
    states.

mux deselect
~~~~~~~~~~~~

This releases a controller selected earlier and sets it back to its idle
state, unless the idle state is 'as-is', in which case the controller keeps
the state it is in. Deselecting a controller which nothing holds is not an
error.

Example
-------

List the mux chips. This is sandbox, where the parent of the mmio-mux node is
both a 'simple-mfd' and a 'syscon', so the node is bound twice and
a-mux-controller appears twice as well::

    => mux list
    a-mux-controller:
        ID              Selected        Current State   Idle State      Num States
        0               no              unknown         as-is           0x4
        1               no              0x2             0x2             0x10
        2               no              0x73            0x73            0x100

    emul-mux-controller:
        ID              Selected        Current State   Idle State      Num States
        0               no              0xabcd          0xabcd          0x100000

    a-mux-controller:
        ID              Selected        Current State   Idle State      Num States
        0               no              unknown         as-is           0x4
        1               no              0x2             0x2             0x10
        2               no              0x73            0x73            0x100

Select state 0x1234 on the only controller of the emulated chip. The other
chips are left out of the listing here::

    => mux select emul-mux-controller 0 1234
    => mux list
    emul-mux-controller:
        ID              Selected        Current State   Idle State      Num States
        0               yes             0x1234          0xabcd          0x100000

Release it again, which puts it back to its idle state of 0xabcd::

    => mux deselect emul-mux-controller 0
    => mux list
    emul-mux-controller:
        ID              Selected        Current State   Idle State      Num States
        0               no              0xabcd          0xabcd          0x100000

A chip which does not exist, or a controller number the chip does not have,
gives an error::

    => mux select no-such-controller 0 1
    Failed to find the specified mux
    => mux select a-mux-controller 9 1
    Failed to find the specified mux

Return value
------------

The return value $? is 0 (true) if the command succeeds. It is 1 (false) if
the named chip does not exist, if the controller number is not a decimal
number or the chip does not have it, if the state is not a hex number, or if
the mux cannot be set to the state asked for, which includes the case where
something already holds it. A missing argument or an unknown sub-command shows
the usage message and gives 1.

See also
--------

* :doc:`pinmux<pinmux>` for the muxing of an SoC's pins, which is a different
  kind of mux, set up from the devicetree rather than by hand
* :doc:`dm<dm>` for the mux chips listed along with the rest of the
  driver-model tree
