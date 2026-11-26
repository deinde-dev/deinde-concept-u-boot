.. SPDX-License-Identifier: GPL-2.0+:

.. index::
   single: cedit (command)

cedit command
=============

Synopsis
--------

::

    cedit load <interface> <dev[:part]> <filename>
    cedit cb_load
    cedit read_fdt <interface> <dev[:part]> <filename>
    cedit write_fdt <interface> <dev[:part]> <filename>
    cedit read_env [-v]
    cedit write_env [-v]
    cedit read_cmos [-v] [dev]
    cedit write_cmos [-v] [dev]
    cedit dump
    cedit run

Description
-----------

The *cedit* command is used to load a configuration-editor description and allow
the user to interact with it.

It makes use of the expo subsystem.

The description is in the form of a devicetree file, as documented at
:ref:`expo_format`.

See :doc:`../../develop/cedit` for information about the configuration editor.

Each sub-command other than *load* and *cb_load* needs an editor to have been
loaded; they report `No expo loaded` if there is none.

cedit load
~~~~~~~~~~

Loads a configuration-editor description from a file. It creates a new cedit
structure ready for use. Initially no settings are read, so default values are
used for each object.

Any editor which is already loaded is destroyed, along with the settings it
holds.

.. _cedit_cb_load:

cedit cb_load
~~~~~~~~~~~~~

This is supported only on x86 devices booted from coreboot. It creates a new
configuration editor which can be used to edit CMOS settings.

cedit read_fdt
~~~~~~~~~~~~~~

Reads the user settings from a devicetree file and updates the cedit with those
settings.

cedit write_fdt
~~~~~~~~~~~~~~~

Writes the current user settings to a devicetree file. For each menu item the
selected ID, its value and its text string are written.

cedit read_env
~~~~~~~~~~~~~~

Reads the settings from the environment variables. For each menu item `<name>`,
cedit looks for a variable called `c.<name>` with the ID of the selected menu
item.

The `-v` flag enables verbose mode, where each variable is printed after it is
read.

cedit write_env
~~~~~~~~~~~~~~~

Writes the settings to environment variables. For each menu item the selected
ID, its value and its text string are written, similar to:

   env set c.<name> <selected_id>
   env set c.<name>-value <selected_id's value>
   env set c.<name>-str <selected_id's text string>

The `-v` flag enables verbose mode, where each variable is printed before it is
set.

cedit read_cmos
~~~~~~~~~~~~~~~

Reads the settings from locations in the CMOS RAM and updates the cedit with
them. The locations used are specified by the schema. See :ref:`expo_format`.

The `-v` flag enables verbose mode, which shows which CMOS locations were read.

Normally the first RTC device is used to hold the data. You can specify a
different device by name using the `dev` parameter.

cedit write_cmos
~~~~~~~~~~~~~~~~

Writes the settings to locations in the CMOS RAM. The locations used are
specified by the schema. See :ref:`expo_format`.

The `-v` flag enables verbose mode, which shows which CMOS locations were
updated.

Normally the first RTC device is used to hold the data. You can specify a
different device by name using the `dev` parameter.

cedit dump
~~~~~~~~~~

Dumps the current expo structure to the console. This shows the expo, its
scenes, objects, and their properties in a human-readable format. This is useful
for debugging and understanding the structure of the configuration editor.

cedit run
~~~~~~~~~

Runs the default configuration-editor event loop. This is very simple, just
accepting character input and moving through the objects under user control.
The implementation is at `cedit_run()`.

The editor is destroyed when the user saves the settings, so that a fresh one is
used for any future editing. Leaving the editor without saving keeps it, along
with the settings chosen while it was running.

Configuration
-------------

The cedit command is only available if CONFIG_CMD_CEDIT=y.

The 'cedit cb_load' subcommand is only available if CONFIG_COREBOOT_SYSINFO=y.

The 'cedit dump' subcommand is only available if CONFIG_CMD_CEDIT_DUMP=y.

Example
-------

This loads a configuration editor on sandbox and runs it, the user selecting
'Always On' for the AC Power setting. Pressing Escape leaves the editor without
saving, which is why nothing is written to the environment at this point::

    => cedit load hostfs - cedit.dtb
    => cedit run

The settings are still held by the editor, so they can be written to a
devicetree file and read back again::

    => cedit write_fdt hostfs - settings.dtb
    => load hostfs - 1000 settings.dtb
    1024 bytes read in 0 ms
    => fdt addr 1000
    Working FDT set to 1000
    => fdt print
    / {
        cedit-values {
            cpu-speed = <0x0000000a>;
            cpu-speed-value = <0x00000000>;
            cpu-speed-str = "2 GHz";
            power-loss = <0x0000000f>;
            power-loss-value = <0x00000001>;
            power-loss-str = "Always On";
            machine-name = [00];
        };
    };
    => cedit read_fdt hostfs - settings.dtb

This shows settings being stored in the environment. Note that read_env only
reads the ID of each setting, since that is enough to select the menu item::

    => cedit write_env -v
    c.cpu-speed=10
    c.cpu-speed-str=2 GHz
    c.cpu-speed-value=0
    c.power-loss=15
    c.power-loss-str=Always On
    c.power-loss-value=1
    c.machine-name=
    => print c.cpu-speed c.power-loss-str
    c.cpu-speed=10
    c.power-loss-str=Always On
    => cedit read_env -v
    c.cpu-speed=10
    c.power-loss=15
    c.machine-name=

This shows writing to CMOS RAM. Notice that the byte at 84 changes::

    => rtc read 80 8
    00000080: 00 00 00 00 00 16 37 06                          ......7.
    => cedit write_cmos -v
    Write 2 bytes from offset 80 to 84
    => rtc read 80 8
    00000080: 00 00 00 00 04 16 37 06                          ......7.
    => cedit read_cmos -v
    Read 2 bytes from offset 80 to 84

Here is an example with the device specified::

    => cedit write_cmos -v rtc@61
    Write 2 bytes from offset 80 to 84

This example shows editing coreboot CMOS-RAM settings. A script could be used
to automate this::

    => cbsysinfo
    Coreboot table at 500, size 5c4, records 1d (dec 29), decoded to 000000007dce3f40, forwarded to 000000007ff9a000

    CPU KHz     : 0
    Serial I/O port: 00000000
       base        : 00000000
       pointer     : 000000007ff9a370
       type        : 1
       base        : 000003f8
       baud        : 0d115200
       regwidth    : 1
       input_hz    : 0d1843200
       PCI addr    : 00000010
    Mem ranges  : 7
              id: type               ||   base        ||   size
               0: 10:table    0000000000000000 0000000000001000
               1: 01:ram      0000000000001000 000000000009f000
               2: 02:reserved 00000000000a0000 0000000000060000
               3: 01:ram      0000000000100000 000000007fe6d000
               4: 10:table    000000007ff6d000 0000000000093000
               5: 02:reserved 00000000fec00000 0000000000001000
               6: 02:reserved 00000000ff800000 0000000000800000
    option_table: 000000007ff9a018
     Bit  Len  Cfg  ID  Name
       0  180    r   0  reserved_memory
     180    1    e   4  boot_option            0:Fallback 1:Normal
     184    4    h   0  reboot_counter
     190    8    r   0  reserved_century
     1b8    8    r   0  reserved_ibm_ps2_century
     1c0    1    e   1  power_on_after_fail    0:Disable 1:Enable
     1c4    4    e   6  debug_level            5:Notice 6:Info 7:Debug 8:Spew
     1d0   80    r   0  vbnv
     3f0   10    h   0  check_sum
    CMOS start  : 1c0
       CMOS end    : 1cf
       CMOS csum loc: 3f0
    VBNV start  : ffffffff
    VBNV size   : ffffffff
    ...
    Unimpl.     : 10 37 40

Check that the CMOS RAM checksum is correct, then create a configuration editor
and load the settings from CMOS RAM::

    => cbcmos check
    => cedit cb_load
    => cedit read_cmos

Now run the cedit. In this case the user selected 'save' so `cedit run` returns
success::

    => if cedit run; then cedit write_cmos -v; fi
    Write 2 bytes from offset 30 to 38
    => echo $?
    0

Update the checksum in CMOS RAM::

    => cbcmos check
    Checksum 6100 error: calculated 7100
    => cbcmos update
    Checksum 7100 written
    => cbcmos check
    =>

This shows dumping the cedit::

    => cedit dump
    Expo: name 'name'
      display (null)
      cons (none)
      mouse (none)
      scene_id 0
      next_id 2a
      req_width 0
      req_height 0
      text_mode 0
      popup 0
      show_highlight 0
      mouse_enabled 0
      mouse_ptr 0000000000000000
      mouse_size 0x0
      mouse_pos (0,0)
      damage (0,0)-(0,0)
      done 0
      save 0
      last_key_ms 7046959059
      Theme:
        font_size 0
        white_on_black 0
        menu_inset 0
        menuitem_gap_y 0

    Scenes:
      Scene 6: name 'main'
        title_id 13 (main.title)
        highlight_id 0 ((none))
        Object 13 (main.title): type text
          flags
          bbox: (0,0)-(0,0)
          dims: 0x0
          Text: str_id 14 font_name '(default)' font_size 0
            str 'Test Configuration'
        Object 15 (main.prompt): type text
          flags
          bbox: (0,0)-(0,0)
          dims: 0x0
          Text: str_id 16 font_name '(default)' font_size 0
            str 'UP and DOWN to choose, ENTER to select'
        Object 8 (cpu-speed): type menu
          flags
          bbox: (0,0)-(0,0)
          dims: 0x0
          Menu: pointer_id 0 title_id 17 manual 0
            Item a: name 'cpu-speed.item-0' key_id 0 label_id 19 desc_id 0 preview_id 0
            Item b: name 'cpu-speed.item-1' key_id 0 label_id 1b desc_id 0 preview_id 0
            Item c: name 'cpu-speed.item-2' key_id 0 label_id 1d desc_id 0 preview_id 0
        Object 17 (cpu-speed.title): type text
          flags
          bbox: (0,0)-(0,0)
          dims: 0x0
          Text: str_id 18 font_name '(default)' font_size 0
            str 'CPU speed'
        Object 19 (cpu-speed.item-0.label): type text
          flags
          bbox: (0,0)-(0,0)
          dims: 0x0
          Text: str_id 1a font_name '(default)' font_size 0
            str '2 GHz'
        Object 1b (cpu-speed.item-1.label): type text
          flags
          bbox: (0,0)-(0,0)
          dims: 0x0
          Text: str_id 1c font_name '(default)' font_size 0
            str '2.5 GHz'
        Object 1d (cpu-speed.item-2.label): type text
          flags
          bbox: (0,0)-(0,0)
          dims: 0x0
          Text: str_id 1e font_name '(default)' font_size 0
            str '3 GHz'
        Object d (power-loss): type menu
          flags
          bbox: (0,0)-(0,0)
          dims: 0x0
          Menu: pointer_id 0 title_id 1f manual 0
            Item e: name 'power-loss.item-0' key_id 0 label_id 21 desc_id 0 preview_id 0
            Item f: name 'power-loss.item-1' key_id 0 label_id 23 desc_id 0 preview_id 0
            Item 10: name 'power-loss.item-2' key_id 0 label_id 25 desc_id 0 preview_id 0
        Object 1f (power-loss.title): type text
          flags
          bbox: (0,0)-(0,0)
          dims: 0x0
          Text: str_id 20 font_name '(default)' font_size 0
            str 'AC Power'
        Object 21 (power-loss.item-0.label): type text
          flags
          bbox: (0,0)-(0,0)
          dims: 0x0
          Text: str_id 22 font_name '(default)' font_size 0
            str 'Always Off'
        Object 23 (power-loss.item-1.label): type text
          flags
          bbox: (0,0)-(0,0)
          dims: 0x0
          Text: str_id 24 font_name '(default)' font_size 0
            str 'Always On'
        Object 25 (power-loss.item-2.label): type text
          flags
          bbox: (0,0)-(0,0)
          dims: 0x0
          Text: str_id 26 font_name '(default)' font_size 0
            str 'Memory'
        Object 11 (machine-name): type textline
          flags
          bbox: (0,0)-(0,0)
          dims: 0x0
          Textline: label_id 27 edit_id 12
            line_chars 14 pos 14
        Object 27 (machine-name.label): type text
          flags
          bbox: (0,0)-(0,0)
          dims: 0x0
          Text: str_id 28 font_name '(default)' font_size 0
            str 'Machine name'
        Object 12 (machine-name.edit): type text
          flags
          bbox: (0,0)-(0,0)
          dims: 0x0
          Text: str_id 29 font_name '(default)' font_size 0
            str ''

Return value
------------

The return value $? is 0 (true) if the sub-command succeeds, else 1 (false). A
missing, unknown or over-long sub-command shows the usage message and returns 1.

`cedit run` returns 1 if the user leaves the editor without saving, since in
that case the settings are not meant to be kept.

See also
--------

* :doc:`mouse<mouse>` for showing the mouse events the editor responds to
* :doc:`bootmenu<bootmenu>` for a simpler menu which needs no expo description
* :doc:`font<font>` for the fonts the editor draws its text with
