.. SPDX-License-Identifier: GPL-2.0+

.. index::
   single: poweroff (command)

poweroff command
================

Synopsis
--------

::

    poweroff

Description
-----------

The poweroff command switches the board off. It takes no arguments and does
not return when it succeeds, since nothing is left running to return to.

How the power is cut depends on the platform. Boards using the sysreset
uclass show 'poweroff ...', wait 100ms for the message to reach the console
and then ask each sysreset device in turn to perform SYSRESET_POWER_OFF,
giving a device which reports that the power-off is under way a further
second to finish. Other platforms provide a power-off routine of their own:
through PSCI firmware, through a PMIC such as the AXP family or TWL4030, or
through SoC-specific code, as Keystone 2 does.

Only boards which can switch themselves off have the command, so it is not
available everywhere.

Example
-------

On sandbox the board is the process, so the command exits it with a status
of 0::

    => poweroff
    poweroff ...

Where the power cannot be cut the command says so and carries on. On a PSCI
board whose firmware has no SYSTEM_OFF function::

    => poweroff
    poweroff ...
    Power off not supported on this platform

Configuration
-------------

The command is available if CONFIG_CMD_POWEROFF=y. With
CONFIG_SYSRESET_CMD_POWEROFF=y the sysreset uclass provides it, which is what
a PMIC or SoC driver selects when its device can switch the power off.

Return value
------------

The command does not return when the board goes off. If the power cannot be
cut, the return value $? is set to 1 (false). Giving the command an argument
shows the usage message instead of switching off, in which case $? is also
set to 1.

See also
--------

* :doc:`reset<reset>` for restarting the board instead of switching it off
* :doc:`panic<panic>` for stopping the board after reporting a problem, which
  switches it off with CONFIG_PANIC_POWEROFF=y
* :doc:`pmic<pmic>` for talking to the PMIC which cuts the power on many
  boards
