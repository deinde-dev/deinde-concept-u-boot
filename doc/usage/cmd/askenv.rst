.. SPDX-License-Identifier: GPL-2.0+:

.. index::
   single: askenv (command)

askenv command
==============

Synopsis
--------

::

    askenv name [message ...] [size]

Description
-----------

The askenv command displays a message, reads a value from stdin and stores
it in environment variable *name*. It is an alias for *env ask*.

name
    name of the environment variable

message
    message displayed while the command waits for the value to be entered
    from stdin. The words of the message are joined with single spaces and
    a space is added at the end. If no message is given, the prompt is
    ``Please enter 'name':`` followed by a space.

size
    maximum number of characters stored in environment variable *name*.
    This is in decimal number format (unlike in other commands where size
    values are hexadecimal). The default value of size is 1023
    (CONFIG_SYS_CBSIZE - 1).

The last argument is taken as the size if all of it converts to a decimal
number, otherwise it forms part of the message. Anything typed beyond size
characters is echoed but discarded.

If nothing is entered, variable *name* is deleted. Pressing Ctrl-C leaves
it as it is.

Example
-------

Set an environment variable env1 with neither a message nor a size:

::

    => askenv env1
    Please enter 'env1': val1
    => env print env1
    env1=val1

Set an environment variable env2 with both a message and a size, where
more than size characters are typed:

::

    => askenv env2 Please type-in a value for env2: 10
    Please type-in a value for env2: 1234567890123
    => env print env2
    env2=1234567890

Set an environment variable env3 with a size only:

::

    => askenv env3 10
    Please enter 'env3': val3
    => env print env3
    env3=val3

Return value
------------

The return value $? is set to 0 (true) if the value is stored, or if an
empty value deletes the variable. It is set to 1 (false) if no name is
given, if size is zero, in which case nothing is read, if the prompt is
cancelled with Ctrl-C, or if the variable cannot be changed.

Configuration
-------------

The askenv command is only available if CMD_ASKENV=y

See also
--------

* :doc:`env<env>` for *env ask*, which is this command, and the other
  environment subcommands
* :doc:`editenv<editenv>` for changing the current value of a variable
  rather than replacing it
* :doc:`printenv<printenv>` for showing the value which was entered
* *setenv* for setting a variable without prompting for it
