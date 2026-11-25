.. SPDX-License-Identifier: GPL-2.0+
.. Copyright 2022-2023 Arm Limited and/or its affiliates <open-source-office@arm.com>

.. index::
   single: armffa (command)

armffa command
==============

Synopsis
--------

::

   armffa getpart <partition UUID>
   armffa ping <partition ID>
   armffa devlist

Description
-----------

armffa is a command showcasing how to use the FF-A bus and how to invoke its
operations.

This provides a guidance to the client developers on how to call the FF-A bus
interfaces.

The command also allows to gather secure partitions information and ping these
partitions.

The command is also helpful in testing the communication with secure
partitions.

None of the sub-commands takes an option, and each wants its argument.

getpart
    lists the partitions offering the service named by *partition UUID*,
    giving the ID, the number of execution contexts and the properties of
    each. The UUID is in big-endian format.

ping
    sends a data pattern to the partition named by *partition ID*, in hex,
    and shows the five words the partition sends back, least-significant
    word first. A partition ID of 0 is refused.

devlist
    shows the name and address of the FF-A device, and of its driver and the
    driver's operations

Example
-------

The following examples are run on Corstone-1000 platform.

* ping

::

   corstone1000# armffa ping 0x8003
   SP response:
   [LSB]
   fffffffe
   0
   0
   0
   0

* ping (failure case)

::

   corstone1000# armffa ping 0x8004
   Sending direct request error (-22)

* getpart

::

   corstone1000# armffa getpart 33d532ed-e699-0942-c09c-a798d9cd722d
   Partition: id = 8003 , exec_ctxt 1 , properties 3

* getpart (failure case)

::

   corstone1000# armffa getpart 33d532ed-e699-0942-c09c-a798d9cd7221
   INVALID_PARAMETERS: Unrecognized UUID
   Failure in querying partition(s) info (error code: -22)

* devlist

::

   corstone1000# armffa devlist
   device arm_ffa, addr fdf41c30, driver arm_ffa, ops fffc0e98

Return value
------------

The return value $? is set to 0 (true) if the command succeeds. It is set to
1 (false) if the FF-A bus cannot be found, if the secure world rejects the
request, or if the command is given an option, an unknown sub-command, a
partition ID of 0 or the wrong number of arguments. In the last four cases
the usage message is shown as well.

See also
--------

* :doc:`scmi<scmi>` for another standardised firmware interface, which
  manages system resources rather than secure partitions
* :doc:`../../arch/arm64.ffa` for the FF-A bus itself and how a client
  calls it

Configuration
-------------

The command is available if CONFIG_CMD_ARMFFA=y and CONFIG_ARM_FFA_TRANSPORT=y.
