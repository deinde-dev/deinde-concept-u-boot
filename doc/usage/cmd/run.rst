.. SPDX-License-Identifier: GPL-2.0+:

.. index::
   single: run (command)

run command
===========

Synopsis
--------

::

    run var [...]

Description
-----------

The run command runs the commands held in an environment variable. This is
how U-Boot keeps a script: the commands are stored as the value of a
variable with :doc:`setenv<setenv>` and the variable is named here to run
them.

The value is handed to the command interpreter as if it had been typed, so
it may hold several commands separated by ';' or by a newline, and those
commands may themselves use *run*. A script started this way can be left
early with :doc:`exit<exit>`.

Each name on the command line is run in turn. The first one which is not an
environment variable, or whose commands report a failure, stops the command;
the names after it are not run.

It is an alias for *env run*, described in :doc:`env`.

var
    Name of the variable holding the commands to run. Several names may be
    given.

Example
-------

Store two commands in a variable and run them::

    => setenv greet 'echo hello; echo world'
    => run greet
    hello
    world
    => printenv greet
    greet=echo hello; echo world

Each name is run in turn, and a name which is not defined stops the run::

    => setenv a 'echo one'
    => setenv b 'echo two'
    => run a b
    one
    two
    => run a nothere b
    one
    ## Error: "nothere" not defined

Configuration
-------------

The command is only available if CONFIG_CMD_RUN=y.

Return value
------------

The return value $? is 0 (true) if every variable is found and its commands
succeed. It is 1 (false) if a name is not an environment variable, or if no
name is given. Where a command in the script fails, the value the
interpreter gives for the script is the return value of *run*, so with the
hush parser an 'exit 2' in the script gives 2.

See also
--------

* :doc:`env<env>` for *env run*, which is this command, and the other
  environment subcommands
* :doc:`setenv<setenv>` for storing the commands in a variable
* :doc:`exit<exit>` for leaving a script before its end
* :doc:`source<source>` for running a script held in an image rather than in
  a variable
* :doc:`bootd<bootd>` for running *bootcmd*, which is what U-Boot does when
  it boots on its own
* :doc:`spawn<spawn>` for running the commands in a variable as a background
  job
