.. SPDX-License-Identifier: GPL-2.0+
.. Copyright 2022, Heinrich Schuchardt <xypron.glpk@gmx.de>

.. index::
   single: sound (command)

sound command
=============

Synopsis
--------

::

    sound init
    sound play [len [freq]]...

Description
-----------

The *sound* command is used to play one or multiple beep sounds.

sound init
    initializes the sound driver.

sound play
    plays a square wave sound. It does not depend on previously calling
    *sound init*.

len
    duration of the sound in ms, defaults to 1000 ms

freq
    frequency of the sound in Hz, defaults to 400 Hz

Each length and frequency play another beep, so a tune is given as a list of
pairs. A length at the end of the list may stand on its own, in which case the
frequency of the beep before it is used again.

A frequency of 0 Hz is silence, which gives a rest in a tune. So is any
frequency above half the sampling rate of the device, since a period of the
wave is then too short to hold a sample of each sign.

Examples
--------

Beep at 400 Hz for 1000 ms::

    sound play

Beep at 400 Hz for 600 ms::

    sound play 600

Beep at 500 Hz for 600 ms::

    sound play 600 500

Play melody::

    sound play 500 1047 500 880 500 0 500 1047 500 880 500 0 500 784 500 698 500 784 1000 698

Play two beeps at 500 Hz, the second of them for 200 ms::

    sound play 600 500 200

Configuration
-------------

The sound command is enabled by CONFIG_CMD_SOUND=y. It needs a sound device,
which CONFIG_SOUND=y and a driver for the board provide.

Return value
------------

The return value $? is set to 0 (true) if the command succeeds. If there is no
sound device, or the driver cannot set it up or play what is asked of it, the
return value $? is set to 1 (false) and the command prints the error it got.
It is also set to 1 when the command shows its usage message, which it does
when it is given no sub-command, or one it does not have.

See also
--------

* :doc:`dm<dm>` for listing the sound device along with the audio codec and
  i2s devices it plays through
* :doc:`pwm<pwm>` for driving a buzzer wired to a PWM output directly
