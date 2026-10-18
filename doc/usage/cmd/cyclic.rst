.. SPDX-License-Identifier: GPL-2.0+

.. index::
   single: cyclic (command)

cyclic command
==============

Synopsis
--------

::

    cyclic list
    cyclic demo <cycletime_ms> <delay_us>

Description
-----------

The cyclic command provides information about the cyclic functions, which are
run from schedule() while U-Boot waits for something.

The cyclic list command provides a list of the currently registered
cyclic functions.

This shows the following information:

Function
    Function name

cpu-time
    Total time spent in this cyclic function.

Frequency
    Frequency of execution of this function, e.g. 100 times/s for a
    period of 10ms.

The cyclic demo command registers a demo function which does nothing but
delay for <delay_us> microseconds each time it is called, with <cycletime_ms>
milliseconds between calls. It is there to show the infrastructure working.
Each use registers another function, since nothing removes the previous one.

See :doc:`../../develop/cyclic` for more information on cyclic functions.

Example
-------

This registers the demo function with a cycle time of 100ms, then shows the
list a second later. The demo has run ten times by then, as its cycle time
asks, and has spent the 10us it is given in each call::

    => cyclic list
    function: video_idle, cpu-time: 22 us, frequency: 144.80 times/s
    => cyclic demo 100 10
    Registered function "cyclic_demo" to be executed all 100ms
    => cyclic list
    function: cyclic_demo, cpu-time: 100 us, frequency: 10.00 times/s
    function: video_idle, cpu-time: 226 us, frequency: 10.92 times/s

The newest function comes first, so the demo is at the top of the list.

Return value
------------

The return value $? is 0 (true) if the command succeeds. It is 1 (false) if
no sub-command is given, if the sub-command is not one of the above, or if
demo is given the wrong number of arguments; in each of those cases the
command shows its usage message.

Configuration
-------------

The cyclic command is only available if CONFIG_CMD_CYCLIC=y.

See also
--------

* :doc:`event<event>` for the other list of functions U-Boot calls for itself,
  those run on an event rather than a timer
* :doc:`wdt<wdt>` for the watchdog, which is one user of a cyclic function
* :doc:`timer<timer>` for reading the time the frequency is worked out from
* :doc:`sleep<sleep>` for waiting a given time, during which cyclic functions
  run
