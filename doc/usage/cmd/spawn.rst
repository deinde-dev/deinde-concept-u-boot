.. SPDX-License-Identifier: GPL-2.0+:

.. index::
   single: spawn (command)

spawn command
=============

Synopsis
--------

::

    spawn command [args...]

Description
-----------

The spawn command starts a command as a background job and returns at once,
so the shell carries on with the next command while the job runs. Each job
is given an identifier, which spawn stores in the *job_id* environment
variable, and *wait* uses that to wait for the job and pick up its result.

The command is run with the arguments as they are given, so the shell has
already expanded any variable and split the line at ';' by the time spawn
sees them. A job which is to run several commands therefore keeps them in
an environment variable and is started with :doc:`run<run>`.

Jobs are scheduled cooperatively: a job runs only while the shell gives up
the processor, which it does in *wait*, while it reads a command line and
whenever a command delays. With CONFIG_UTHREAD each udelay() and mdelay()
passes the processor to the next job until its time is up, as does each
call to schedule(), so any command which waits for time to pass lets the
jobs run, :doc:`sleep<sleep>` being the simplest. A job's output therefore
appears at the next wait rather than when it is started, and the examples
below give a whole sequence on one line so that nothing comes between the
commands.

The commands worth starting as jobs are those which spend their time
waiting on hardware, polling it with a timeout, such as bringing up USB,
obtaining an address over DHCP or downloading over TFTP. Two of these
started together take about as long as the slower one rather than the sum
of both: on an i.MX93 EVK with a USB disk behind a hub,
``spawn usb start; spawn dhcp; wait`` takes 4.5 seconds where running the
two in turn takes 8.1. A command which only computes, such as crc32 or cp,
never gives up the processor, so it runs to the end in one go and gains
nothing from being a job.

command
    Command to run as a job, with its arguments.

Up to CONFIG_CMD_SPAWN_NUM_JOBS jobs may run at the same time. The
identifiers run from 1 to that number and each becomes free again once its
job has finished.

Example
-------

Start a job and pick up its output at the wait::

    => spawn echo hello; printenv job_id; wait
    job_id=1
    hello

Two jobs which sleep for two seconds take two seconds together rather than
four::

    => spawn sleep 2; spawn sleep 2; time wait

    time: 2.000 seconds

Jobs take turns whenever one of them delays, so two scripts started this way
make progress together. Here the first job prints its second line while the
second is still sleeping::

    => setenv ja 'echo a1; sleep 0.1; echo a2'
    => setenv jb 'echo b1; sleep 0.3; echo b2'
    => spawn run ja; spawn run jb; wait
    a1
    b1
    a2
    b2

Configuration
-------------

The command is only available if CONFIG_CMD_SPAWN=y. It needs
CONFIG_UTHREAD=y, which provides the cooperative scheduling, and
CONFIG_CMD_SPAWN_NUM_JOBS sets how many jobs may run at once, 16 by
default.

Return value
------------

The return value $? is 0 (true) if the job starts. It is 1 (false) if no
job identifier is free, if there is not enough memory for the job, or if no
command is given, in which case the usage message is shown as well. The
return value of the command itself is reported by *wait*.

See also
--------

* *wait* for waiting until a job has finished and reading its return value
* :doc:`run<run>` for running several commands as one job
* :doc:`sleep<sleep>` for a delay which lets the jobs run
