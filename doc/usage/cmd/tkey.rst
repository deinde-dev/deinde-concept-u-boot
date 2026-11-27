.. SPDX-License-Identifier: GPL-2.0+

.. index::
   single: tkey (command)

tkey command
============

Synopsis
--------

::

    tkey connect [device-name]
    tkey fwmode
    tkey getkey <uss> [verify-hash]
    tkey info
    tkey loadapp [uss]
    tkey signer
    tkey wrapkey <password>

Description
-----------

The *tkey* command provides an interface to interact with Tillitis TKey
security tokens. The TKey is a USB security device that can be used for
cryptographic operations, particularly for deriving encryption keys in a
secure and reproducible manner.

The TKey operates in two modes:

Firmware mode
    The device starts in this mode after being plugged in. In firmware mode,
    the device provides access to its Unique Device Identifier (UDI) and allows
    loading applications.

App mode
    After an application is loaded, the device enters app mode. The UDI is no
    longer accessible, but the loaded app can perform cryptographic operations.

The primary use case is full disk encryption (FDE) key derivation, where the
TKey combines a User-Supplied Secret (USS, typically a password) with its
internal UDI to generate deterministic encryption keys.


tkey connect
~~~~~~~~~~~~

Test connectivity to a TKey device and optionally select a specific device for
subsequent commands.

When called without arguments, this command connects to the first available TKey
device in the system. When a device name is provided, it connects to that
specific device.

Once connected, the selected device is remembered and will be used by all
subsequent tkey commands (info, getkey, loadapp, etc.) until a different device
is selected with another connect command. A connect which fails leaves the
device chosen before it in place. Without a connect, each sub-command uses the
first available device, without remembering it.

device-name
    Optional name of a specific TKey device to connect to


tkey fwmode
~~~~~~~~~~~

Check whether the TKey device is currently in firmware mode or app mode.

Firmware mode
    The device has just been plugged in or reset. The UDI is accessible and
    apps can be loaded.

App mode
    An application has been loaded and is running. The UDI is not accessible.


tkey getkey
~~~~~~~~~~~

Derive a disk encryption key by loading the embedded signer app with a
User-Supplied Secret (USS). This is the main command for full-disk-encryption
workflows.

The command performs these steps:

#. Loads the embedded signer app with the provided USS
#. Retrieves the public key from the signer app
#. Derives the disk encryption key (SHA-256 of the public key written out
   as hex characters)
#. Computes a verification hash (BLAKE2b-256 of the disk key)

The USS is typically a user password or passphrase. The same USS always produces
the same disk encryption key, making this suitable for unlocking encrypted
disks.

If a verification hash is provided as the second argument, the command verifies
that the USS is correct by comparing the computed hash with the expected hash.
This is useful for validating user passwords before attempting to decrypt a
disk.

uss
    User-Supplied Secret (password/passphrase) for key derivation, at most
    32 bytes long

verify-hash
    Optional hash to verify the USS is correct, as exactly 64 hex characters

The command outputs:

Public Key
    32-byte Ed25519 public key derived from USS

Disk Key
    32-byte encryption key (SHA-256 of the public key's hex form)

Verification Hash
    32-byte hash of disk key (save this for later verification)

The verification hash is shown only when none is given on the command line.
With one, the command reports whether it matches instead, and shows both
hashes where it does not.


tkey info
~~~~~~~~~

Display information about the TKey device, including its name, version, and
Unique Device Identifier (UDI).

The name and version are available in both modes, so the first line always
appears. The UDI is only available in firmware mode: in app mode the command
reports the error the device gives for the request and returns failure. A
device which rejects the request as unsupported, which is what the firmware
does, brings the message 'UDI not available - replug device'.


tkey loadapp
~~~~~~~~~~~~

Load the embedded signer application to the TKey device. This can only be done
when the device is in firmware mode.

An optional USS (User-Supplied Secret) can be provided, which will be used by
the signer app for key derivation. If no USS is provided, the app loads without
a secret.

After loading, the device transitions to app mode and the UDI becomes
inaccessible.

uss
    Optional User-Supplied Secret for the signer app, at most 32 bytes long


tkey signer
~~~~~~~~~~~

Display information about the embedded signer application binary that is
compiled into U-Boot.


tkey wrapkey
~~~~~~~~~~~~

Derive a wrapping key from a password and the device's UDI. This uses the
BLAKE2b-256 hash function to combine the UDI with the password.

The wrapping key can be used to encrypt/decrypt other secrets. Unlike getkey,
this command does not load an app - it only requires the UDI, so it must be
run in firmware mode.

The same password always produces the same wrapping key for a given device,
but different TKey devices (with different UDIs) will produce different
wrapping keys even with the same password.

password
    Password to combine with UDI for key derivation


Example
-------

This shows a session on sandbox, which has an emulated TKey called
'tkey-emul'. Connect to it, look at it and derive both kinds of key::

    => tkey connect tkey-emul
    Connected to TKey device
    => tkey info
    Name0: tk1  Name1: mkdf Version: 4
    UDI: a0a1a2a3a4a5a6a7
    => tkey fwmode
    firmware mode
    => tkey signer
    signer binary: 28024 bytes at 00005c5382a05160-00005c5382a0bed8
    => tkey wrapkey hunter2
    Wrapping Key: ae70b66ec52f60327bf4a60d51686f8314407102cbab35f40e57353091554d8c
    => tkey getkey hunter2
    Public Key: 505152535455565758595a5b5c5d5e5f505152535455565758595a5b5c5d5e5f
    Disk Key: e9b0599268ff8b083ef80dbd04be207ce9a19a60a888ccb3fe93710a0a70a34e
    Verification Hash: 8583a08d6c534e84ae81a8518071c16a8030893df05fecb84e514438591ba5ed

Deriving the disk key loads the signer app, so the device is in app mode
afterwards::

    => tkey fwmode
    app mode

Checking a secret against the verification hash saved from that run::

    => tkey getkey hunter2 8583a08d6c534e84ae81a8518071c16a8030893df05fecb84e514438591ba5ed
    Public Key: 505152535455565758595a5b5c5d5e5f505152535455565758595a5b5c5d5e5f
    Disk Key: e9b0599268ff8b083ef80dbd04be207ce9a19a60a888ccb3fe93710a0a70a34e

    password correct

Where the hash does not match, the command shows both and returns failure::

    => tkey getkey hunter2 0000000000000000000000000000000000000000000000000000000000000000
    Public Key: 505152535455565758595a5b5c5d5e5f505152535455565758595a5b5c5d5e5f
    Disk Key: e9b0599268ff8b083ef80dbd04be207ce9a19a60a888ccb3fe93710a0a70a34e

    wrong password
    Expected: 0000000000000000000000000000000000000000000000000000000000000000
    Got: 8583a08d6c534e84ae81a8518071c16a8030893df05fecb84e514438591ba5ed

Loading the signer app with a secret, on a device in firmware mode. The UDI
is out of reach once it is loaded, so info then fails::

    => tkey connect tkey-emul
    Connected to TKey device
    => tkey loadapp hunter2
    Loading signer app (28024 bytes) with USS...done
    => tkey fwmode
    app mode
    => tkey info
    Name0: tk1  Name1: sign Version: 4
    Failed to get UDI (err -5: I/O error)


Return value
------------

The return value $? is 0 (true) if the sub-command succeeds. It is 1
(false) if the device cannot be found, if the device reports an error, or
if the secret given to getkey or loadapp is longer than 32 bytes. The
usage message is shown, and $? is 1, where the sub-command does not exist
or its arguments are wrong, which includes a verification hash which is
not 64 hex characters long.


Configuration
-------------

The tkey command is available if CONFIG_CMD_TKEY is enabled, which needs
CONFIG_TKEY for the uclass the command talks to. The drivers in the tree
are sandbox's: 'sandbox,tkey' reaches a real TKey through a host serial
device such as /dev/ttyACM0 and 'tkey,emul' emulates one, which is what
the tests use.


See also
--------

* :doc:`../tkey-fde` for the full-disk-encryption guide which puts these
  sub-commands together, from deriving a key to unlocking a volume with it
* :doc:`luks<luks>` for the command which uses such a key to open an
  encrypted volume
* :doc:`bootctl<bootctl>` for boot control, which can unlock a volume with
  a TKey on the way to booting an Operating System
* `Tillitis TKey documentation <https://tillitis.se/>`_ for the device
  itself and the signer app this command loads
