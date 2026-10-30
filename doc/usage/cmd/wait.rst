.. SPDX-License-Identifier: GPL-2.0+:

.. index::
   single: wait (command)

wait command
============

Synopsis
--------

::

    wait [job_id ...]

Description
-----------

The wait command waits until the background jobs started by
:doc:`spawn<spawn>` have finished. The shell gives up the processor while it
waits, which is what lets the jobs run, so this is where the output of a
job usually appears.

Each identifier is waited for in turn, and the return value is the one from
the last job waited for. An identifier whose job has already finished, or
which has no job at all, returns at once.

job_id
    Identifier of a job to wait for, from 1 to
    CONFIG_CMD_SPAWN_NUM_JOBS, as set in the *job_id* environment variable
    by *spawn*. Several may be given. When no identifier is given, every
    running job is waited for.

CTRL+C stops the command, which shows '<INTERRUPT>' and leaves the jobs
running. A job which is waiting for a timeout of its own, as
:doc:`sleep<sleep>` does, may take the CTRL+C instead: that job stops and
the wait returns its failure.

Example
-------

The return value of a job comes back from the wait::

    => spawn false; wait; echo $?
    1

Each running job has its own identifier::

    => spawn sleep 1; echo $job_id; spawn sleep 1; echo $job_id; wait
    1
    2

Jobs may be waited for one at a time, which gives the return value of each
rather than of the last::

    => spawn false; spawn true; wait 1; echo $?; wait 2; echo $?
    1
    0

Configuration
-------------

The command is only available if CONFIG_CMD_SPAWN=y.

Return value
------------

The return value $? is the return value of the last job waited for, so it
is 0 (true) where no job is waited for at all. It is 1 (false) if an
identifier is outside the range of job identifiers, in which case the usage
message is shown as well, or if CTRL+C stops the command.

See also
--------

* :doc:`spawn<spawn>` for starting a job and the *job_id* variable
* :doc:`sleep<sleep>` for a delay which lets the jobs run without waiting
  for them
