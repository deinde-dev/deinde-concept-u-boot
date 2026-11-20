.. SPDX-License-Identifier: GPL-2.0+

qconfig - Querying CONFIG options
=================================

It is not possible to see all the CONFIG options used by a board without
building its `.config` file. This tool allows this to be done efficiently for
all boards, or a subset, writing the results to a unified database file.

This database can be queried, to find boards which used a certain combination
of options, to aid in discovering Kconfig options which imply others.

The tool also permits syncing of defconfigs, which corrects the ordering and
drops options which are implied by others.

Finally, it allows scanning the source code to look for inconsistencies in the
use of Kconfig options.

Installation
------------

You may need to install 'python3-asteval' for the 'asteval' module.

The tool uses kconfiglib from the buildman package, which you can install with
``pip install buildman``. It can also use a buildman checkout, as described in
:ref:`python_tool_checkouts`.

How does it work?
-----------------

The tool must be run at the top of the source directory.

Building a database
~~~~~~~~~~~~~~~~~~~

When building a database (`-b`), this tool evaluates the Kconfig tree directly
using kconfiglib (a pure-Python Kconfig implementation). For each defconfig, it
loads the file with ``kconf.load_config()``, resolves all symbol dependencies,
and collects the resulting CONFIG values. This is the same approach used by
``buildman`` when scanning board parameters.

Multiple worker processes run in parallel (one per CPU core by default,
adjustable with ``-j``), each parsing the Kconfig tree once and then processing
its share of defconfigs. This completes in a few seconds for all ~1500 boards,
since no ``make`` subprocesses or cross-compiler toolchains are needed.
The tool stores the database in a file named qconfig.db at the top of the
source directory.

Defconfig files containing ``#include`` directives are preprocessed with the
C preprocessor before loading, matching the behaviour of the build system.

Resyncing defconfigs
~~~~~~~~~~~~~~~~~~~~

When resyncing defconfigs (`-s`), the tool also uses kconfiglib.  It loads
each defconfig with ``load_config()`` and writes a minimal config with
``write_min_config()`` (equivalent to ``make savedefconfig``).  Defconfigs
that use ``#include`` directives are handled by computing the delta between
the full expanded config and the base provided by the included files, so the
include structure is preserved.

The ``-r`` (git-ref) option also uses kconfiglib: it clones the reference
commit, creates a Kconfig instance from that tree, loads each defconfig
against it, then normalises the result against the current tree's Kconfig.
No toolchains are needed.

Toolchains
----------

Toolchains are **not** needed for any qconfig operation. The only
difference from using a real toolchain is that ``CONFIG_GCC_VERSION``
reflects the host compiler rather than each board's cross-compiler.
This does not affect database queries, imply analysis, or defconfig
sync, since ``CONFIG_GCC_VERSION`` is a build-time value that never
appears in defconfig files or influences Kconfig defaults.


Examples
--------

To sync only X86 defconfigs::

   ./tools/qconfig.py -s -d <(grep -l X86 configs/*)

or::

   grep -l X86 configs/* | ./tools/qconfig.py -s -d -

or::

   ./tools/qconfig.py -s -D $(grep -l X86 configs/*)

To process CONFIG_CMD_FPGAD only for a subset of configs based on path match::

   ./tools/qconfig.py -C CONFIG_CMD_FPGAD -D configs/{amd_versal2,am68_sk}*


Finding boards with particular CONFIG combinations
--------------------------------------------------

You can use `qconfig.py` to figure out which boards have a CONFIG enabled, or
which do not. To use it, first build a database::

    ./tools/qconfig.py -b

Then you can run queries using the `-f` flag followed by a list of CONFIG terms.
Each term is CONFIG name, with or without a tilde (~) prefix. The tool searches
for boards which match the CONFIG name, or do not match if tilde is used. For
example, to find boards which enabled CONFIG_PCIE1 but not CONFIG_PCIE2::

    ./tools/qconfig.py -f PCIE1 ~PCIE2
    10 matches
    MPC8548CDS_36BIT MPC8548CDS MPC8548CDS_legacy kmcent2
    ls1012afrwy_tfa_SECURE_BOOT ls1012afrwy_tfa ls1012aqds_tfa_SECURE_BOOT
    ls1012aqds_tfa ls1012ardb_tfa_SECURE_BOOT ls1012ardb_tfa

It is also possible to search for particular values. For example, this finds all
boards with an empty string for `CONFIG_DEFAULT_FDT_FILE`::

    ./tools/qconfig.py -f DEFAULT_FDT_FILE=\"\"
    1229 matches
    ...

This finds boards which have a value for SYS_MAXARGS other than 64::

    ./tools/qconfig.py -f ~SYS_MAXARGS=64
    296 matches
    ...

String values can be searched for, too, including regular expressions, but
taking care of needed quoting and escaping. Since the values are stored in the
database with their double quotes, the quotes must be part of the pattern
itself. Enclosing the pattern with single quotes makes the shell pass the double
quotes and backslashes through unchanged.

    ./tools/qconfig.py -f SPL_LDSCRIPT=arch/arm/cpu/armv8/u-boot-spl.lds
    0 matches

    ./tools/qconfig.py -f SPL_LDSCRIPT="arch/arm/cpu/armv8/u-boot-spl.lds"
    0 matches

    ./tools/qconfig.py -f SPL_LDSCRIPT='"arch/arm/cpu/armv8/u-boot-spl.lds"'
    326 matches
    ...

    ./tools/qconfig.py -f SPL_LDSCRIPT='"arch/arm/cpu/.*/u-boot-spl.lds"'
    462 matches
    ...

The pattern must match the whole value stored in the database, therefore::

    ./tools/qconfig.py -f SPL_LDSCRIPT='"arch/arm/cpu/.*/'
    0 matches

The values stored are the ones from the Kconfig files before expansion, thus::

    ./tools/qconfig.py -f SPL_LDSCRIPT='"arch/arm/cpu/u-boot-spl.lds"'
    0 matches

    ./tools/qconfig.py -f SPL_LDSCRIPT='"arch/\$\(ARCH\)/cpu/u-boot-spl.lds"' \
        SYS_ARCH='"arm"'
    158 matches
    ...


Finding implied CONFIGs
-----------------------

Some CONFIG options can be implied by others and this can help to reduce
the size of the defconfig files. For example, CONFIG_X86 implies
CONFIG_CMD_IRQ, so we can put 'imply CMD_IRQ' under 'config X86' and
all x86 boards will have that option, avoiding adding CONFIG_CMD_IRQ to
each of the x86 defconfig files.

This tool can help find such configs. To use it, first build a database::

    ./tools/qconfig.py -b

Then try to query it::

   ./tools/qconfig.py -i CONFIG_I8042_KEYB
   CONFIG_I8042_KEYB found in 33/5155 defconfigs
   28 : CONFIG_X86
   28 : CONFIG_SA_PCIEX_LENGTH
   28 : CONFIG_HPET_ADDRESS
   28 : CONFIG_MAX_PIRQ_LINKS
   28 : CONFIG_I8254_TIMER
   28 : CONFIG_I8259_PIC
   28 : CONFIG_RAMBASE
   28 : CONFIG_IRQ_SLOT_COUNT
   28 : CONFIG_PCIE_ECAM_SIZE
   28 : CONFIG_APIC
   ...

This shows a list of config options which might imply CONFIG_I8042_KEYB along
with how many defconfigs they cover. From this you can see that CONFIG_X86
generally implies CONFIG_I8042_KEYB but not always (28 out of 35). Therefore,
instead of adding CONFIG_I8042_KEYB to
the defconfig of every x86 board, you could add a single imply line to the
Kconfig file::

    config X86
        bool "x86 architecture"
        ...
        imply CMD_EEPROM

That will cover 28 defconfigs and you can perhaps find another condition that
indicates that CONFIG_I8042_KEYB is not needed for the remaining 5 boards. Many
of the options listed are not suitable as they are not related. E.g. it would be
odd for CONFIG_RAMBASE to imply CONFIG_I8042_KEYB.

Using this search you can reduce the size of qconfig patches.

You can automatically add 'imply' statements in the Kconfig with the -a
option::

    ./tools/qconfig.py -s -i CONFIG_SCSI \
            -a CONFIG_ARCH_LS1021A,CONFIG_ARCH_LS1043A

This will add 'imply SCSI' to the two CONFIG options mentioned, assuming that
the database indicates that they do actually imply CONFIG_SCSI and do not
already have an 'imply SCSI'.

The output shows where the imply is added::

   18 : CONFIG_ARCH_LS1021A       arch/arm/cpu/armv7/ls102xa/Kconfig:1
   13 : CONFIG_ARCH_LS1043A       arch/arm/cpu/armv8/fsl-layerscape/Kconfig:11
   12 : CONFIG_ARCH_LS1046A       arch/arm/cpu/armv8/fsl-layerscape/Kconfig:31

The first number is the number of boards which can avoid having a special
CONFIG_SCSI option in their defconfig file if this 'imply' is added.
The location at the right is the Kconfig file and line number where the config
appears. For example, adding 'imply CONFIG_SCSI' to the 'config ARCH_LS1021A'
in arch/arm/cpu/armv7/ls102xa/Kconfig at line 1 will help 18 boards to reduce
the size of their defconfig files.

If you want to add an 'imply' to every imply config in the list, you can use::

    ./tools/qconfig.py -s -i CONFIG_SCSI -a all

To control which ones are displayed, use -I <list> where list is a list of
options (use '-I help' to see possible options and their meaning).

To skip showing you options that already have an 'imply' attached, use -A.

When you have finished adding 'imply' options you can regenerate the
defconfig files for affected boards with something like::

    git show --stat | ./tools/qconfig.py -s -d -

This will regenerate only those defconfigs changed in the current commit.
If you start with (say) 100 defconfigs being changed in the commit, and add
a few 'imply' options as above, then regenerate, hopefully you can reduce the
number of defconfigs changed in the commit.


Checking for stale options
--------------------------

Kconfig ignores an option which it does not know about. So when an option is
renamed or removed, a defconfig file or config fragment which still sets the
old one gives no error: the board silently gets the default value instead. This
is easy to miss when a new board arrives after a rename.

To check every defconfig file in `configs/` and every config fragment
(`*.config`) in `configs/` and `board/` against the Kconfig files::

    ./tools/qconfig.py --check-stale

This lists each stale option with its file and line number::

    configs/snow_defconfig:112: CONFIG_NOT_REAL is not in Kconfig
    1 stale options in 1 of 1631 config files: Kconfig ignores them, so use the new name or drop them

It returns a non-zero exit code if any are found, so it can be used in CI. No
toolchain or database is needed and it takes about a second.

Available options
-----------------

 --check-stale
   Check the defconfig files and config fragments for options which Kconfig
   does not define.

 --nocolour
   Disables colouring of output. This is normally used when writing to a
   terminal.

 -C, --commit
   Create a git commit with the changes when the operation is complete. A
   standard commit message is used which may need to be edited.

 -d, --defconfigs
  Specify a file containing a list of defconfigs to move.  The defconfig
  files can be given with shell-style wildcards. Use '-' to read from stdin.

 -f, --find
   Find boards with a given config combination

 -n, --dry-run
   Perform a trial run that does not make any changes.  It is useful to
   see what is going to happen before one actually runs it.

 -e, --exit-on-error
   Exit immediately if Make exits with a non-zero status while processing
   a defconfig file.

 -s, --force-sync
   Do "make savedefconfig" forcibly for all the defconfig files.
   If not specified, "make savedefconfig" only occurs for cases
   where at least one CONFIG was moved.

 -S, --spl
   Look for moved config options in spl/include/autoconf.mk instead of
   include/autoconf.mk.  This is useful for moving options for SPL build
   because SPL related options (mostly prefixed with CONFIG_SPL\_) are
   sometimes blocked by CONFIG_XPL_BUILD ifdef conditionals.

 -j, --jobs
   Specify the number of threads to run simultaneously.  If not specified,
   the number of threads is the same as the number of CPU cores.

 -r, --git-ref
   Specify the git ref to clone for evaluating the Kconfig tree. If
   unspecified, use the CWD. This is useful when changes to Kconfig affect
   default values and you want to capture the state of defconfigs from
   before that change was in effect. If in doubt, specify a ref pre-Kconfig
   changes (use HEAD if Kconfig changes are not committed). Worst case it
   will take a bit longer to run, but will always do the right thing.

 -v, --verbose
   Show any build errors as boards are built

To see the complete list of supported options, run::

    ./tools/qconfig.py -h
