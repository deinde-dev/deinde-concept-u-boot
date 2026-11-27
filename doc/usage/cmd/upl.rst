.. SPDX-License-Identifier: GPL-2.0+:

.. index::
   single: upl (command)

upl command
===========

Synopsis
--------

::

    upl info [-v]
    upl read <addr>
    upl write

Description
-----------

The *upl* command is used to test U-Boot's support for the Universal Payload
Specification (UPL) firmware standard (see :doc:`../upl`). It allows creation of
a fake handoff for use in testing.


upl info
~~~~~~~~

Show whether U-Boot booted from a UPL handoff:

    UPL state
        active or inactive (indicates whether U-Boot booted from UPL or not)

With -v, and only while UPL is active, the handoff itself is shown as well:

    fit
        Address of the FIT which was loaded

    conf_offset
        FIT offset of the chosen configuration

For each image the following information is shown:

    Image number
        Images are numbered from 0

    load
        Address to which the image was loaded

    size
        Size of the loaded image

    offset
        FIT offset of the image

    description
        Description of the image


upl read
~~~~~~~~

Decode the UPL handoff structure at <addr>. This allows testing that the handoff
can be obtained; the information read is discarded rather than becoming the
state which *upl info* shows.


upl write
~~~~~~~~~

Write a fake UPL handoff structure. The `upladdr` environment variable is set to
the address of this structure and `uplsize` is set to the size.


Example
-------

This shows checking whether U-Boot booted from a UPL handoff. Sandbox did
not, so the state is inactive::

    => upl info
    UPL state: inactive

This shows how to use the command to write and display the handoff::

    => upl write
    UPL handoff written to 66f55a0 size 662
    => print upladdr
    upladdr=66f55a0
    => print uplsize
    uplsize=662
    => fdt addr ${upladdr}
    Working FDT set to 66f55a0
    => fdt print
    / {
        #address-cells = <0x00000001>;
        #size-cells = <0x00000001>;
        options {
            upl-params {
                smbios = <0x00000123>;
                acpi = <0x00000456>;
                bootmode = "default", "s3";
                addr-width = <0x0000002e>;
                acpi-nvs-size = <0x00000100>;
            };
            upl-image {
                fit = <0x00000789>;
                conf-offset = <0x00000234>;
                image-1 {
                    load = <0x00000001>;
                    size = <0x00000002>;
                    offset = <0x00000003>;
                    description = "U-Boot";
                };
                image-2 {
                    load = <0x00000004>;
                    size = <0x00000005>;
                    offset = <0x00000006>;
                    description = "ATF";
                };
            };
        };
        memory@0x10 {
            reg = <0x00000010 0x00000020 0x00000030 0x00000040 0x00000040 0x00000050>;
        };
        memory@0x70 {
            reg = <0x00000070 0x00000080>;
            hotpluggable;
        };
        memory-map {
            acpi@0x11 {
                reg = <0x00000011 0x00000012 0x00000013 0x00000014 0x00000015 0x00000016 0x00000017 0x00000018 0x00000019 0x0000001a>;
                usage = "acpi-reclaim";
            };
            u-boot@0x21 {
                reg = <0x00000021 0x00000022>;
                usage = "boot-data";
            };
            efi@0x23 {
                reg = <0x00000023 0x00000024>;
                usage = "runtime-code";
            };
            empty@0x25 {
                reg = <0x00000025 0x00000026 0x00000027 0x00000028>;
            };
            acpi-things@0x29 {
                reg = <0x00000029 0x0000002a>;
                usage = "acpi-nvs", "runtime-code";
            };
        };
        reserved-memory {
            mmio@0x2b {
                reg = <0x0000002b 0x0000002c>;
            };
            memory@0x2d {
                reg = <0x0000002d 0x0000002e 0x0000002f 0x00000030>;
                no-map;
            };
        };
        serial@0xf1de0000 {
            compatible = "ns16550a";
            clock-frequency = <0x001c2000>;
            current-speed = <0x0001c200>;
            reg = <0xf1de0000 0x00000100>;
            reg-io-shift = <0x00000002>;
            reg-offset = <0x00000040>;
            virtual-reg = <0x20000000>;
            access-type = "mmio";
        };
        framebuffer@0xd0000000 {
            compatible = "simple-framebuffer";
            reg = <0xd0000000 0x10000000>;
            width = <0x00000500>;
            height = <0x00000500>;
            stride = <0x00001400>;
            format = "a8r8g8b8";
        };
    };
    =>

This shows reading the handoff back again::

    => upl read ${upladdr}
    Reading UPL at 66f55a0
    =>

This shows the detail of a handoff which U-Boot really did boot from::

    => upl info -v
    UPL state: active
    fit 1264000
    conf_offset 2a4
    image 0: load 200000 size 105f5c8 offset a4: U-Boot 2024.07-00770-g739ee12e8358 for sandbox board


Return value
------------

The return value $? is 0 (true) if the command succeeds. It is 1 (false) if
the handoff cannot be written, if the data at <addr> cannot be decoded as a
handoff, or if the arguments are wrong, in which case the command shows its
usage message.


See also
--------

* :doc:`../upl` for the specification and U-Boot's support for it
* :doc:`fdt<fdt>` for inspecting the handoff which *upl write* leaves in memory
* :doc:`printenv<printenv>` for showing the `upladdr` and `uplsize` variables
  which *upl write* sets
