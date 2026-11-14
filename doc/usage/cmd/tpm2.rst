.. SPDX-License-Identifier: GPL-2.0+:

.. index::
   single: tpm2 (command)

tpm2 command
============

Synopsis
--------

::

    tpm2 device [<num>]
    tpm2 info
    tpm2 state
    tpm2 init
    tpm2 autostart
    tpm2 startup <mode> [off]
    tpm2 self_test <type>
    tpm2 clear <hierarchy> [<password>]
    tpm2 pcr_extend <pcr> <digest_addr> [<digest_algo>]
    tpm2 pcr_read <pcr> <digest_addr> [<digest_algo>]
    tpm2 pcr_allocate <algorithm> <on|off> [<password>]
    tpm2 get_capability <capability> <property> <addr> <count>
    tpm2 dam_reset [<password>]
    tpm2 dam_parameters <max_tries> <recovery_time> <lockout_recovery> [<password>]
    tpm2 change_auth <hierarchy> <new_pw> [<old_pw>]
    tpm2 pcr_setauthpolicy <pcr> <key> [<password>]
    tpm2 pcr_setauthvalue <pcr> <key> [<password>]

Description
-----------

The tpm2 command talks to a TPMv2.x device: a chip which holds measurements of
the boot in its Platform Configuration Registers (PCRs), protects them with a
password per hierarchy and limits how often a wrong password may be tried.

The sub-commands act on the currently selected TPM, chosen with 'tpm2 device'
and remembered until it is changed. When nothing has been selected the first
TPM in the system is used.

Which sub-commands exist depends on the version of the selected device, not on
the name typed. This page describes a TPMv2.x device; a TPMv1.x one has a
different set, described by :doc:`tpm<tpm>`. The two commands share a handler,
so 'tpm2' on a board whose first TPM is a TPMv1.x device offers the TPMv1.x
sub-commands, and 'tpm' on a TPMv2.x device offers these ones. Only the help
text differs.

Most of the sub-commands are a thin wrapper around a single TPM command, so
the TPM specification is the reference for what each one means.

Every numeric argument, the memory addresses included, is read in the base its
prefix gives: 0x for hexadecimal, a leading 0 for octal, otherwise decimal.
This differs from most U-Boot commands, where an address is hexadecimal
whether or not it is prefixed, so an address here needs its 0x. The device
number of 'tpm2 device' is the exception, being read as decimal whatever it
looks like.

tpm2 device
~~~~~~~~~~~

Show the TPMs in the system, each by its number and description, or select the
one which later sub-commands act on.

tpm2 info
~~~~~~~~~

Show the description of the selected TPM.

tpm2 state
~~~~~~~~~~

Show whatever internal state the driver chooses to report, which is meant for
debugging the driver rather than for use by a script. A driver which reports
nothing leaves this empty.

tpm2 init
~~~~~~~~~

Open the TPM and leave it waiting for a startup, without issuing one. This is
the first sub-command to issue, and only 'tpm2 startup' may follow it.

tpm2 autostart
~~~~~~~~~~~~~~

Do the whole opening sequence: init, a Startup(clear) and a full self test.
This is the usual way to bring a TPM up, since the three are almost always
wanted together.

tpm2 startup
~~~~~~~~~~~~

Issue TPM2_Startup. The mode is TPM2_SU_CLEAR to start with fresh PCRs or
TPM2_SU_STATE to restore the state saved by the last shutdown. Adding 'off'
issues TPM2_Shutdown with that mode instead, which is how the TPM is told that
power is about to go away. The names are matched without regard to case.

tpm2 self_test
~~~~~~~~~~~~~~

Run the self tests. The type is 'full' to run them all, or 'continue' to run
only those which have not been run yet.

tpm2 clear
~~~~~~~~~~

Issue TPM2_Clear, which resets the hierarchy: its password, the PCRs and the
objects belonging to it. TPM2_RH_LOCKOUT and TPM2_RH_PLATFORM are the two
hierarchies this sub-command accepts, the platform one being the wider of the
two.

tpm2 pcr_extend
~~~~~~~~~~~~~~~

Add a measurement to a PCR. The digest is read from memory and is as long as
the algorithm needs: 32 bytes for sha256, which is the default. The TPM
replaces the register with the hash of its old contents followed by the
digest, so the order in which measurements arrive is part of the result. The
new contents are shown, along with the number of bytes and the algorithm.

tpm2 pcr_read
~~~~~~~~~~~~~

Read a PCR into memory, then show it as hex bytes. The count of 'known
updates' is how many measurements the TPM has taken into that register, which
tells a script whether anything has extended it since it last looked.

tpm2 pcr_allocate
~~~~~~~~~~~~~~~~~

Issue TPM2_PCR_Allocate to turn a bank of PCRs on or off. A bank is the set of
registers kept for one hash algorithm, so this chooses which algorithms the
TPM measures with. The change takes effect at the next startup. The banks the
device has are listed the first time the sub-command runs.

tpm2 get_capability
~~~~~~~~~~~~~~~~~~~

Read a run of capability properties into memory, then show each as its number
and value. Both are four bytes, written at the address given, so a count of
three needs 24 bytes of space.

tpm2 dam_reset
~~~~~~~~~~~~~~

Reset the counter of failed authorisation attempts, which is what dictionary
attack mitigation (DAM) counts. A TPM which has already locked itself out
refuses this.

tpm2 dam_parameters
~~~~~~~~~~~~~~~~~~~

Set the three dictionary attack mitigation parameters: how many failures are
allowed before the TPM locks out, how long it takes to forget one failure, and
how long a lockout lasts. The values are logged as they are sent.

tpm2 change_auth
~~~~~~~~~~~~~~~~

Set the password of a hierarchy, giving the old one if it has one. The
hierarchy is TPM2_RH_LOCKOUT, TPM2_RH_ENDORSEMENT, TPM2_RH_OWNER or
TPM2_RH_PLATFORM.

tpm2 pcr_setauthpolicy, tpm2 pcr_setauthvalue
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Change the policy or the value which guards access to a PCR. The key is
exactly 32 characters. Both are marked in the help text as untested, so expect
to have to read the code.

num
    number of the TPM to use, decimal, counting from 0 in the order the
    devices are bound

mode
    startup mode, either TPM2_SU_CLEAR or TPM2_SU_STATE

type
    self-test type, either full or continue

hierarchy
    name of a hierarchy, such as TPM2_RH_LOCKOUT

pcr
    PCR number, counting from 0

digest_addr, addr
    memory address of the digest, or of the buffer to read into

digest_algo, algorithm
    hash algorithm: sha1, sha256, sha384, sha512 or sm3_256. The default for a
    digest is sha256

capability, property
    capability group and the first property within it

count
    number of properties to read

max_tries
    number of failed authorisations which locks the TPM out, 0 to lock out
    always

recovery_time
    seconds before the failure counter drops by one, 0 to never lock out

lockout_recovery
    seconds a lockout lasts, 0 if only a reboot ends it

password, new_pw, old_pw
    password of the hierarchy, at most 32 characters

key
    secret guarding a PCR, exactly 32 characters

Example
-------

Sandbox emulates a TPMv2.x device as well as a TPMv1.x one, and the TPMv2.x
device is the first, so nothing has to be selected. Bringing it up produces no
output; the state the driver reports shows that it is open::

    => tpm2 device
    device 0: Sandbox TPM2.x
    device 1: sandbox TPM
    => tpm2 info
    Sandbox TPM2.x
    => tpm2 state
    init_done=0
    => tpm2 autostart
    => tpm2 state
    init_done=1

A PCR starts at zero and counts the measurements it has taken. Extending it
with 32 bytes of zeroes gives the hash of a zeroed register followed by a
zeroed digest::

    => tpm2 pcr_read 10 0x1000000
    PCR #10 sha256 32 byte content (0 known updates):
     00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
     00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
    => tpm2 pcr_extend 10 0x1000000
    PCR #10 extended with 32 byte sha256 digest
     00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
     00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
    => tpm2 pcr_read 10 0x1000100
    PCR #10 sha256 32 byte content (1 known updates):
     f5 a5 fd 42 d1 6a 20 30 27 98 ef 6e d3 09 97 9b
     43 00 3d 23 20 d9 f0 e8 ea 98 31 a9 27 59 fb 4b

The digest is shown before the register is extended, so the second read is
needed to see the result, and it must read into different memory to leave the
digest alone.

Capabilities are read a run at a time. Property 0x20e is the lockout counter,
and 0x20f to 0x211 are the three dictionary attack mitigation parameters,
which read back as they were set::

    => tpm2 get_capability 0x6 0x20e 0x1000200 1
    Capabilities read from TPM:
    Property 0x0000020e: 0x00000000
    => tpm2 dam_parameters 3 10 0
    do_tpm_dam_parameters() Changing dictionary attack parameters:
    do_tpm_dam_parameters() - maxTries: 3
    do_tpm_dam_parameters() - recoveryTime: 10
    do_tpm_dam_parameters() - lockoutRecovery: 0
    => tpm2 get_capability 0x6 0x20f 0x1000200 3
    Capabilities read from TPM:
    Property 0x0000020f: 0x00000003
    Property 0x00000210: 0x0000000a
    Property 0x00000211: 0x00000000

The parameters are logged rather than printed, so the function name in front
of each line comes from sandbox's log format and a board which does not show
it prints the message alone.

Setting a password means giving it to the sub-commands which need one
afterwards::

    => tpm2 change_auth TPM2_RH_LOCKOUT unicorn
    => tpm2 clear TPM2_RH_LOCKOUT
    Authentication failed: wrong password.
    Error: 162
    => tpm2 clear TPM2_RH_LOCKOUT unicorn

A device which is not there, a startup mode which is not recognised and a bank
which the device does not support are all reported::

    => tpm2 device 5
    Couldn't set TPM 5 (rc = 1)
    => tpm2 startup FOO
    Couldn't recognize mode string: FOO
    => tpm2 pcr_allocate sha1 on
    Active bank[0]: sha256
    sha1 is not supported by the tpm device

The last of these shows its usage message as well, since it reports the
problem and asks for the arguments to be looked at again.

A PCR the device does not have, and a password or a key of the wrong length,
are reported only by the return value, so watch $? where those are possible::

    => tpm2 pcr_read 99 0x1000000
    => echo $?
    1

Note that sandbox implements only the commands its tests need, so 'tpm2
dam_reset' and 'tpm2 pcr_allocate' report error 323 (TPM2_RC_COMMAND_CODE)
there however they are called.

Configuration
-------------

The tpm2 command is available if CONFIG_CMD_TPM=y and CONFIG_TPM_V2=y, which
also needs a driver for the TPM itself. A board with a TPMv1.x device as well
has the :doc:`tpm<tpm>` command too, and either name reaches either device.

Return value
------------

The return value $? is 0 (true) if the operation succeeds. It is 1 (false) if
the TPM reports an error, in which case the error code is shown first, and
also if the device given to 'tpm2 device' does not exist, if a password or a
key is the wrong length or if a PCR index is out of range.

A missing or unrecognised sub-command, and a wrong number of arguments, are
reported as a usage error, which is also 1 (false).

See also
--------

* :doc:`tpm<tpm>` for the same command driving a TPMv1.x device
* :doc:`tpmtest<tpmtest>` for a suite of tests which exercise a TPMv1.x device
* :doc:`md<md>` for showing the memory that a read fills in
