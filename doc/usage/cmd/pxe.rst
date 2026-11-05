.. SPDX-License-Identifier: GPL-2.0+:

.. index::
   single: pxe (command)

pxe command
===========

Synopsis
--------

::

    pxe get [-ipv6]
    pxe boot [pxefile_addr_r] [-ipv6]

Description
-----------

The pxe command downloads a PXELINUX configuration file over TFTP and boots
the images it names, following the rules used by PXELINUX_. It has two
sub-commands.

Standard boot does the same job through the *extlinux* boot method, described
in :doc:`bootmeth<bootmeth>`, so a board using standard boot has no need of
this command.

pxe get
~~~~~~~

This downloads a configuration file to the address held in the environment
variable *pxefile_addr_r*, which must be set. The file is looked for under
``pxelinux.cfg/`` on the TFTP server, using the first of these names which
exists:

* the name given by DHCPv4 option 209 or DHCPv6 option 60, if the board has
  CONFIG_BOOTP_PXE_DHCP_OPTION or CONFIG_DHCP6_PXE_DHCP_OPTION and the server
  sent one. No other name is tried in that case
* the value of the environment variable *pxeuuid*
* the MAC address of the current Ethernet device, as
  ``01-00-18-31-bf-50-9c``
* the IPv4 address of the board in upper-case hex, as ``C0A80002``, then the
  same with one character removed at a time, down to a single character
* ``default-<arch>-<soc>-<board>``, ``default-<arch>-<soc>`` and
  ``default-<arch>``, built from CONFIG_SYS_ARCH, CONFIG_SYS_SOC and
  CONFIG_SYS_BOARD
* ``default``

Each attempt prints a *Retrieving file:* line, so the console shows the search
as it happens. The name printed by the final *Config file ... found* message
is the value of *bootfile*, not the name of the file which was found; with
*bootfile* unset it shows ``<NULL>``.

pxe boot
~~~~~~~~

This parses the configuration file already in memory and boots the label it
selects, retrieving each kernel, initrd and devicetree it names over TFTP.
The address defaults to the environment variable *pxefile_addr_r*, so
``pxe get`` followed by ``pxe boot`` is the usual sequence. Where the file
holds a menu, the menu is shown and the default label is booted once
*pxe_timeout* expires.

Options
~~~~~~~

-ipv6
    Use IPv6 rather than IPv4. This is a parameter rather than an option: it
    must come last, after the address if one is given, and it is accepted
    only on a board with CONFIG_IPV6.

Example
-------

This is on a BeagleBone Black, with a TFTP server at 192.168.0.1 holding
``pxelinux.cfg/default-arm-am33xx-am335x``::

    => dhcp
    link up on port 0, speed 100, full duplex
    BOOTP broadcast 1
    DHCP client bound to address 192.168.0.2 (4 ms)
    => setenv pxefile_addr_r 0x82100000
    => pxe get
    missing environment variable: pxeuuid
    Retrieving file: pxelinux.cfg/01-00-18-31-bf-50-9c
    Using ethernet@4a100000 device
    TFTP from server 192.168.0.1; our IP address is 192.168.0.2
    Filename 'pxelinux.cfg/01-00-18-31-bf-50-9c'.
    Load address: 0x82100000
    Loading: *
    TFTP error: 'File not found' (1)
    Not retrying...
    Retrieving file: pxelinux.cfg/C0A80002
    ...
    Retrieving file: pxelinux.cfg/default-arm-am33xx-am335x
    Using ethernet@4a100000 device
    TFTP from server 192.168.0.1; our IP address is 192.168.0.2
    Filename 'pxelinux.cfg/default-arm-am33xx-am335x'.
    Load address: 0x82100000
    Loading: ##
             436.5 KiB/s
    done
    Bytes transferred = 74 (4a hex)
    Config file '<NULL>' found

The file names a single label::

    label Linux
        menu label Boot kernel
        kernel Image
        fdt system.dtb
        initrd rootfs.cpio.gz.u-boot

so ``pxe boot`` retrieves those three files and starts the kernel::

    => pxe boot
    Retrieving file: Image
    ...
    Retrieving file: system.dtb
    ...
    Retrieving file: rootfs.cpio.gz.u-boot
    ...
    ## Flattened Device Tree blob at 83000000
       Booting using the fdt blob at 0x83000000
    Starting kernel ...

Giving no sub-command shows the usage message::

    => pxe
    pxe - get and boot from pxe files

    Usage:
    pxe get [-ipv6] - try to retrieve a pxe file using tftp
    pxe boot [pxefile_addr_r] [-ipv6] - boot from the pxe file at pxefile_addr_r

Return value
------------

The return value $? is 0 if the configuration file was downloaded, or the
selected label was booted, and 1 otherwise. It is also 1 when *pxefile_addr_r*
is unset, when the address given to ``pxe boot`` is not a hex number, and when
no sub-command or an unknown one is given.

Note that ``pxe boot`` returns 0 for a file which names no label it can boot,
since parsing the file is all the command undertakes to do.

See also
--------

* :doc:`bootp<bootp>` for obtaining the network configuration which the
  search names depend on
* *tftpboot* for downloading a single file over TFTP
* :doc:`sysboot<sysboot>` for the same file format read from a local
  filesystem instead
* :doc:`bootm<bootm>` for booting an image which is already in memory

.. _PXELINUX: https://wiki.syslinux.org/wiki/index.php?title=PXELINUX
