.. SPDX-License-Identifier: GPL-2.0+
.. Copyright 2021, Kory Maincent <kory.maincent@bootlin.com>

.. index::
   single: extension (command)

extension command
=================

Synopsis
--------

::

    extension scan
    extension list
    extension apply <extension number|all>

Description
-----------

The "extension" command proposes a generic U-Boot mechanism to detect
extension boards connected to the HW platform, and apply the appropriate
Device Tree overlays depending on the detected extension boards.

The "extension" command comes with three sub-commands:

 - "extension scan" makes the generic code call a board-specific extension
   function to retrieve the list of detected extension boards. The list is
   emptied first, so a second scan reports what is plugged in now rather
   than adding to what an earlier scan found.

 - "extension list" allows to list the detected extension boards. It says
   that nothing is registered until a scan has run.

 - "extension apply <number>|all" allows to apply the Device Tree
   overlay(s) corresponding to one, or all, extension boards

The latter requires a working devicetree, set with the
:doc:`fdt addr<fdt>` command, and two environment variables to exist:

 - extension_overlay_addr: the RAM address where to load the Device
   Tree overlays

 - extension_overlay_cmd: the U-Boot command to load one overlay.
   Indeed, the location and mechanism to load DT overlays is very setup
   specific.

The overlay to load is named in the extension_overlay_name variable, which
the command sets before running extension_overlay_cmd.

In order to enable this mechanism, board-specific code must provide a scan
function which fills in a list of "struct extension", each describing one
extension board, and register it with U_BOOT_EXTENSION(). In addition, the
board-specific code must select the SUPPORT_EXTENSION_SCAN Kconfig
boolean.

Example
-------

Sandbox provides two fictional extension boards, with an overlay each.
The paths below are relative to the build directory.

Load a devicetree and make it the working devicetree::

    => host load hostfs - $fdt_addr_r arch/sandbox/dts/sandbox.dtb
    22876 bytes read in 0 ms
    => fdt addr $fdt_addr_r
    Working FDT set to c00000

Detect the plugged extension boards::

    => extension scan
    Found 2 extension board(s).

List what was found, with the name of the overlay for each::

    => extension list
    Extension 0: extension board 0
            Manufacturer:           sandbox
            Version:                1.1
            Devicetree overlay:     overlay0.dtbo
            Other information:      Fictional extension board
    Extension 1: extension board 1
            Manufacturer:           sandbox
            Version:                1.1
            Devicetree overlay:     overlay1.dtbo
            Other information:      Fictional extension board

Say where to load an overlay and how::

    => setenv extension_overlay_addr 0x1000
    => setenv extension_overlay_cmd 'host load hostfs - ${extension_overlay_addr} arch/sandbox/dts/${extension_overlay_name}'

Apply the overlay of the first extension board, then the overlays of all
of them::

    => extension apply 0
    4459 bytes read in 0 ms
    => extension apply all
    4459 bytes read in 0 ms
    4459 bytes read in 0 ms

Each overlay adds a button, so the devicetree now has four::

    => fdt list /buttons
    buttons {
            compatible = "gpio-keys";
            btn4 {
            };
            btn3 {
            };
            btn1 {
            };
            btn2 {
            };
    };

On a real board the same sequence loads the overlays from storage::

    => load mmc 0:1 $fdt_addr_r /boot/dtb
    => fdt addr $fdt_addr_r
    => extension scan
    => setenv extension_overlay_addr 0x88080000
    => setenv extension_overlay_cmd 'load mmc 0:1 ${extension_overlay_addr} /boot/${extension_overlay_name}'
    => extension apply all

Return value
------------

The return value $? is set to 0 (true) if the sub-command succeeds and 1
(false) if it fails. A missing or unknown sub-command shows the usage
message and returns 1.

"extension list" fails when the board provides no extension device. It
succeeds when there is one and no scan has run yet, reporting that
nothing is registered.

"extension scan" reports no extension boards, rather than failing, when
the board provides no extension device.

"extension apply" fails when there is no working devicetree, when the
extension number is out of range, when either environment variable is
missing, or when the overlay cannot be loaded or applied.

Simple extension_board_scan function example
--------------------------------------------

.. code-block:: c

    static int foo_extension_board_scan(struct udevice *dev,
                                        struct alist *extension_list)
    {
        struct extension extension = {0};

        snprintf(extension.overlay, sizeof(extension.overlay), "overlay.dtbo");
        snprintf(extension.name, sizeof(extension.name), "extension board");
        snprintf(extension.owner, sizeof(extension.owner), "sandbox");
        snprintf(extension.version, sizeof(extension.version), "1.1");
        snprintf(extension.other, sizeof(extension.other), "Extension board information");
        if (!alist_add(extension_list, extension))
                return -ENOMEM;

        return 1;
    }

    U_BOOT_EXTENSION(foo_extension_name, foo_extension_board_scan);

    U_BOOT_DRVINFO(foo_extension_name) = {
        .name = "foo_extension_name",
    };

See also
--------

* :doc:`fdt<fdt>` for setting the working devicetree the overlays are
  applied to, and for inspecting the result
* :doc:`setenv<setenv>` for setting the two variables which say where an
  overlay is loaded from
