.. SPDX-License-Identifier: GPL-2.0+:

.. index::
   single: coninfo (command)

coninfo command
===============

Synopsis
--------

::

    coninfo

Description
-----------

The coninfo command provides a list of available console input and output
devices and their assignment as stdin, stdout, stderr console devices.

Each device is listed with the letters I and O, showing whether it can be
used for input, for output or for both. The console files the device serves
are listed under it, so a device which appears with no file below it is
available but unused.

With CONFIG_CMD_CONSOLE_EXTRA=y the listing gains a header and a third
column, giving the uclass of each device which comes from driver model.

If CONFIG_SYS_CONSOLE_IS_IN_ENV=y, the assignment is controlled by the
environment variables stdin, stdout, stderr which contain a comma separated
list of device names. With CONFIG_CONSOLE_MUX=y a change to one of those
variables takes effect at once, so coninfo shows the new assignment.

Example
-------

This shows the devices a sandbox build has, then moves stderr from the two
devices it starts on to the serial device alone::

    => coninfo
    List of available devices

    Device  File               Uclass
    |-- sandbox_serial (IO)    serial
    |-- cros-ec-keyb (I)       keyboard
    |   |-- stdin
    |-- serial (IO)
    |   |-- stdin
    |   |-- stdout
    |   |-- stderr
    |-- nulldev (IO)
    |-- nc (IO)
    |-- vidconsole (O)         vidconsole0
    |   |-- stdout
    |   |-- stderr
    => setenv stderr serial
    => coninfo
    List of available devices

    Device  File               Uclass
    |-- sandbox_serial (IO)    serial
    |-- cros-ec-keyb (I)       keyboard
    |   |-- stdin
    |-- serial (IO)
    |   |-- stdin
    |   |-- stdout
    |   |-- stderr
    |-- nulldev (IO)
    |-- nc (IO)
    |-- vidconsole (O)         vidconsole0
    |   |-- stdout

Configuration
-------------

The coninfo command is only available if CONFIG_CMD_CONSOLE=y. The uclass
column is shown if CONFIG_CMD_CONSOLE_EXTRA=y, which costs about 200 bytes.

Return value
------------

The return value $? is 0 (true) when the command runs, since it only
reports. It is 1 (false) if more arguments are given than the command
accepts, as the command takes none.

See also
--------

* :doc:`ufetch<ufetch>` for the console assignment alongside the other board
  details
* :doc:`cls<cls>` for clearing the display which these devices drive
