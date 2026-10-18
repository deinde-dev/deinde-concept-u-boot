.. SPDX-License-Identifier: GPL-2.0+

.. index::
   single: event (command)

event command
=============

Synopsis
--------

::

    event list

Description
-----------

The event command provides information about the event spies, the functions
U-Boot calls when something of interest happens to it.

The event list command shows every spy built into U-Boot, in the order they
appear in the linker list, which is also the order they are called in for a
given event type.

This shows the following information:

Seq
    Sequence number of the spy, numbered from 0

Type
    Type of the spy, both as a number and a label. If `CONFIG_EVENT_DEBUG` is
    not enabled, the label just shows `(unknown)`.

Function
    Address of the function to call

ID
    ID string for this event, if `CONFIG_EVENT_DEBUG` is enabled. Otherwise this
    just shows `?`.

Only the static spies are shown. Those registered at run time with
event_register(), which `CONFIG_EVENT_DYNAMIC` enables, are not in the list.

See :doc:`../../develop/event` for more information on events.

Example
-------

This is the list on sandbox, where several spies share the last_stage_init
type and so run one after another::

    => event list
    Seq  Type                              Function  ID
      0  f   ft_fixup                  56a7e7dcfbf1  bootmeth_vbe_ft_fixup
      1  f   ft_fixup                  56a7e7dd0b4e  bootmeth_vbe_simple_ft_fixup
      2  c   last_stage_init           56a7e7f68d77  alloc_write_acpi_tables
      3  c   last_stage_init           56a7e7eb3033  debug_announce
      4  c   last_stage_init           56a7e7f13b80  efi_block_device_create
      5  c   last_stage_init           56a7e7f31fa4  install_smbios_table
      6  c   last_stage_init           56a7e7d9ed1f  last_stage_init
      7  c   last_stage_init           56a7e7ebfc9a  vfs_init
      8  8   misc_init_f               56a7e7d9e9a2  sandbox_early_getopt_check
      9  16  rng_seed                  56a7e800f192  h_rng_seed
      a  b   settings_r                56a7e7da411a  sandbox_settings
      b  1   test                      56a7e81ab368  h_adder_simple

Both the sequence number and the type are shown in hex, so the last two spies
above are numbers 10 and 11 in the list, and the rng_seed type is 22, the last
of the event types.

Return value
------------

The return value $? is 0 (true) if the command succeeds. It is 1 (false) if
no sub-command is given, if the sub-command is not list, or if list is given
an argument; in each of those cases the command shows its usage message.

Configuration
-------------

The event command is only available if CONFIG_CMD_EVENT=y.

See also
--------

* :doc:`cyclic<cyclic>` for the other list of functions U-Boot calls for
  itself, those run on a timer rather than an event
* :doc:`dm<dm>` for the devices which many of these spies are called for
* :doc:`bootstage<bootstage>` for the timings of the boot steps the events
  mark
