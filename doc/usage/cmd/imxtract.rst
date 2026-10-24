.. SPDX-License-Identifier: GPL-2.0+:

.. index::
   single: imxtract (command)

imxtract command
================

Synopsis
--------

::

    imxtract addr part [dest]
    imxtract addr uname [dest]

Description
-----------

The imxtract command is used to extract a part of a multi-image file.

Two different file formats are supported:

* FIT images
* legacy U-Boot images

addr
    Address of the multi-image file from which a part shall be extracted.
    Without it the value of the *loadaddr* environment variable is used,
    which starts out as CONFIG_SYS_LOAD_ADDR

part
    Index (hexadecimal) of the part of a legacy U-Boot image to be extracted,
    the first part being 0

uname
    Name of the part of a FIT image to be extracted

dest
    Destination address to copy the part to. Without it the part is only
    located and nothing is copied

The address and the length of the part are reported in the environment
variables *fileaddr* and *filesize*, whether or not a destination is given.
For a compressed part *fileaddr* is where the compressed data sits and
*filesize* is the length it uncompresses to.

A part which is compressed can only be extracted to a destination, since it
has to be uncompressed on the way. Asking for one without a destination is an
error. The command can uncompress gzip, bzip2 and Zstandard parts, where
support for the algorithm is built in; *CONFIG_SYS_XIMG_LEN* limits how large
a gzip part may uncompress to.

A legacy image must hold either multiple components or a script. The *iminfo*
command lists the parts of an image, so it shows which indices this command
accepts.

The value of environment variable *verify* controls if the hashes and
signatures of FIT images or the check sums of legacy U-Boot images are checked.
To enable checking set *verify* to one of the values *1*, *yes*, *true*.
(Actually only the first letter is checked disregarding the case.) A *verify*
which is not set at all enables checking too.

Examples
--------

This extracts the second part of a legacy image holding two of them. The image
is at 0x1000000 and the part is copied to 0x2000000:

.. code-block:: console

    => iminfo 1000000

    ## Checking Image at 01000000 ...
       Legacy image found
       Image Name:   two-part image
       Created:      1970-01-01   0:00:00 UTC
       Image Type:   Sandbox Linux Multi-File Image (uncompressed)
       Data Size:    24 Bytes = 24 Bytes
       Load Address: 00000000
       Entry Point:  00000000
       Contents:
          Image 0: 6 Bytes = 6 Bytes
          Image 1: 4 Bytes = 4 Bytes
       Verifying Checksum ... OK
    => imxtract 1000000 1 2000000
    ## Copying part 1 from legacy image at 01000000 ...
       Verifying Checksum ... OK
       Loading part 1 ... OK
    => md.b 2000000 4
    02000000: 80 81 82 83                                      ....

Without a destination the part is located but not copied, which is a way of
finding where a part sits:

.. code-block:: console

    => imxtract 1000000 0
    ## Copying part 0 from legacy image at 01000000 ...
       Verifying Checksum ... OK
    => printenv fileaddr filesize
    fileaddr=100004c
    filesize=6

With verify=no incorrect hashes, signatures, or check sums don't stop the
extraction. But correct hashes are still indicated in the output
(here: sha256, sha512).

.. code-block:: console

    => env set verify no
    => imxtract $loadaddr kernel-1 $kernel_addr_r
    ## Copying 'kernel-1' subimage from FIT image at 40200000 ...
    sha256+ sha512+    Loading part 0 ... OK
    =>

With verify=yes incorrect hashes, signatures, or check sums stop the extraction.

.. code-block:: console

    => env set verify yes
    => imxtract $loadaddr kernel-1 $kernel_addr_r
    ## Copying 'kernel-1' subimage from FIT image at 40200000 ...
    sha256 error!
    Bad hash value for 'hash-1' hash node in 'kernel-1' image node
    Bad Data Hash
    =>

Configuration
-------------

The imxtract command is only available if CONFIG_CMD_XIMG=y. Support for FIT
images requires CONFIG_FIT=y. Support for legacy U-Boot images requires
CONFIG_LEGACY_IMAGE_FORMAT=y.

Return value
------------

The return value $? is 0 (true) if the part is extracted. It is 1 (false) if
there is no image at the address, if the image does not hold the part asked
for, if verification fails or if the part cannot be uncompressed.

See also
--------

* :doc:`iminfo<iminfo>` for listing what an image holds before extracting
  from it
* :doc:`bootm<bootm>` for booting a multi-file image rather than taking it
  apart
* :doc:`load<load>` for reading an image into memory in the first place
* :doc:`source<source>` for running a script image, which this command also
  accepts
