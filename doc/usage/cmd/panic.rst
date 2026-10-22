.. SPDX-License-Identifier: GPL-2.0+:

.. index::
   single: panic (command)

panic command
=============

Synopsis
--------

::

    panic [message]

Description
-----------

The panic command stops U-Boot in the same way as an unrecoverable internal
error does: it shows the message, then either hangs or resets the board,
depending on the configuration.

message
    text to show before stopping. The command takes at most one argument, so
    a message containing a space must be quoted. The text is shown as it
    stands; a percent sign in it has no special meaning.

Example
-------

With CONFIG_PANIC_HANG=y the board is left waiting for the user to reset it::

    => panic 'Unrecoverable error'
    Unrecoverable error
    ### ERROR ### Please RESET the board ###

Sandbox is configured that way and exits at that point with a status of 1.

Without that option the board resets instead::

    => panic 'Unrecoverable error'
    Unrecoverable error
    resetting ...

Configuration
-------------

If CONFIG_PANIC_HANG=y, the user has to reset the board manually. If
CONFIG_PANIC_POWEROFF=y, the board is powered off instead, falling back to
hanging if that fails.

Return value
------------

The command does not return, so there is no return value to check. Giving it
more than one argument shows the usage message instead of panicking, in which
case the return value $? is set to 1 (false).

See also
--------

* :doc:`reset<reset>` for restarting the board without reporting a problem
* :doc:`exception<exception>` for provoking a processor exception, which is
  another way of bringing U-Boot down
* *poweroff* for switching the board off, which is what a panic does with
  CONFIG_PANIC_POWEROFF=y
