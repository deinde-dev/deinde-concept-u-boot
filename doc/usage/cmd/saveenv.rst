.. SPDX-License-Identifier: GPL-2.0+:

.. index::
   single: saveenv (command)

saveenv command
===============

Synopsis
--------

::

    saveenv

Description
-----------

The saveenv command writes the environment held in memory to persistent
storage, so that the variables are there on the next boot. Everything is
written, not just the variables which have changed, and the stored copy is
protected by a checksum.

The storage which is written is the one chosen by 'env select', or the
highest priority location if that command has not been used. Not every
location can be written: those which provide no save operation report 'not
possible' instead.

It is an alias for *env save*, described in :doc:`env`.

Example
-------

This uses sandbox, where the SPI flash location must be selected first since
the default is 'nowhere'::

    => setenv myvar wilma
    => env select SPIFlash
    Select Environment on SPIFlash: OK
    => saveenv
    Saving Environment to SPIFlash... SF: Detected m25p16 with page size 256 Bytes, erase size 64 KiB, total 2 MiB
    Erasing SPI flash...Writing to SPI flash...done
    Valid environment: 1
    OK

The stored copy survives a change made afterwards, since reading it back
replaces what is in memory::

    => setenv myvar
    => env load
    Loading Environment from SPIFlash... OK
    => printenv myvar
    myvar=wilma

With the default 'nowhere' location there is nothing to write to::

    => saveenv
    Saving Environment to nowhere... not possible

Configuration
-------------

The command is only available if CONFIG_CMD_SAVEENV=y and a storage location
is configured, since CONFIG_ENV_IS_DEFAULT=y removes it.

Return value
------------

The return value $? is 0 (true) if the environment is written. It is 1
(false) if the location has no save operation, is not initialised, or the
write fails.

See also
--------

* :doc:`env<env>` for *env save*, which is this command, and the other
  environment subcommands
* :doc:`eraseenv<eraseenv>` for removing the stored copy
* :doc:`setenv<setenv>` for changing a variable in memory
* :doc:`printenv<printenv>` for showing what is about to be written
