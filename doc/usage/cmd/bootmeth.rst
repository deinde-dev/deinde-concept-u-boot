.. SPDX-License-Identifier: GPL-2.0+:

.. index::
   single: bootmeth (command)

bootmeth command
================

Synopsis
--------

::

    bootmeth list [-a]          - list selected bootmeths (-a for all)
    bootmeth order ["<bm> ..."] - select the order of bootmeths
    bootmeth set <bootmeth> <property> <value> - set optional property


Description
-----------

The `bootmeth` command is used to manage bootmeths. It can list them, change
the order in which they are used and set a property of one of them.

See :doc:`/develop/bootstd/index` for more information.


.. _bootmeth_order:

bootmeth order
~~~~~~~~~~~~~~

Selects which bootmeths to use and the order in which they are invoked. When
scanning bootdevs, each bootmeth is tried in turn to see if it can find a valid
bootflow. You can use this command to adjust the order or even to omit some
bootmeths.

The argument is a quoted list of bootmeths to use, by name. If global bootmeths
are included, they must be at the end, otherwise the scanning mechanism will not
work correctly.

With no argument at all the ordering is dropped, so that every bootmeth is used
again, in sequence order.

The command keeps the list in the `bootmeths` environment variable. Setting
that variable has the same effect as using this command, since standard boot
reads the new value as it is written.


bootmeth list
~~~~~~~~~~~~~

This lists the selected bootmeths, or all of them, if the `-a` flag is used.
The format looks like this:

=====  ===  ==================  =================================
Order  Seq  Name                Description
=====  ===  ==================  =================================
    0    0  extlinux            Extlinux boot from a block device
    1    1  efi                 EFI boot from an .efi file
    2    2  vbe                 VBE A/B/recovery for OS
 glob    3  firmware0           VBE simple
=====  ===  ==================  =================================

The fields are as follows:

Order:
    The order in which these bootmeths are invoked for each bootdev. If this
    shows as a hyphen, then the bootmeth is not in the current ordering. If it
    shows as 'glob', then this is a global bootmeth and should be at the end.

Seq:
    The sequence number of the bootmeth, i.e. the normal ordering if none is set

Name:
    Name of the bootmeth

Description:
    A friendly description for the bootmeth


bootmeth set
~~~~~~~~~~~~

Allows setting of bootmeth specific configuration. This allows finer grain
control over the boot process in specific instances.

The following configuration options are currently supported:

========  ===================  ========  ===============================
Property  Supported bootmeths  Values    Description
========  ===================  ========  ===============================
fallback  extlinux, pxe        0 | 1     Enable or disable fallback path
========  ===================  ========  ===============================

A bootmeth which has no properties at all reports that its `set_property`
method is missing.


Example
-------

This shows listing bootmeths on sandbox. All are present and in the normal
order::

    => bootmeth list
    Order  Seq  Name                Description
    -----  ---  ------------------  ------------------
        0    0  extlinux            Extlinux boot from a block device
        1    1  efi                 EFI boot from an .efi file
        2    2  vbe                 VBE A/B/recovery for OS
     glob    3  firmware0           VBE simple
    -----  ---  ------------------  ------------------
    (4 bootmeths)

Now the order is changed, to include only two of them, with efi first::

    => bootmeth order "efi extlinux"
    => bootmeth list
    Order  Seq  Name                Description
    -----  ---  ------------------  ------------------
        0    1  efi                 EFI boot from an .efi file
        1    0  extlinux            Extlinux boot from a block device
    -----  ---  ------------------  ------------------
    (2 bootmeths)

The -a flag shows all bootmeths so you can clearly see which ones are used and
which are not::

    => bootmeth list -a
    Order  Seq  Name                Description
    -----  ---  ------------------  ------------------
        1    0  extlinux            Extlinux boot from a block device
        0    1  efi                 EFI boot from an .efi file
        -    2  vbe                 VBE A/B/recovery for OS
     glob    3  firmware0           VBE simple
    -----  ---  ------------------  ------------------
    (4 bootmeths)

A name which no bootmeth has stops the command, leaving the ordering as it
was::

    => bootmeth order "efi fred"
    Unknown bootmeth 'fred'
    Failed (err=-19)

With no argument every bootmeth is used again::

    => bootmeth order
    => bootmeth list
    Order  Seq  Name                Description
    -----  ---  ------------------  ------------------
        0    0  extlinux            Extlinux boot from a block device
        1    1  efi                 EFI boot from an .efi file
        2    2  vbe                 VBE A/B/recovery for OS
     glob    3  firmware0           VBE simple
    -----  ---  ------------------  ------------------
    (4 bootmeths)

This enables the extlinux fallback path, then asks for a property which the
bootmeth does not have::

    => bootmeth set extlinux fallback 1
    => bootmeth set extlinux fred 1
    Invalid option
    Failed (err=-22)


Bootmeth set Example
--------------------

With the bootcount functionality enabled, when the bootlimit is reached, the
`altbootcmd` environment variable lists the command used for booting rather
than `bootcmd`. We can set the fallback configuration to cause the fallback
boot option to be preferred, to revert to a previous known working boot option
after a failed update for example. So if `bootcmd` is set to::

    bootflow scan -lb

We would set "altbootcmd" to::

    bootmeth set extlinux fallback 1; bootflow scan -lb


Return value
------------

The return value $? is 0 (true) if the command succeeds. It is 1 (false) if
the arguments are wrong, if `bootmeth order` is given a name which no bootmeth
has, or if `bootmeth set` is given fewer than three arguments, a bootmeth which
does not exist or one which does not support the property or the value.

For `bootmeth list` the return value is always 0, unless standard boot is
unavailable.


See also
--------

* :doc:`bootflow<bootflow>` for scanning with these bootmeths and booting what
  they find
* :doc:`bootdev<bootdev>` for the boot devices which each bootmeth scans
* :doc:`bootstd<bootstd>` for the state which holds the bootmeth order
* :doc:`vbe<vbe>` for the VBE bootmeths, which have settings of their own
* :doc:`/develop/bootstd/index` for standard boot documentation
