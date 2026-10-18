.. SPDX-License-Identifier: GPL-2.0+

.. index::
   single: wdt (command)

wdt command
===========

Synopsis
--------

::

    wdt list
    wdt dev [<name>]
    wdt start <timeout_ms> [flags]
    wdt stop
    wdt reset
    wdt expire [flags]

Description
-----------

The wdt command is used to control watchdog timers.

The *wdt list* command shows a list of all watchdog devices, with the name of
the driver for each in brackets. It probes nothing, so a device appears whether
or not anything has asked for it yet.

The *wdt dev* command called without argument shows the current watchdog device.
The current device is set when passing the name of the device as argument. A
name which no device has is reported and leaves the current device as it is.

The *wdt start* command starts the current watchdog timer. The watchdog
sub-system reports the timeout it has set, and how often U-Boot services the
device while it runs, which is a quarter of the *hw_margin_ms* property of the
devicetree node.

The *wdt stop* command stops the current watchdog timer. A watchdog whose
devicetree node has an *always-running* property cannot be stopped, so the
command reports the error the driver gives.

The *wdt reset* command services the current watchdog timer without stopping
it, so that the timeout starts again. It prints nothing. U-Boot does this by
itself while the watchdog runs, so the command is needed only around something
which keeps U-Boot from reaching its main loop.

The *wdt expire* command lets the current watchdog timer expire immediately,
which resets the board. There is nothing to show for it: the next output is the
board starting up again.

Every sub-command but *list* and *dev* needs a current device, and says
``No device set, use 'wdt dev' first`` where there is none.

A sub-command may be shortened to any leading part of its name which only one
sub-command has, so *l* means list and *d* means dev. *s* names neither start
nor stop, since both begin with it.

name
    name of the watchdog device

timeout_ms
    timeout interval in milliseconds

flags
    unsigned long value passed to the driver. The usage is driver specific.
    The value is ignored by most drivers.

Example
-------

This shows the three watchdogs of the sandbox devicetree. No device is current
to begin with, so one has to be chosen before the timer can be started::

    => wdt list
    wdt-gpio-toggle (wdt_gpio)
    wdt-gpio-level (wdt_gpio)
    wdt@0 (wdt_sandbox)
    => wdt dev
    No watchdog timer device set!
    => wdt dev wdt@0
    => wdt dev
    dev: wdt@0
    => wdt start 3000
    WDT:   Started wdt@0 with servicing every 50ms (3s timeout)
    => wdt reset
    => wdt stop

A name no watchdog has is refused, and so is stopping one which the devicetree
marks always-running::

    => wdt dev nosuch
    Can't get the watchdog timer: nosuch
    => wdt dev wdt-gpio-toggle
    => wdt stop
    Stopping watchdog timer failed (-95)

Configuration
-------------

The command is only available if CONFIG_CMD_WDT=y.

The servicing which *wdt start* reports happens only if CONFIG_WATCHDOG=y.
Without it U-Boot starts the watchdog and never services it, so the board
resets when the timeout runs out.

Return value
------------

The return value $? is set to 0 (true) if the command succeeds. It is set to 1
(false) if no device is current, if the name given to *wdt dev* matches no
device, or if the driver cannot do what is asked of it. It is also set to 1
when the command shows its usage message, which it does for a sub-command it
does not know, for *wdt start* without a timeout, and for a sub-command given
more arguments than it takes.

See also
--------

* :doc:`cyclic<cyclic>` for the list of functions U-Boot calls regularly,
  which is how it services a running watchdog
* :doc:`reset<reset>` for resetting the board without waiting for a timeout
