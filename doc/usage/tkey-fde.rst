.. SPDX-License-Identifier: GPL-2.0+

TKey Full Disk Encryption
==========================

Overview
--------

U-Boot supports using `Tillitis TKey <https://tillitis.se/>`_ hardware-security
tokens to unlock LUKS-encrypted partitions. This provides hardware-backed
full-disk encryption (FDE) where the encryption key is derived from two pieces
of information:

* A user password/passphrase (USS - User Supplied Secret)
* The TKey's internal Unique Device Identifier (UDI)

The same password on the same TKey always produces the same encryption key,
making it suitable for unlocking encrypted root filesystems at boot time.

Note: Despite its name, FDE generally refers to the encryption of a single
partition on a disk, rather than an entire disk.

**Key Features:**

* Hardware-backed key derivation using TKey security token
* Compatible with standard LUKS1 and LUKS2 encrypted partitions
* Automatic unlock during boot flow detection
* Test infrastructure for creating encrypted disk images
* Python tools for key generation and disk encryption

How It Works
------------

TKey Key Derivation Process
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The TKey derives encryption keys through this process:

1. **Load Signer App**: The TKey firmware loads the signer application with
   your password (USS)
2. **Generate Key Pair**: The signer app combines the USS with the device's UDI
   to generate an Ed25519 key pair
3. **Derive Disk Key**: The public key is hashed with SHA-256 to produce a
   32-byte encryption key
4. **Decrypt Partition**: The key is used to unlock the LUKS-encrypted
   partition

This means:

* The same password always produces the same key (deterministic)
* Different passwords produce completely different keys
* The physical TKey device is required (UDI is device-specific)
* No key material is stored on disk - only derived when needed


U-Boot Integration
~~~~~~~~~~~~~~~~~~

When U-Boot detects a LUKS-encrypted partition during bootflow booting:

1. Prompts the user for their password
2. Detects if a TKey device is present
3. Loads the TKey signer app with the password
4. Derives the encryption key from the TKey
5. Attempts to unlock the LUKS partition
6. Creates a blkmap device for accessing decrypted data
7. Continues with normal boot process


Tools and Workflow
------------------

The ``scripts/tkey_fde_key.py`` Script
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

This Python script handles TKey key derivation and disk encryption operations.

**Basic Usage - Generate a key:**

::

    # Generate key interactively (prompts for password)
    $ ./scripts/tkey_fde_key.py

    # Generate key from password file
    $ echo "mypassword" > passfile
    $ ./scripts/tkey_fde_key.py -p passfile -o diskkey.bin --binary

    # Generate key from stdin
    $ echo "mypassword" | ./scripts/tkey_fde_key.py -p - --binary

**Encrypting a Disk Image:**

::

    # Create a disk image
    $ dd if=/dev/zero of=rootfs.img bs=1M count=1000

    # Encrypt the entire disk with LUKS
    $ ./scripts/tkey_fde_key.py -e rootfs.img -p passfile

    # Encrypt a specific partition
    $ ./scripts/tkey_fde_key.py -e disk.img -P 2 -p passfile

**Opening an Encrypted Disk:**

::

    # Open encrypted disk (creates /dev/mapper/tkey-disk)
    $ ./scripts/tkey_fde_key.py -O rootfs.img -p passfile

    # Mount the decrypted filesystem
    $ sudo mount /dev/mapper/tkey-disk /mnt

    # When done, unmount and close
    $ sudo umount /mnt
    $ sudo cryptsetup close tkey-disk

**Advanced Options:**

::

    # Save both encrypted disk and backup key file
    $ ./scripts/tkey_fde_key.py -e disk.img -p passfile -o backup.key

    # Use verbose output to see what's happening
    $ ./scripts/tkey_fde_key.py -e disk.img -p passfile --verbose

    # Use debug mode for troubleshooting
    $ ./scripts/tkey_fde_key.py --debug -e disk.img -p passfile


Creating Test Images
--------------------

Test Disk Images
~~~~~~~~~~~~~~~~

The U-Boot test infrastructure creates several LUKS-encrypted test images:

* ``mmc11.img`` - LUKS1 encrypted Ubuntu image
* ``mmc12.img`` - LUKS2 encrypted Ubuntu image with Argon2id KDF
* ``mmc13.img`` - LUKS2 encrypted Ubuntu image for TKey testing
* ``mmc14.img`` - LUKS2 encrypted image with pre-derived master key

By default, ``mmc13.img`` is encrypted with a key derived from the TKey
emulator's deterministic public key. This allows testing without physical
hardware.

Using override.bin for Physical TKey Testing
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

To test with a physical TKey device instead of the emulator, create an
``override.bin`` file containing the TKey-derived disk key:

::

    # Generate override.bin from your physical TKey with password "test"
    $ echo "test" | ./scripts/tkey_fde_key.py -p - -o override.bin --binary

    # Regenerate test images with your TKey's key
    $ ./test/py/test.py -B sandbox --build-dir /tmp/b/sandbox \
        --build -k test_ut_dm_init_bootstd

    # Test unlocking mmc13 with your physical TKey
    $ /tmp/b/sandbox/u-boot -T -c \
        "tkey connect sandbox_tkey; sb devon mmc13; luks unlock -t mmc d:2 test"

When ``override.bin`` exists in the source directory, the test infrastructure
uses it instead of the emulator's key to encrypt ``mmc13.img``. This allows
you to test the full TKey unlock flow with real hardware.

To switch back to emulator testing, simply remove the override file:

::

    $ rm override.bin
    $ ./test/py/test.py -B sandbox --build-dir /tmp/b/sandbox \
        --build -k test_ut_dm_init_bootstd

Using the Python Test Infrastructure
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The test infrastructure in ``test/py/tests/test_ut.py`` handles TKey key
generation automatically:

::

    def test_ut_dm_init_bootstd(u_boot_config, u_boot_log):
        """Initialize data for bootflow tests with TKey encryption"""

        # Check for override key file (for physical TKey testing)
        override_keyfile = os.path.join(u_boot_config.source_dir, 'override.bin')
        if os.path.exists(override_keyfile):
            keyfile = override_keyfile
            u_boot_log.action(f'Using override TKey key: {keyfile}')
        else:
            # Generate key matching TKey emulator's deterministic pubkey
            pubkey = bytes([0x50 + (i & 0xf) for i in range(32)])
            disk_key = hashlib.sha256(pubkey.hex().encode()).digest()
            keyfile = os.path.join(u_boot_config.persistent_data_dir,
                                   'tkey_emul.key')
            with open(keyfile, 'wb') as f:
                f.write(disk_key)

        # Create LUKS2 encrypted image for TKey testing
        setup_ubuntu_image(u_boot_config, u_boot_log, 13, 'mmc',
                           use_fde=2, luks_kdf='argon2id',
                           encrypt_keyfile=keyfile)

**Helper Class Usage:**

See ``test/py/tests/fs_helper.py`` for the ``FsHelper`` class:

::

    from fs_helper import FsHelper, DiskHelper

    # Create LUKS2 encrypted filesystem with TKey key file
    with FsHelper(config, 'ext4', 30, 'test',
                  part_mb=60,
                  encrypt_keyfile='/path/to/tkey-derived-key.bin') as fsh:
        fsh.setup()
        # Add files to fsh.srcdir
        with open(os.path.join(fsh.srcdir, 'hello.txt'), 'w') as f:
            f.write('Hello from TKey FDE!\n')

        # Create encrypted filesystem
        fsh.mk_fs()

Step-by-Step Workflow
----------------------

Complete Example: Testing TKey FDE with mmc13
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

**1. Create Test Disk Images**

Run the test infrastructure to create encrypted images:

::

    $ ./test/py/test.py -B sandbox --build-dir /tmp/b/sandbox \
        --build -k test_ut_dm_init_bootstd

This creates ``mmc13.img`` (LUKS2 with Argon2id) encrypted with the TKey
emulator's key.

**2. Test Unlocking with TKey Emulator**

Run U-Boot sandbox and test the unlock process with the emulator:

::

    $ /tmp/b/sandbox/u-boot -T -c \
        "tkey connect tkey-emul; sb devon mmc13; luks unlock -t mmc d:2 test"

**Expected Output:**

::

    Connected to TKey device
    Device 'mmc13' enabled
    Unlocking LUKS2 partition...
    Using TKey for disk encryption key
    Loading TKey signer app (28024 bytes) with USS...
    TKey public key:   50 51 52 53 54 55 56 57 58 59 5a 5b 5c 5d 5e 5f
      50 51 52 53 54 55 56 57 58 59 5a 5b 5c 5d 5e 5f
    TKey disk key derived successfully
    TKey derived disk key:   e9 b0 59 92 68 ff 8b 08 3e f8 0d bd 04 be 20 7c
      e9 a1 9a 60 a8 88 cc b3 fe 93 71 0a 0a 70 a3 4e
    Unlocked LUKS partition as blkmap device 'luks-mmc-d:2'

**3. Test with Physical TKey**

To test with a real TKey device:

::

    # Generate override.bin from your physical TKey with password "test"
    $ echo "test" | ./scripts/tkey_fde_key.py -p - -o override.bin --binary -f -v
    Reading password from stdin...
    Password length: 4, repr: 'test'
    TKey detected via USB enumeration
    ...
    Ed25519 public key (hex): df4faa680d9fd79079cc572c1f84fb3fa59ab904dad652e90a22e5b672a67eb1
    Derived disk key (hex): 1546bdaf99e9ed9867d83ae69062c9da3202a617584a35ee4ae38672ec775a7f
    Key saved to override.bin

    # Regenerate mmc13.img with your TKey's key
    $ ./test/py/test.py -B sandbox --build-dir /tmp/b/sandbox \
        --build -k test_ut_dm_init_bootstd

    # Test unlocking with physical TKey (ensure TKey is plugged in)
    $ /tmp/b/sandbox/u-boot -T -c "sb devon mmc13; luks unlock -t mmc d:2 test" -P

**Expected Output with Physical TKey:**

::

    Device 'mmc13' enabled
    Unlocking LUKS2 partition...
    Using TKey for disk encryption key
    Loading TKey signer app (28024 bytes) with USS...
    TKey public key:   df 4f aa 68 0d 9f d7 90 79 cc 57 2c 1f 84 fb 3f
      a5 9a b9 04 da d6 52 e9 0a 22 e5 b6 72 a6 7e b1
    TKey disk key derived successfully
    TKey derived disk key:   15 46 bd af 99 e9 ed 98 67 d8 3a e6 90 62 c9 da
      32 02 a6 17 58 4a 35 ee 4a e3 86 72 ec 77 5a 7f
    Unlocked LUKS partition as blkmap device 'luks-mmc-d:2'

**4. Verify Encryption**

You can verify the disk is encrypted by checking with cryptsetup:

::

    # Extract partition 2 from the disk image
    $ dd if=mmc13.img bs=512 skip=38912 count=122880 of=mmc13_part2.img

    # Check LUKS header
    $ cryptsetup luksDump mmc13_part2.img
    LUKS header information
    Version:        2
    ...

    # Test unlock with the TKey emulator key (when no override.bin exists)
    # First generate the emulator's key
    $ python3 -c "
    import hashlib
    pubkey = bytes([0x50 + (i & 0xf) for i in range(32)])
    key = hashlib.sha256(pubkey.hex().encode()).digest()
    open('emul.key', 'wb').write(key)
    "
    $ sudo cryptsetup open mmc13_part2.img test-luks --key-file=emul.key
    $ ls /dev/mapper/test-luks
    /dev/mapper/test-luks
    $ sudo cryptsetup close test-luks

Troubleshooting
---------------

Key Mismatch Errors
~~~~~~~~~~~~~~~~~~~

**Problem:** U-Boot shows "Failed to unlock LUKS partition"

::

    LUKS1: Keyslot 0 failed with error -13
    Failed to unlock LUKS1 with binary passphrase (err=-2)
    Failed to unlock LUKS partition (err=-13: Permission denied)

**Cause:** The disk was encrypted with a different key than U-Boot is deriving.

**Solutions:**

1. **Verify you're using the correct password:** Make sure you enter the same
   password in U-Boot that was used to generate the key file.

2. **Regenerate the key and verify it matches:**

   ::

       # Generate key again with same password
       $ ./scripts/tkey_fde_key.py -p mykey.txt -o test-key.bin --binary

       # Compare with original
       $ diff mykey test-key.bin

       # If different, TKey may have been in different state

3. **Recreate disk images with the correct key:** If U-Boot consistently
   derives a different key than what was used for encryption, capture the key
   U-Boot derives and use that to encrypt the disk:

   ::

       # Run U-Boot and note the "Binary pass" hex values from debug output
       # Then recreate the key file with those values
       $ python3 << 'EOF'
       key_hex = "10f1132e6c27e6e8...d29b6b8a"  # from U-Boot output
       with open('mykey', 'wb') as f:
           f.write(bytes.fromhex(key_hex))
       EOF

       # Recreate disk images with this key
       $ rm mmc11.img mmc12.img
       $ ./test/py/test.py -B sandbox --build-dir /tmp/b/sandbox \
           -k test_ut_dm_init_bootstd

TKey Not Detected
~~~~~~~~~~~~~~~~~

**Problem:** U-Boot doesn't detect the TKey device

**Solutions:**

* Ensure TKey is plugged in before starting U-Boot
* Check USB device is accessible: ``ls /dev/ttyACM*``
* Try replugging the TKey device
* For sandbox, ensure ``sandbox,device-path`` is set correctly in device tree

TKey App Already Loaded
~~~~~~~~~~~~~~~~~~~~~~~~

**Problem:** TKey is in app mode instead of firmware mode

**Solution:** Remove and reinsert the TKey. The device must be in firmware mode
to load the signer app.

Configuration
-------------

Kconfig Options
~~~~~~~~~~~~~~~

To enable TKey FDE support in U-Boot:

::

    CONFIG_CMD_TKEY=y          # TKey command
    CONFIG_TKEY_DRIVER=y       # TKey device driver
    CONFIG_CMD_LUKS=y          # LUKS command
    CONFIG_BLK_LUKS=y          # LUKS block device support
    CONFIG_BLKMAP=y            # Block device mapping
    CONFIG_BOOTCTL=y           # Boot control with unlock support
    CONFIG_BOOTCTL_LOGIC=y     # Boot control unlock logic
    CONFIG_ARGON2=y            # For LUKS2 Argon2id support (optional)

Device Tree Configuration
~~~~~~~~~~~~~~~~~~~~~~~~~

For sandbox testing, the TKey is configured in ``arch/sandbox/dts/test.dts``:

::

    tkey {
        compatible = "sandbox,tkey";
        sandbox,device-path = "/dev/ttyACM0";
    };

For real hardware, configure the USB serial device path appropriately.

Security Considerations
-----------------------

Key Storage
~~~~~~~~~~~

* The TKey-derived key should **never be stored permanently** on disk
* Only temporary key files (like ``mykey``) used during testing should exist
* In production, keys should be derived fresh each boot from TKey + password
* The ``mykey`` file is for **testing only** and should be kept secure

Password Security
~~~~~~~~~~~~~~~~~

* Use a strong password (at least 16 characters recommended)
* Different passwords produce completely different encryption keys
* The TKey's UDI adds additional entropy to the key derivation
* Consider using a hardware security token for additional protection

Hardware Security
~~~~~~~~~~~~~~~~~

* Physical access to the TKey is required to derive keys
* The TKey's UDI is unique per device - keys cannot be derived without it
* If the TKey is lost, encrypted data cannot be recovered
* Consider keeping a backup TKey or traditional key recovery mechanism

Memory Security
~~~~~~~~~~~~~~~

* Keys are held in memory while the device is unlocked
* Memory is not securely erased on warm reboot
* This is acceptable for boot-time use but not for long-term key storage


Comparison with Traditional LUKS
---------------------------------

**Traditional LUKS (Password Only):**

* Encryption key derived only from password
* Vulnerable to offline password cracking attacks
* No hardware requirement - same password works anywhere

**TKey-Enhanced LUKS:**

* Encryption key derived from password + TKey UDI
* Requires physical TKey device to derive key
* Resistant to offline password cracking (attacker needs both password and
  TKey)
* Same password + same TKey always produces same key
* Different TKey devices produce different keys even with same password

Example Use Case: Secure Boot
------------------------------

A typical secure boot workflow with TKey FDE:

1. **System Powers On**

   * U-Boot starts and scans for boot devices
   * Finds LUKS-encrypted root partition

2. **User Authentication**

   * U-Boot prompts user for password
   * User inserts TKey device
   * User enters password

3. **Key Derivation**

   * U-Boot loads TKey signer app with password
   * TKey derives encryption key from password + UDI
   * Returns 32-byte encryption key to U-Boot

4. **Partition Unlock**

   * U-Boot attempts to unlock LUKS partition
   * If successful, creates blkmap device
   * Encrypted data is accessible as standard block device

5. **Boot Continues**

   * U-Boot loads kernel from unlocked partition
   * System boots into encrypted root filesystem

Testing
-------

Unit Tests
~~~~~~~~~~

Run the bootctl TKey unlock tests:

::

    $ ./test/py/test.py -B sandbox --build-dir /tmp/b/sandbox \
        -k bootctl_logic_tkey


Manual Testing
~~~~~~~~~~~~~~

Test mmc13 with the TKey emulator:

::

    # 1. Build sandbox and create test images
    $ crosfw sandbox -L
    $ ./test/py/test.py -B sandbox --build-dir /tmp/b/sandbox \
        --build -k test_ut_dm_init_bootstd

    # 2. Test unlocking mmc13 with TKey emulator
    $ /tmp/b/sandbox/u-boot -T -c \
        "tkey connect tkey-emul; sb devon mmc13; luks unlock -t mmc d:2 test"

Test mmc13 with a physical TKey:

::

    # 1. Generate override.bin with your TKey and password "test"
    $ echo "test" | ./scripts/tkey_fde_key.py -p - -o override.bin --binary

    # 2. Regenerate test images
    $ ./test/py/test.py -B sandbox --build-dir /tmp/b/sandbox \
        --build -k test_ut_dm_init_bootstd

    # 3. Test unlocking with physical TKey
    $ /tmp/b/sandbox/u-boot -T -c \
        "tkey connect sandbox_tkey; sb devon mmc13; luks unlock -t mmc d:2 test"

See Also
--------

* :doc:`luks` - LUKS encryption documentation
* :doc:`cmd/luks` - LUKS command reference
* :doc:`cmd/tkey` - TKey command reference
* :doc:`cmd/blkmap` - Blkmap device mapping
* ``scripts/tkey_fde_key.py`` - TKey key derivation tool
* ``test/py/tests/fs_helper.py`` - Filesystem test helpers
* `Tillitis TKey Documentation <https://tillitis.se/>`_
