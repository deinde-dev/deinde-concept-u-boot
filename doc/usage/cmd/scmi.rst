.. SPDX-License-Identifier: GPL-2.0+:

.. index::
   single: scmi (command)

scmi command
============

Synopsis
--------

::

    scmi info
    scmi perm_dev <agent id> <device id> <flags>
    scmi perm_proto <agent id> <device id> <protocol id> <flags>
    scmi reset <agent id> <flags>

Description
-----------

Arm System Control and Management Interface (SCMI hereafter) is a set of
standardised interfaces to manage system resources, like clocks, power
domains, pin controls, reset and so on, in a system-wide manner.

An entity which provides those services is called a SCMI firmware, or SCMI
server. It may be implemented by EL3 software, by a dedicated system control
processor (SCP) or by something else again.

A user of SCMI interfaces, including U-Boot, is called a SCMI agent. It
issues commands, which are defined in each protocol for specific system
resources, to the SCMI server over a communication channel, called a
transport. Those interfaces are independent from the server's implementation
thanks to a transport layer.

For more details, see the `SCMI specification`_.

While most of system resources managed under SCMI protocols are implemented
and handled as standard U-Boot devices, for example clk_scmi, scmi command
provides additional management functionality against SCMI server.

scmi info
~~~~~~~~~
    Show base information about SCMI server and supported protocols

scmi perm_dev
~~~~~~~~~~~~~
    Allow or deny access permission to the device

scmi perm_proto
~~~~~~~~~~~~~~~
    Allow or deny access to the protocol on the device

scmi reset
~~~~~~~~~~
    Reset the platform resource settings which the agent has configured,
    optionally together with its access permissions

Parameters are used as follows:

<agent id>
    SCMI Agent ID, hex value

<device id>
    SCMI Device ID, hex value

    Please note that what a device means is not defined
    in the specification.

<protocol id>
    SCMI Protocol ID, hex value

    It must not be 0x10 (base protocol)

<flags>
    Flags to control the action, hex value

    For perm_dev and perm_proto, 0 denies access and 1 allows it.

    For reset, 0 keeps the access permissions already configured, while
    1 resets them along with the other resource settings.

    The remaining bits are reserved, so a server rejects a value which
    sets any of them. The values allowed may grow with the version of
    SCMI the server implements. See SCMI specification for more details.

Example
-------

Obtain basic information about SCMI server:

::

    => scmi info
    SCMI device: scmi
      protocol version: 0x20000
      # of agents: 3
          0: platform
        > 1: OSPM
          2: PSCI
      # of protocols: 4
          Power domain management
          Performance domain management
          Clock management
          Sensor management
      vendor: Linaro
      sub vendor: PMWG
      impl version: 0x20b0000

Ask for access permission to device#0:

::

    => scmi perm_dev 1 0 1

Ask for access to the clock management protocol, 0x14, on device#0:

::

    => scmi perm_proto 1 0 14 1

Reset configurations with all access permission settings retained:

::

    => scmi reset 1 0

A request the server refuses reports the error it returned, here from a
server which does not let this agent reset its access permissions:

::

    => scmi reset 1 1
    Reset failed (-13)

Return value
------------

The return value $? is 0 (true) if the sub-command succeeds and 1 (false) if
it fails. A sub-command which the server completes shows nothing, apart from
info, whose output is its result.

The command shows its usage message where there is no sub-command, where the
sub-command is not one of those listed above, where it is given the wrong
number of arguments, or where one of those arguments is not a hex value.

A sub-command fails where no SCMI agent is present, where the server does not
support the request, and where the server refuses it, for instance because
the agent or the device is not one it knows, or the flags set a reserved bit.

Configuration
-------------

The scmi command is only available if CONFIG_CMD_SCMI=y.
Default n because this command is mainly for debug purpose.

See also
--------

* :doc:`clk<clk>` for the rate of each clock the SCMI server provides
* :doc:`armffa<armffa>` for another standardised firmware interface, which
  reaches the secure partitions of the secure world

.. _`SCMI specification`: https://developer.arm.com/documentation/den0056/e/?lang=en
