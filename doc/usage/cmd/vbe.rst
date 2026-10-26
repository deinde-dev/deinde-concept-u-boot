.. SPDX-License-Identifier: GPL-2.0+:

.. index::
   single: vbe (command)

vbe command
===========

Synopsis
--------

::

    vbe list
    vbe select [<bootmeth>]
    vbe info
    vbe state

Description
-----------

The `vbe` command looks at and controls Verified Boot for Embedded (VBE). See
:doc:`/develop/vbe` for more information about VBE.

vbe list
~~~~~~~~

Lists the VBE bootmeths. The 'Sel' column shows the one selected by
`vbe select`, if any.

vbe select
~~~~~~~~~~

Selects a VBE bootmeth, by name or sequence number, for use by the `vbe info`
subcommand. With no argument, the selection is cleared.

vbe info
~~~~~~~~

Shows information about the selected VBE bootmeth, such as the version of the
firmware which it boots.

vbe state
~~~~~~~~~

Shows which phases of U-Boot used VBE to load the next phase. For each phase
which relocated itself to make room for the next one, the margin left in its
memory is shown too.

Example
-------

This shows the VBE bootmeths on sandbox::

    => vbe list
      #  Sel  Device           Driver          Description
    ---  ---  --------------   --------------  -----------
      2       vbe              vbe_abrec_os    VBE A/B/recovery for OS
      3       firmware0        vbe_simple      VBE simple
    ---  ---  --------------   --------------  -----------

Configuration
-------------

The `vbe` command is enabled by CONFIG_CMD_VBE

Return value
------------

The return value $? is 0 (true) on success, 1 (false) on failure.
