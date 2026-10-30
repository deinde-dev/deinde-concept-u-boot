.. SPDX-License-Identifier: GPL-2.0+:

.. index::
   single: time (command)

time command
============

Synopsis
--------

::

    time command [args...]

Description
-----------

The time command runs a single command and prints how long it took. The
command and its arguments are run exactly as if they had been typed on their
own, so a command's own options and arguments work unchanged.

The timing comes from get_timer(), the millisecond timer
:doc:`gettime<gettime>` prints, read either side of the command. The
resolution is therefore one timer tick, which is a millisecond on every
board, and the figure covers only the command itself: the time the
interpreter spends parsing the line is not counted.

The report is written as a blank line followed by a line of the form
'time: <seconds>.<milliseconds> seconds'. Where the command takes a minute or
more, the whole minutes are given first, as 'time: 2 minutes, 3.456 seconds'.

Only one command can be timed. The interpreter splits a line on ';' before
the command sees it, so 'time echo a; echo b' times the echo of 'a' alone. To
time a sequence, put it in an environment variable and time
:doc:`run<run>`.

command
    Name of the command to run and time.

args
    Arguments for that command, including any options it takes.

Example
-------

Time a command which does almost nothing::

    => time echo hello world
    hello world

    time: 0.000 seconds

Time a command which takes a known time, which shows the timing is real::

    => time sleep 1

    time: 1.000 seconds

A sequence of commands has to be timed through *run*::

    => setenv script 'echo one; sleep 1; echo two'
    => time run script
    one
    two

    time: 1.000 seconds

Over a minute the whole minutes are shown separately::

    => time sleep 61

    time: 1 minutes, 1.000 seconds

Configuration
-------------

The command is only available if CONFIG_CMD_TIME=y.

Return value
------------

The return value $? is the return value of the command which was run, so 0
(true) if it succeeds. It is 1 (false) if no command is given, or if the name
is not a command, or if the command itself fails. The time is reported
whatever the command returns; where the command never runs, the report gives
0.000 seconds.

See also
--------

* :doc:`gettime<gettime>` for the timer this command reads, when the two ends
  of an interval are wanted separately
* :doc:`timer<timer>` for measuring an interval which is not a single command
* :doc:`bootstage<bootstage>` for timings of the individual steps of the boot
* :doc:`sleep<sleep>` for waiting for a given time
* :doc:`run<run>` for running the commands in a variable, which is how a
  sequence is timed
* :doc:`dhry<dhry>` for a benchmark which does its own timing
