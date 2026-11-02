.. SPDX-License-Identifier: GPL-2.0+:

.. index::
   single: boot (command)

boot command
============

Synopsis
--------

::

    boot

Description
-----------

The boot command boots the board the way its environment describes: it runs
the commands held in the *bootcmd* environment variable, so it does the same
thing as *run bootcmd*. U-Boot runs that variable itself when the boot delay
expires, so *boot* carries on with the boot which a key press interrupted.

The command takes no arguments and has no options, since *bootcmd* says what
to do; anything on the command line is refused with a usage message. On a
board using standard boot *bootcmd* holds 'bootflow scan -lb' by default,
which scans the boot devices and boots the first bootflow it finds.

A *bootcmd* which runs *boot* again is refused with::

    'bootd' recursion detected

U-Boot sets a flag while it runs *bootcmd* for this. The simple command-line
parser hands that flag to each command of *bootcmd*, so the recursion is
caught there; the hush parsers do not pass it on.

The same command is available under the older name *bootd*, described in
:doc:`bootd<bootd>`.

Example
-------

Any commands may be booted, so *bootcmd* can be set to something harmless to
see what happens::

    => env set bootcmd 'echo Hello World'
    => boot
    Hello World

The return value is the one of the commands which run::

    => env set bootcmd true
    => boot; echo $?
    0
    => env set bootcmd false
    => boot; echo $?
    1

With nothing in *bootcmd* there is nothing to boot::

    => env set bootcmd
    => boot; echo $?
    1

Configuration
-------------

The command is only available if CONFIG_CMD_BOOTD=y, which is the default on
a board with CONFIG_CMD_BOOTM=y. It uses nothing from bootm, so a board
without that command can enable it as well.

Return value
------------

The return value $? is the return value of the commands in *bootcmd*. It is 1
(false) if *bootcmd* is not set, since there is then nothing to boot.

See also
--------

* :doc:`bootd<bootd>` for the older name of this command
* :doc:`run<run>` for running the commands held in any environment variable
* :doc:`bootflow<bootflow>` for the standard-boot scan which *bootcmd* starts
  on a board using standard boot
* :doc:`bootm<bootm>` for booting an image which is already in memory
* :doc:`bootcount<bootcount>` for the counter which, once past *bootlimit*,
  sends U-Boot to *altbootcmd* instead of *bootcmd*
