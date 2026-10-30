.. SPDX-License-Identifier: GPL-2.0+:

.. index::
   single: bootd (command)
   single: boot (command)

bootd command
=============

Synopsis
--------

::

    bootd
    boot

Description
-----------

The bootd command executes the command stored in the environment variable
*bootcmd*, i.e. it does the same thing as *run bootcmd*. It takes no
arguments and has no options.

The same command is also called *boot*, which is the name U-Boot itself uses;
*bootd* is kept for backward compatibility.

Example
-------

::

    => env set bootcmd 'echo Hello World'
    => bootd
    Hello World
    => env set bootcmd true
    => bootd; echo $?
    0
    => env set bootcmd false
    => bootd; echo $?
    1

Return value
------------

The return value $? of the bootd command is the return value of the command in
the environment variable *bootcmd*. It is 1 if *bootcmd* is not set, since
there is then nothing to boot.

See also
--------

* :doc:`run<run>` for running the commands held in any environment variable
* :doc:`bootflow<bootflow>` for the standard-boot scan which *bootcmd* starts
  on a board using standard boot
* :doc:`bootm<bootm>` for booting an image which is already in memory
* :doc:`bootcount<bootcount>` for the counter which, once past *bootlimit*,
  sends U-Boot to *altbootcmd* instead of *bootcmd*
