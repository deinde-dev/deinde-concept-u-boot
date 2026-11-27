.. SPDX-License-Identifier: GPL-2.0+:

.. index::
   single: unbind (command)

unbind command
==============

Synopsis
--------

::

    unbind <node path>
    unbind <class> <seq>
    unbind <class> <seq> <driver>

Description
-----------

The unbind command is used to unbind a device from a driver. This makes the
device unavailable in U-Boot.

The first two forms unbind a single device, named either by its device-tree
node path or by its device class and sequence number. The third form unbinds
those children of the named device which use a particular driver, leaving the
device itself bound.

node path
    path of the device's device-tree node

class
    device class name

seq
    sequence number of the device in the device class

driver
    name of the driver whose child devices are to be unbound

Example
-------

Given a system with a real time clock device with device path */pl031@9010000*
and using driver rtc-pl031 the device is unbound first by its node path and
then by its class and sequence number.

.. code-block::

    => dm tree
     Class     Seq    Probed  Driver                Name
    -----------------------------------------------------------
     root          0  [ + ]   root_driver           root_driver
    ...
     rtc           0  [   ]   rtc-pl031             |-- pl031@9010000
    ...
    => fdt addr $fdtcontroladdr
    Working FDT set to 7ed7fdb0
    => fdt print
    / {
            interrupt-parent = <0x00008003>;
            model = "linux,dummy-virt";
            #size-cells = <0x00000002>;
            #address-cells = <0x00000002>;
            compatible = "linux,dummy-virt";
    ...
            pl031@9010000 {
                    clock-names = "apb_pclk";
                    clocks = <0x00008000>;
                    interrupts = <0x00000000 0x00000002 0x00000004>;
                    reg = <0x00000000 0x09010000 0x00000000 0x00001000>;
                    compatible = "arm,pl031", "arm,primecell";
            };
    ...
    }
    => unbind /pl031@9010000
    => dm tree
     Class     Seq    Probed  Driver                Name
    -----------------------------------------------------------
     root          0  [ + ]   root_driver           root_driver
    ...
    => unbind /pl031@9010000
    Cannot find a device with path /pl031@9010000
    => bind /pl031@9010000 rtc-pl031
    => dm tree
     Class     Seq    Probed  Driver                Name
    -----------------------------------------------------------
     root          0  [ + ]   root_driver           root_driver
    ...
     rtc           0  [   ]   rtc-pl031             |-- pl031@9010000
    => unbind rtc 0

The third form removes children rather than the device it names. On the
sandbox board *bind-test-child2* has no children until one is bound to it:

.. code-block::

    => bind simple_bus 1 simple_bus
    => dm tree
     Class     Seq    Probed  Driver                Name
    -----------------------------------------------------------
     root          0  [ + ]   root_driver           root_driver
    ...
     simple_bus    0  [   ]   simple_bus            |-- bind-test
     phy           0  [   ]   phy_sandbox           |   |-- bind-test-child1
     simple_bus    1  [   ]   simple_bus            |   `-- bind-test-child2
     simple_bus   13  [   ]   simple_bus            |       `-- simple_bus
    ...
    => unbind simple_bus 1 simple_bus
    => dm tree
     Class     Seq    Probed  Driver                Name
    -----------------------------------------------------------
     root          0  [ + ]   root_driver           root_driver
    ...
     simple_bus    0  [   ]   simple_bus            |-- bind-test
     phy           0  [   ]   phy_sandbox           |   |-- bind-test-child1
     simple_bus    1  [   ]   simple_bus            |   `-- bind-test-child2
    ...

*bind-test-child2* is still bound, since the command removes its child and
not the device named on the command line.

Configuration
-------------

The unbind command is only available if CONFIG_CMD_BIND=y.

Return value
------------

The return value $? is 0 (true) if the command succeeds. It is 1 (false) if
the node path or the device class is not known, if no device is bound at the
given node path, if no device has the given sequence number, if no driver has
the given name, or if the device cannot be removed. Unbinding the children of
a device which has none is not an error.

See also
--------

* :doc:`bind<bind>` for attaching a driver to a device
* :doc:`dm<dm>` for the devices and uclasses driver model has bound
* :doc:`demo<demo>` for the example uclass used to show how driver model
  works
* :doc:`sb<sb>` for removing and unbinding a sandbox device named by its
  devicetree node
