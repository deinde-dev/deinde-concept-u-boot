.. SPDX-License-Identifier: GPL-2.0-or-later:

.. index::
   single: env (command)

env command
===========

Synopsis
--------

::

	env ask name [message] [size]
	env callbacks
	env default [-k] [-f] (-a | var [...])
	env delete [-f] var [...]
	env edit [-e] name
	env erase
	env exists name
	env export [-t | -b | -c] [-s size] addr [var ...]
	env flags
	env grep [-e] [-n | -v | -b] string [...]
	env import [-d] [-t [-r] | -b | -c] addr [size] [var ...]
	env indirect to from [default]
	env info [-d] [-p] [-q]
	env load
	env print [-a | name ...]
	env print -e [-guid guid] [-n] [-s] [-v] [name ...]
	env run var [...]
	env save
	env select [target]
	env set [-f] name [value]
	env set -e [-nv][-bs][-rt][-at][-a][-i addr:size][-v] name [value]

Description
-----------

The *env* command is used to handle the U-Boot (:doc:`../environment`) or
the UEFI variables. Each of its sub-commands is described below.

The next commands are kept as alias and for compatibility:

+ :doc:`askenv <askenv>` = *env ask*
+ :doc:`editenv <editenv>` = *env edit*
+ :doc:`eraseenv <eraseenv>` = *env erase*
+ :doc:`grepenv <grepenv>` = *env grep*
+ :doc:`printenv <printenv>` = *env print*
+ :doc:`run <run>` = *env run*
+ :doc:`saveenv <saveenv>` = *env save*
+ :doc:`setenv <setenv>` = *env set*

Ask
~~~

The *env ask* command asks for the new value of an environment variable
(alias :doc:`askenv`).

    name
        name of the environment variable.

    message
        message to be displayed while the command waits for the value to be
        entered from stdin. If no message is specified, the prompt is
        ``Please enter 'name':`` followed by a space.

    size
        maximum number of characters that will be stored in the environment
        variable name. This is in decimal number format (unlike in
        other commands where size values are hexadecimal). The default
        value of size is 1023 (CONFIG_SYS_CBSIZE - 1).

Callbacks
~~~~~~~~~

The *env callbacks* command prints callbacks and their associated variables.

Default
~~~~~~~

The *env default* command resets the selected variables in the U-Boot
environment to their default values.

    var
        list of variable names. If variable is not part of default
        environment, it is deleted with a warning message on console.
    \-a
        all U-Boot environment.
    \-f
        forcibly, overwrite read-only/write-once variables.
    \-k
        keep the variables which are not part of the default environment,
        rather than deleting them.

Delete
~~~~~~

The *env delete* command deletes the selected variables from the U-Boot
environment.

    var
        name of the variable to delete.
    \-f
        forcibly, overwrite read-only/write-once variables.

Edit
~~~~

The *env edit* command edits an environment variable.

    name
        name of the variable.
    \-e
        use the expo graphical editor rather than the command line. This
        requires CONFIG_CMD_EDITENV_EXPO.

Erase
~~~~~

The *env erase* command erases the U-Boot environment.

Exists
~~~~~~

The *env exists* command tests for existence of variable.

    name
        name of the variable.

Export
~~~~~~

The *env export* command exports the U-Boot environment in memory; on success,
the variable $filesize will be set.

    addr
        memory address where environment gets stored.
    var
        list of variable names that get included into the export.
        Without arguments, the whole environment gets exported.
    \-b
        export as binary format (name=value pairs separated by
        list end marked by double "\0\0").
    \-t
        export as text format; if size is given, data will be
        padded with '\0' bytes; if not, one terminating '\0'
        will be added.
    \-c
        Export as checksum protected environment format as used by
        'env save' command.
    \-s size
        size of output buffer.

Flags
~~~~~

The *env flags* command prints variables that have non-default flags.

Grep
~~~~

The *env grep* command searches environment, list environment name=value pairs
matching the requested 'string'.

    string
        string to search in U-Boot environment.
    \-e
        enable regular expressions.
    \-n
        search string in variable names.
    \-v
        search string in variable values.
    \-b
        search both names and values (default).

Import
~~~~~~

The *env import* command imports environment from memory.

    addr
        memory address to read from.
    size
        length of input data; if missing, proper '\0' termination is mandatory
        if var is set and size should be missing (i.e. '\0' termination),
        set size to '-'.
    var
        List of the names of the only variables that get imported from
        the environment at address 'addr'. Without arguments, the whole
        environment gets imported.
    \-d
        delete existing environment before importing if no var is passed;
        if vars are passed, if one var is in the current environment but not
        in the environment at addr, delete var from current environment;
        otherwise overwrite / append to existing definitions.
    \-t
        assume text format; either "size" must be given or the text data must
        be '\0' terminated.
    \-r
        handle CRLF like LF, that means exported variables with a content which
        ends with \r won't get imported. Used to import text files created with
        editors which are using CRLF for line endings.
        Only effective in addition to -t.
    \-b
        assume binary format ('\0' separated, "\0\0" terminated).
    \-c
        assume checksum protected environment format.

Indirect
~~~~~~~~

The *env indirect* command sets one variable to the value of another, so
that a script can read a variable whose name it works out as it runs.

    to
        name of the variable to set.
    from
        name of the variable to read.
    default
        value to use when *from* is not set. Without it an unset *from* is
        an error.

Info
~~~~

The *env info* command displays (without argument) or evaluates the U-Boot
environment information.

    \-d
        evaluate if the default environment is used.
    \-p
        evaluate if environment can be persisted.
    \-q
        quiet output,  use only for command result, by example with
        'test' command.

Load
~~~~

The *env load* command loads the U-Boot environment from persistent storage.

Print
~~~~~

The *env print* command prints the selected variables in U-Boot environment or
in UEFI variables.

    name
        list of variable name.
    \-a
        all U-Boot environment, when 'name' is absent.
    \-e
        print UEFI variables, all by default if 'name' is not provided.
    \-guid guid
        print only the UEFI variables matching this GUID (any by default)
        with guid format = "xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx".
    \-n
        don't show hexadecimal dump of value for UEFI variables.
    \-s
        sort UEFI variables by name before displaying.
    \-v
        show verbose output for UEFI variables including GUID, attributes, data
        size and hexadecimal dump of value.

Run
~~~

The *env run* command runs commands in an environment variable.

    var
        name of the variable.

Save
~~~~

The *env save* command saves the U-Boot environment in persistent storage.

Select
~~~~~~

The *env select* command selects a U-Boot environment target. It is useful to
override the default location when several U-Boot environment backends are
available.

    target
        name of the U-Boot environment backend to select: EEPROM, EXT4, FAT,
        Flash, MMC, NAND, nowhere, NVRAM, OneNAND, Remote, SATA, SPIFlash, UBI.


Set
~~~

The *env set* command sets or delete (when 'value' or '-i' are absent)
U-Boot variable in environment or UEFI variables (when -e is specified).

    name
        variable name to modify.
    value
        when present, set the environment variable 'name' to 'value'
        when absent, delete the environment variable 'name'.
    \-f
        forcibly, overwrite read-only/write-once U-Boot variables.
    \-e
        update UEFI variables.
    \-nv
        set non-volatile attribute (UEFI).
    \-bs
        set boot-service attribute (UEFI).
    \-rt
        set runtime attribute (UEFI).
    \-at
        set time-based authentication attribute (UEFI).
    \-a
        append-write (UEFI).
    \-i addr:size
        use <addr,size> as variable's value (UEFI).
    \-v
        verbose message (UEFI).

Example
-------

Print the U-Boot environment variables::

    => env print -a
    => env print bootcmd stdout

Update environment variable in memory::

    => env set bootcmd "run distro_bootcmd"
    => env set stdout "serial,vidconsole"

Delete environment variable in memory::

    => env delete bootcmd
    => env set bootcmd

Set a variable to the value of another, with a default for the case where
that one is not set::

    => env indirect dtb_name board_dtb default.dtb

Reset environment variable to default value, in memory::

    => env default bootcmd
    => env default ipaddr serverip
    => env default -a

Save current environment in persistent storage::

    => env save

Restore the default environment in persistent storage::

    => env erase

Create a text snapshot/backup of the current settings in RAM
(${filesize} can be use to save the snapshot in file)::

    => env export -t ${backup_addr}

Re-import this snapshot, deleting all other settings::

    => env import -d -t ${backup_addr}

Save environment if the default environment is used and persistent storage is
selected::

    => if env info -p -d -q; then env save; fi

Configuration
-------------

The env command is always available but some sub-commands depend on
configuration options:

ask
    CONFIG_CMD_ASKENV

callbacks
    CONFIG_CMD_ENV_CALLBACK

edit
    CONFIG_CMD_EDITENV, CONFIG_CMD_EDITENV_EXPO for the '-e' option

exists
    CONFIG_CMD_ENV_EXISTS

erase
    CONFIG_CMD_ERASEENV

export
    CONFIG_CMD_EXPORTENV

flags
    CONFIG_CMD_ENV_FLAGS

grep
    CONFIG_CMD_GREPENV, CONFIG_REGEX for '-e' option

import
    CONFIG_CMD_IMPORTENV

indirect
    CONFIG_CMD_NVEDIT_INDIRECT

info
    CONFIG_CMD_NVEDIT_INFO

load
    CONFIG_CMD_NVEDIT_LOAD

print
    CONFIG_CMD_NVEDIT_EFI for UEFI variables support ('-e' option),
    additionally CONFIG_HEXDUMP to display content of UEFI variables

run
    CONFIG_CMD_RUN

save
    CONFIG_CMD_SAVEENV

select
    CONFIG_CMD_NVEDIT_SELECT

set
    CONFIG_CMD_NVEDIT_EFI for UEFI variables support ('-e' option)

Return value
------------

The return value $? is 0 (true) when the sub-command succeeds. A
sub-command which is not known, or one given an option or an argument it
does not accept, shows the usage message above and returns 1 (false).

Two sub-commands report more than a simple failure:

*env exists*
    1 (false) when the variable is not set.

*env print*
    the number of names which are not set, so 1 (false) for a single name
    which is missing.

*env info* returns 0 (true) only when every evaluation it is asked for
holds, so 'env info -d -p -q' succeeds on a board which is using the
default environment and can also persist it.

See also
--------

* :doc:`bootcount<bootcount>` for the counter which *bootlimit* and
  *altbootcmd* control
* :doc:`run<run>` for *env run*, which runs the commands held in a variable
