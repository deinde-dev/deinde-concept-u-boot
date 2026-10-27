.. SPDX-License-Identifier: GPL-2.0+:

.. index::
   single: setenv (command)

setenv command
==============

Synopsis
--------

::

    setenv [-f] name [value ...]
    setenv -e [-guid guid][-nv][-bs][-rt][-at][-a][-v] [-i addr:size] name [value]

Description
-----------

The setenv command sets an environment variable to a value, or deletes it when
no value is given. Only the copy in memory is changed, so the new value is lost
on the next boot unless the environment is written to persistent storage with
:doc:`saveenv<saveenv>`.

The value is all the remaining arguments joined with a single space, so the
quoting applied by the shell decides where the value starts and ends. A name
may not contain an '=' character, since that is what separates the name from
the value in the stored environment.

Variables may carry access flags which make them read-only, write-once or
only writeable while they hold their default value. The *env flags* command
lists the ones in force; see :doc:`env`.

It is an alias for *env set*, described in :doc:`env`.

\-f
    Force the change, ignoring the access flags of the variable. This has no
    effect when CONFIG_ENV_ACCESS_IGNORE_FORCE=y, which reports an error
    instead.

\-e
    Set a UEFI variable rather than an environment variable. The options which
    go with it are described under *env set* in :doc:`env`.

name
    Name of the variable to set or delete.

value
    New value of the variable. With no value the variable is deleted.

Example
-------

Set a variable, change it and delete it::

    => setenv myvar wilma
    => printenv myvar
    myvar=wilma
    => setenv myvar one two three
    => printenv myvar
    myvar=one two three
    => setenv myvar
    => printenv myvar
    ## Error: "myvar" not defined

The *serial#* variable is write-once, so a second value is refused unless -f
is given::

    => setenv serial# 1234
    => setenv serial# 5678
    ## Error: Can't overwrite "serial#"
    ## Error inserting "serial#" variable, errno=1
    => setenv -f serial# 5678
    => printenv serial#
    serial#=5678

A name containing '=' is refused::

    => setenv my=var 1
    ## Error: illegal character '=' in variable name "my=var"

Configuration
-------------

The command is always available. UEFI variables are only supported if
CONFIG_CMD_NVEDIT_EFI=y.

Return value
------------

The return value $? is 0 (true) if the variable is set or deleted. It is 1
(false) if the name contains '=', if the access flags of the variable do not
allow the change, or if no name is given.

See also
--------

* :doc:`env<env>` for *env set*, which is this command, and the other
  environment subcommands
* :doc:`printenv<printenv>` for showing the value of a variable
* :doc:`editenv<editenv>` for changing the current value of a variable rather
  than replacing it
* :doc:`askenv<askenv>` for prompting the user for the value
* :doc:`saveenv<saveenv>` for writing the environment to persistent storage
