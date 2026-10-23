.. SPDX-License-Identifier: GPL-2.0+

.. index::
   single: shim (command)

shim command
============

Synopsis
--------

::

    shim debug [[-n] <0/1>]

Description
-----------

The ``shim`` command adjusts the settings which Shim reads when it starts.
Shim is a UEFI bootloader which checks the signature of the operating-system
loader it hands over to, so that a machine using secure boot can start a
signed kernel. It takes its settings from EFI variables, which this command
writes before Shim is started. This is useful when debugging booting,
particularly on x86 machines.

shim debug
~~~~~~~~~~

Show or set Shim's verbose-debugging mode. With the mode enabled, Shim
reports what it is doing as it boots, which helps in working out why a
machine does not start. The setting is held in the ``SHIM_VERBOSE``
variable in the Shim-lock GUID namespace
(``605dab50-e046-4300-abb6-3dd810dd8b23``), as four bytes holding 0 or 1.

With no value, the command shows the current setting: 1 if the mode is
enabled, 0 if it is disabled or the variable does not exist.

0/1
    new setting, read as a hex number. Zero disables the mode and any other
    value enables it.

-n
    write the variable as non-volatile, so that the setting survives a
    reset. Without this option the variable is volatile and lasts only until
    the board restarts. The option must come before the value.

UEFI does not allow the attributes of an existing variable to change, so
writing the setting the other way round fails. Delete the variable first::

    => env set -e -guid 605dab50-e046-4300-abb6-3dd810dd8b23 SHIM_VERBOSE

Example
-------

This shows the mode being enabled, then read back::

    => shim debug
    0
    => shim debug 1
    => shim debug
    1

Asking for a setting which survives a reset while a volatile variable
exists fails, since the attributes cannot change::

    => shim debug -n 1
    Failed to write variable (err=8000000000000002)

Return value
------------

The return value $? is set to 0 (true) if the command succeeds. It is set to
1 (false) if UEFI cannot be started, or the variable cannot be read or
written. A sub-command which the command does not know, or -n with no value
after it, shows the usage message and sets $? to 1 as well.

Configuration
-------------

The command is available if CONFIG_CMD_SHIM=y, which needs EFI-variable
support through CONFIG_CMD_NVEDIT_EFI. It is enabled by default on sandbox
and in the EFI app.

See also
--------

* :doc:`bootefi<bootefi>` for starting an EFI application such as Shim
* :doc:`efidebug<efidebug>` for setting the UEFI boot variables and boot
  options
* :doc:`env<env>` for reading and writing EFI variables directly
