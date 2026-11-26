.. SPDX-License-Identifier: GPL-2.0+

.. index::
   single: font (command)

font command
============

Synopsis
--------

::

    font list
    font select [<name> [<size>]]
    font size [<size>]
    font info

Description
-----------

The *font* command allows selection of the font to use on the video console.
It offers the bitmap fonts enabled in the build and, where the TrueType
console is in use, the TrueType fonts as well.

font list
~~~~~~~~~

This lists the available fonts, using the name of the font file in the build.
Any enabled bitmap fonts are listed as well.

font select
~~~~~~~~~~~

This selects a new font and optionally changes the size. If the name is not
provided, the default font is used, at its default size. The size cannot be
given on its own, since it follows the name.

Selecting a bitmap font switches the console to fixed-font mode, whatever size
is requested.

font size
~~~~~~~~~

This changes the font size only, keeping the current font. With no argument it
shows the current size.

A bitmap font has a fixed size, so this shows a dimension of its characters in
pixels — the height with the TrueType console, the width otherwise — and a new
value has no effect.

font info
~~~~~~~~~

This shows glyph rendering statistics, specifically the number of glyphs
rendered since the video console was set up.

This subcommand requires CONFIG_VIDEO_GLYPH_STATS=y.

Examples
--------

Sandbox enables two bitmap fonts and four TrueType ones::

    => font list
    8x16
    12x22
    nimbus_sans_l_regular
    cantoraone_regular
    ubuntu_light
    ubuntu_bold
    => font size
    18
    => font select cantoraone_regular 20
    => font size
    20
    => font select
    => font size
    18

This shows an example of selecting a bitmap font when TrueType is active::

    => font select 8x16
    => font size
    16

An unknown font name reports the error from the video console::

    => font select badname
    Failed (error -2)

This shows glyph rendering statistics::

    => font info
    glyphs rendered: 654

Return value
------------

The return value $? is 0 if the sub-command succeeds. It is 1 if there is no
video console, if the font cannot be selected, or if the sub-command is
missing, unknown, or given more arguments than it takes, in which case the
usage message is shown.

Configuration
-------------

The command is only available if CONFIG_CMD_SELECT_FONT=y, which requires
CONFIG_VIDEO=y and is enabled by default where the TrueType console is in use.

CONFIG_CONSOLE_TRUETYPE_GLYPH_BUF enables a pre-allocated buffer for glyph
rendering, avoiding malloc/free per character. The buffer starts at 4KB and
grows as needed via realloc().

CONFIG_CONSOLE_TRUETYPE_SCRATCH enables a scratch buffer for internal stbtt
allocations. Without this, the TrueType library performs around 5 allocations
per character (totalling ~26KB), creating malloc/free overhead and heap
fragmentation. With the scratch buffer, memory is allocated once at probe time
and reused for each character. CONFIG_CONSOLE_TRUETYPE_SCRATCH_SIZE sets the
buffer size (default 32KB), which is sufficient for most Latin characters.
Complex glyphs (CJK, emoji) or very large font sizes may need 64KB or more.
Allocations exceeding the buffer size fall back to malloc transparently.

CONFIG_VIDEO_GLYPH_STATS enables tracking of glyph-rendering statistics.

See also
--------

* :doc:`video<video>` for the display and the console drawn on it
* :doc:`osd<osd>` for the text overlay, which has a font of its own
* :doc:`cls<cls>` for clearing the video console
