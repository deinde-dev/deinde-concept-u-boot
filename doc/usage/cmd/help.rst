.. SPDX-License-Identifier: GPL-2.0+:

.. index::
   single: help (command)

help command
============

Synopsis
--------

::

    help [<command>...]
    ? [<command>...]

Description
-----------

The help command shows what this U-Boot can do. With no arguments it lists
every command built into the binary; with one or more command names it shows
the detailed help for each of them.

*?* is an alias for *help* and behaves identically, so '?' on its own lists
the commands and '? md' shows the help for *md*. It exists because a question
mark is quicker to type at the prompt.

command
    name of a command to describe. Several may be given and each is looked up
    in turn

The listing
~~~~~~~~~~~

The listing holds one line per command, sorted by name, giving the command
name padded to CFG_SYS_HELP_CMD_WIDTH columns (10 by default) and the short
description from the command's table entry. It covers only the commands this
build has, so it is the authoritative answer to what is available.

The listing can be long, since sandbox has over 260 commands, so pressing
Ctrl-C stops it. The command then fails rather than succeeding, which lets a
script tell an abort from a complete listing.

The detailed help
~~~~~~~~~~~~~~~~~

For each name given, help shows the name and its short description, a blank
line, then 'Usage:' and the command's long help text. The long help is what
the command itself shows when it is given arguments it does not understand.

The name may be abbreviated, so long as the abbreviation matches exactly one
command: 'help vers' describes *version*. Where it matches more than one, or
none, nothing is found. A length suffix is ignored too, since the lookup stops
at the first dot, so 'help md.b' describes *md*.

A command whose entry carries no long help text shows
'No additional help available.' in place of it. With CONFIG_SYS_LONGHELP
disabled no command has any, so the 'Usage:' part is dropped for all of them
and only the first line is shown.

An unknown or ambiguous name produces::

    Unknown command 'xyz' - try 'help' without arguments for list of all known commands

The remaining names are still looked up, so one bad name does not hide the
help for the good ones.

Example
-------

This lists the commands sandbox has, with most of the 264 lines left out::

    => help
    2048      - The 2048 game
    ?         - alias for 'help'
    [         - alias for 'test'
    abootimg  - manipulate Android Boot Image
    acpi      - ACPI tables
    addr_find - find a load address suitable for a file
    ...
    zip       - zip a memory region
    =>

This asks the alias for the help of the command itself::

    => ? help
    help - print command description/usage

    Usage:
    help
        - print brief description of all commands
    help command ...
        - print detailed usage of 'command'
    =>

This asks about a command which is not there::

    => help xyz
    Unknown command 'xyz' - try 'help' without arguments for list of all known commands

    =>

Return value
------------

The return value $? is 0 (true) only for a listing which runs to the end.
It is 1 (false) when Ctrl-C stops the listing, and it is also 1 whenever a
command name is given, even when the command is found and its help is shown,
since cmd_usage() reports the usage message as a failure. A script therefore
cannot use $? to tell whether a command exists.

Configuration
-------------

The command is present when CONFIG_CMD_HELP=y, which is the default. The
'?' alias comes with it.

CONFIG_SYS_LONGHELP=y, also the default, is what gives each command its long
help text; without it there is nothing for help to show beyond the short
description. CFG_SYS_HELP_CMD_WIDTH sets the column the descriptions line up
in, 10 unless a board overrides it.

See also
--------

* :doc:`history<history>` for the commands which have been typed, rather than
  the ones which are available
* :doc:`config<config>` for the CONFIG options of this build, which decide
  which commands it has
