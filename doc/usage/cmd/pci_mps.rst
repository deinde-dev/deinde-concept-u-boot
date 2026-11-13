.. SPDX-License-Identifier: GPL-2.0+

.. index::
   single: pci_mps (command)

pci_mps command
===============

Synopsis
--------

::

    pci_mps safe
    pci_mps peer2peer

Description
-----------

The pci_mps command sets the Maximum Payload Size (MPS) of every PCI Express
device in the system. The MPS is the largest amount of data a device may put
in a single transaction, and the PCI Express specification requires every
device on a path to agree on it: a device which is sent a payload larger than
it is configured for reports a malformed packet. U-Boot leaves each device at
its reset value of 128 bytes, so this command is the way to raise the MPS
before booting, or to lower it again so that later changes to the topology
remain safe.

Only the first letter of the policy is examined, so 'pci_mps s' and
'pci_mps p' work as well as the full words.

safe
    Set every device to the largest MPS which all of them support. The
    command reads the Max Payload Size Supported field of each device's PCI
    Express capability, takes the smallest, and writes that to every device.
    Any device whose own maximum is below that value keeps its own maximum.
    This is the policy Linux calls pcie_bus_safe.

peer2peer
    Set every device to 128 bytes, the smallest MPS and the one every device
    supports. That wastes bandwidth, but it is the only setting under which a
    device may be hot-added, or two devices may transfer to each other
    directly, without first checking what the rest of the system is using.
    This is the policy Linux calls pcie_bus_peer2peer.

Neither policy alters the Maximum Read Request Size.

The buses are walked in the order the driver model numbers them, so the
devices must be enumerated first, which :doc:`pci enum<pci>` does. Where no
PCI Express device is found at all, 'pci_mps safe' leaves the system alone
and prints nothing.

Example
-------

On sandbox, where the largest MPS all the emulated devices support is 256
bytes::

    => pci enum
    => pci_mps safe
    Setting MPS of all devices to 256B

Returning to the hotplug-safe setting::

    => pci_mps peer2peer
    Setting MPS of all devices to 128B

Configuration
-------------

The command is available if CONFIG_CMD_PCI_MPS=y, which requires
CONFIG_PCI=y.

Return value
------------

The return value $? is 0 (true) if the MPS of every device is set. It is 1
(false) if no policy is given, or the word given is neither 'safe' nor
'peer2peer', in which case the usage message is shown. A failure to read or
write a device's configuration space gives the error number from that access,
which is also false.

See also
--------

* :doc:`pci<pci>` for enumerating the devices this command configures, and for
  reading back the Device Control register it writes
