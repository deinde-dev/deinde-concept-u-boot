.. SPDX-License-Identifier: GPL-2.0+

.. index::
   single: scp03 (command)

scp03 command
=============

Synopsis
--------

::

    scp03 enable
    scp03 provision

Description
-----------

The *scp03* command calls into a Trusted Application running in a Trusted
Execution Environment to control the Secure Channel Protocol 03 established
between the processor and a secure element.

This protocol encrypts all the communication between the processor and the
secure element using a set of keys which both of them hold. 'scp03 enable'
asks the Trusted Application to bring the channel up with the keys which are
there. 'scp03 provision' asks it to generate a new set, write them to the
secure element and bring the channel up with those, so the keys are rotated.

Neither sub-command takes an argument. The work happens in the Trusted
Application, so the command reports only whether the request succeeded and
says nothing about the keys themselves.

The command needs CONFIG_CMD_SCP03 and a Trusted Execution Environment which
provides the SCP03 Trusted Application: OP-TEE on a real board, or the one
sandbox emulates, which CONFIG_OPTEE_TA_SCP03 adds.

Example
-------

This brings the channel up and then rotates the keys, on sandbox::

    => scp03 enable
    SCP03 is enabled
    => scp03 provision
    SCP03 is provisioned

Return value
------------

The return value $? is 0 (true) if the Trusted Application does what it is
asked.

It is 1 (false) if no Trusted Execution Environment can be found, or if the
request fails, in which case the command says which request that was. It
shows its usage message where the sub-command is missing, is not one the
command has, or is given an argument.

See also
--------

* :doc:`optee<optee>` for checking that the Trusted Execution Environment
  answers at all
* :doc:`avb<avb>` for the other command which works through a Trusted
  Application

For some information on the internals implemented in the TEE, please
check the GlobalPlatform documentation on `Secure Channel Protocol '03'`_

.. _Secure Channel Protocol '03':
   https://globalplatform.org/wp-content/uploads/2014/07/GPC_2.3_D_SCP03_v1.1.2_PublicRelease.pdf
