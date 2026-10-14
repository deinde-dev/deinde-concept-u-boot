.. SPDX-License-Identifier: GPL-2.0+

.. index::
   single: cls (command)

cls command
===========

Synopsis
--------

::

    cls

Description
-----------

The cls command clears the screen and moves the cursor to the top left
corner.

It sends the ANSI escape sequences ``ESC [2J`` and ``ESC [1;1H`` to the
console, which a serial terminal acts on. A video display is cleared by the
same sequence where CONFIG_VIDEO_ANSI=y, since the video console decodes
escape sequences for itself; otherwise the display is cleared directly.

The command also starts a new page, so that with CONFIG_CONSOLE_PAGER=y the
output which follows it is not paused part-way down an empty screen.

Example
-------

The command writes nothing to the console but the escape sequence, so a
terminal which acts on it shows only the prompt::

    => cls
    =>

Configuration
-------------

The cls command is only available if CONFIG_CMD_CLS=y.

Return value
------------

The return value $? is 0 (true) if the screen is cleared. It is 1 (false) if
the display cannot be cleared, which happens only where
CONFIG_VIDEO_ANSI=n and there is no video console to clear.

See also
--------

* :doc:`osd<osd>` for the text overlay, which this command leaves alone
* :doc:`bmp<bmp>` for drawing a BMP image on the display
* :doc:`2048<2048>` for a game which clears the screen for itself
* :doc:`coninfo<coninfo>` for the console devices the sequence is sent to
