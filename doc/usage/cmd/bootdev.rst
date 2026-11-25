.. SPDX-License-Identifier: GPL-2.0+:

.. index::
   single: bootdev (command)

bootdev command
===============

Synopsis
--------

::

    bootdev list [-p]              - list all available bootdevs
    bootdev hunt [-l | <spec>]     - use hunt drivers to find bootdevs
    bootdev order [clear | <spec>] - view or update the bootdev order
    bootdev select [<bd>]          - select a bootdev by name, label or seq
    bootdev info [-p]              - show information about a bootdev

Description
-----------

The `bootdev` command is used to manage bootdevs. It can list available
bootdevs, select one and obtain information about it.

See :doc:`/develop/bootstd/index` for more information about bootdevs in general.


bootdev list
~~~~~~~~~~~~

This lists available bootdevs

Scanning with `-p` causes the bootdevs to be probed. This happens automatically
when they are used.

The list looks something like this:

===  ======  ======  ========  =========================
Seq  Probed  Status  Uclass    Name
===  ======  ======  ========  =========================
  0   [ + ]      OK  mmc       mmc@7e202000.bootdev
  1   [   ]      OK  mmc       sdhci@7e300000.bootdev
  2   [   ]      OK  ethernet  smsc95xx_eth.bootdev
===  ======  ======  ========  =========================


The fields are as follows:

Seq:
    Sequence number in the scan, used to reference the bootflow later

Probed:
    Shows a plus (+) if the device is probed, empty if not.

Status:
    Shows the status of the device. Typically this is `OK` meaning that there is
    no error. If you use -p and an error occurs when probing, then this shows
    the error number. You can look up Linux error codes to find the meaning of
    the number.

Uclass:
    Name of the media device's Uclass. This indicates the type of the parent
    device (e.g. MMC, Ethernet).

Name:
    Name of the bootdev. This is generated from the media device appended
    with `.bootdev`


bootdev hunt
~~~~~~~~~~~~

This hunts for new bootdevs, or shows a list of hunters.

Use `-l` to list the available bootdev hunters. The Used column shows a star
for each hunter which has been used, since a hunter runs only once.

To run hunters, specify the name of the hunter to run, e.g. "mmc". A name with
a sequence number after it ("mmc1") selects the same hunter as the name on its
own. If no name is provided, all hunters are run. A name which matches no
hunter is not an error; nothing is hunted for in that case.


bootdev order
~~~~~~~~~~~~~

This allows the bootdev order to be examined or set. With no argument the
current ordering is shown, one item per line.

The argument can either be 'clear' or a space-separated list of labels. Each
label can be the name of a bootdev (e.g. "mmc1.bootdev"), a bootdev sequence
number ("3") or a media uclass ("mmc") with an optional sequence number (mmc2).

Use `bootdev order clear` to clear any ordering and use the default.

By default, the ordering is defined by the `boot_targets` environment variable
or, failing that, the bootstd node in the devicetree ("bootdev-order" property).
If no ordering is provided, then a default one is used.

Note that this command does not check that the ordering is valid. In fact the
meaning of the ordering depends on what the bootflow iterator discovers when it
is used. Invalid entries will result in no bootdevs being found for that entry,
so they are effectively skipped.

bootdev select
~~~~~~~~~~~~~~

Use this to select a particular bootdev. You can select it by the sequence
number or name, as shown in `bootdev list`, or by a label such as "mmc1",
which names the media device the bootdev belongs to. The sequence number is
read as hexadecimal, as `bootdev list` shows it.

Once a bootdev is selected, you can use `bootdev info` to look at it or
`bootflow scan` to scan it.

If no bootdev name or number is provided, then any existing bootdev is
unselected.


bootdev info
~~~~~~~~~~~~

This shows information on the current bootdev, with the format looking like
this:

=========  =======================
Name       `mmc@7e202000.bootdev`
Sequence   0
Status     Probed
Uclass     mmc
Bootflows  1 (1 valid)
=========  =======================

Most of the information is the same as `bootdev list` above. The new fields
are:

Name
    Name of the bootdev

Status
    Shows `Probed` if the device is probed, `OK` if not. If `-p` is used and the
    device fails to probe, an error code is shown.

Bootflows
    Indicates the number of bootflows attached to the bootdev. This is 0
    unless you have used 'bootflow scan' on the bootflow, or on all bootflows.

If no bootdev is selected, the command asks for `bootdev select` to be used
first and returns 1.


Example
-------

This example, from sandbox, shows listing the available bootdevs, selecting
one by its label and getting information about it::

    => bootdev list
    Seq  Probed  Status  Uclass    Name
    ---  ------  ------  --------  ------------------
      0   [   ]      OK  qfw       qfw_sandbox.bootdev
      1   [   ]      OK  ethernet  eth@10002000.bootdev
      2   [   ]      OK  ethernet  eth@10003000.bootdev
      3   [   ]      OK  ethernet  sbe5.bootdev
      4   [   ]      OK  ethernet  eth@10004000.bootdev
      5   [   ]      OK  ethernet  phy-test-eth.bootdev
      6   [   ]      OK  ethernet  dsa-test-eth.bootdev
      7   [   ]      OK  ethernet  dsa-test@0.bootdev
      8   [   ]      OK  ethernet  dsa-test@1.bootdev
      9   [   ]      OK  mmc       mmc2.bootdev
      a   [   ]      OK  mmc       mmc1.bootdev
      b   [   ]      OK  mmc       mmc0.bootdev
      c   [   ]      OK  spi_flash spi.bin@0.bootdev
      d   [   ]      OK  spi_flash spi.bin@1.bootdev
      e   [   ]      OK  fs        vfs_rootfs.bootdev
    ---  ------  ------  --------  ------------------
    (15 bootdevs)
    => bootdev select mmc2
    => bootdev info
    Name:      mmc2.bootdev
    Sequence:  9
    Status:    Probed
    Uclass:    mmc
    Bootflows: 0 (0 valid)

Selecting a bootdev probes it, so the status is `Probed` here. The bootflow
count stays at 0 until `bootflow scan` has looked at the bootdev.

This shows running one of the available hunters, then listing them::

    => bootdev hunt mmc
    Hunting with: mmc
    => bootdev hunt -l
    Prio  Used  Uclass           Hunter
    ----  ----  ---------------  ---------------
       6        ethernet         eth_bootdev
       3        fs               fs_bootdev
       5        ide              ide_bootdev
       2     *  mmc              mmc_bootdev
       4        nvme             nvme_bootdev
       4        qfw              qfw_bootdev
       4        host             sandbox_bootdev
       4        scsi             scsi_bootdev
       4        spi_flash        sf_bootdev
       5        usb              usb_bootdev
       2        virtio           virtio_bootdev
    (total hunters: 11)

This shows viewing and changing the ordering::

    => bootdev order
    mmc2
    mmc1
    => bootdev order 'mmc1 usb'
    => bootdev order
    mmc1
    usb
    => bootdev order clear
    => bootdev order
    No ordering

Note that the labels of a new order are separated by spaces, so they must be
quoted to reach the command as a single argument.


Return value
------------

The return value $? is 0 (true) if the command succeeds. It is 1 (false) if
the arguments are wrong, if `bootdev select` cannot find the bootdev it is
given, if `bootdev info` is used with no bootdev selected, or if a hunter
fails.

See also
--------

* :doc:`bootflow<bootflow>` for scanning the bootdevs and booting what is
  found on them
* :doc:`bootmeth<bootmeth>` for the methods used to scan each bootdev
* :doc:`bootstd<bootstd>` for the state which holds the selected bootdev and
  the order
* :doc:`bootctl<bootctl>` for boot control, whose schema can set the bootdev
  order
