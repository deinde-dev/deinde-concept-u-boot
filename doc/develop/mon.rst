.. SPDX-License-Identifier: GPL-2.0+

MON: the secure monitor
=======================

Introduction
------------

On ARMv8 some firmware has to stay in memory at EL3, the most privileged
exception level, after the OS has started. This is the secure monitor. It
handles the calls (SMCs) which the OS and U-Boot proper make to start and stop
CPUs, reset the machine and so on, using the PSCI interface.

Most ARMv8 boards use BL31 from TF-A (Trusted Firmware-A) for this. BL31 is
built separately and passed to U-Boot's build, and the image does not boot
without it.

MON is a secure monitor which is part of U-Boot. It is one of the xPL phases
(see :doc:`spl`), but it is not a loader. It is a *resident* phase: SPL loads it
alongside U-Boot proper and it stays in memory afterwards. With MON, a board
can be built and booted with nothing but U-Boot.

MON is new and does much less than BL31. See `Limitations`_ before using it.

Boot flow
---------

SPL starts MON in the same way as it starts BL31, so nothing in SPL is specific
to MON:

#. SPL loads the FIT, which holds MON in the place where BL31 would be, along
   with U-Boot proper and its devicetree
#. SPL jumps to MON at EL3, with the MMU and caches off. It passes TF-A's
   parameters in ``x0``, which give the entry point of U-Boot proper (BL33),
   and the address of the devicetree in ``x1``
#. MON sets up a stack for each CPU, clears its BSS and installs its exception
   vectors
#. MON sets up the things which only EL3 can (see below)
#. MON starts U-Boot proper at EL2 in the non-secure state, with SMCs enabled
   so that the lower exception levels can reach it
#. From then on MON only runs when U-Boot proper or the OS makes an SMC

MON accepts both the older ``bl31_params`` and the newer ``bl_params`` forms
of TF-A's parameters.

What MON sets up
----------------

Before it starts U-Boot proper, MON does the following:

Timer frequency
    Only EL3 can set the frequency which the generic timer reports
    (``CNTFRQ_EL0``). MON sets it from ``CONFIG_COUNTER_FREQUENCY``, the
    option which the other phases use, unless that is zero.

Interrupt controller
    Out of reset every interrupt of a GICv3 is in the secure group, and the
    lower exception levels cannot use its system registers. MON puts all
    interrupts in Group 1 non-secure, wakes the redistributor of the boot CPU
    and lets EL2 and EL1 use the system registers.

SoC security settings
    MON calls ``mon_board_init()``, which the SoC can provide. On the RK3399
    this lets the non-secure side reach the bus masters and slaves which are
    secure-only out of reset.

Services
--------

MON uses ``arch/arm/cpu/armv8/psci.S`` for its exception vectors and to
dispatch PSCI calls. It reports PSCI v0.2. A call which has no implementation
returns ``NOT_SUPPORTED``.

.. list-table::
   :header-rows: 1

   * - Call
     - Provided by
     - Notes
   * - ``PSCI_VERSION``
     - MON
     - Returns 0.2
   * - ``SYSTEM_RESET``
     - SoC
     - Implemented for the RK3399
   * - ``CPU_ON``, ``CPU_OFF``, ``CPU_SUSPEND``, ``SYSTEM_OFF``
     - SoC
     - Not implemented yet

An SoC implements a call by defining the function which ``psci.S`` looks for,
such as ``psci_system_reset()``, marked ``__secure``.

Running U-Boot proper at EL3
----------------------------

MON normally starts U-Boot proper at EL2, so that only MON runs at EL3. With
``CONFIG_MON_SWITCH_TO_EL2`` disabled, MON starts U-Boot proper at EL3
instead. U-Boot proper can then see the secure state, which is useful when
working on MON, for example to look at registers which only EL3 can read. The
``bdinfo`` command shows the exception level, as ``CurrentEL``.

U-Boot proper uses its own exception vectors at EL3, so MON cannot be called
while U-Boot proper runs there: the ``smc`` command does not work and U-Boot's
PSCI driver does not start.

When U-Boot proper drops to EL2, either to start the OS or to run an EFI app,
it first calls a second entry point in MON, at ``CONFIG_MON_TEXT_BASE + 4``.
This puts MON's vectors back and returns. From then on MON handles SMCs as
usual, so the OS sees no difference.

Nothing protects MON from U-Boot proper in this mode, so it is intended for
development only.

Finding devices
---------------

MON needs the addresses of the devices it sets up. There are two ways for it
to get them, selected with a Kconfig choice:

.. list-table::
   :header-rows: 1

   * - Option
     - How it works
     - Size on the RK3399
   * - ``CONFIG_MON_FDT`` (default)
     - MON has no devicetree of its own. It reads the one for U-Boot proper,
       which SPL passes to it, using libfdt. The interrupt controller is the
       node which is compatible with ``arm,gic-v3``: its first ``reg`` entry
       is the distributor and its second is the redistributors
     - About 10KB
   * - ``CONFIG_MON_BARE``
     - MON takes the addresses from ``CONFIG_MON_GICD_BASE`` and
       ``CONFIG_MON_GICR_BASE`` and does not use a devicetree. libfdt is not
       built for MON
     - About 6KB

With ``CONFIG_MON_FDT``:

- SPL must pass the devicetree, so ``CONFIG_SPL_ATF_NO_PLATFORM_PARAM`` is not
  available
- MON does not translate addresses, so the node must be at the top level, or
  below buses which map their addresses directly
- if there is no usable node, MON stops, since U-Boot proper cannot run
  without an interrupt controller

Memory
------

MON is linked to run at ``CONFIG_MON_TEXT_BASE`` and must fit within
``CONFIG_MON_MAX_SIZE``. This includes a 1KB stack for each of
``CONFIG_MON_CPUS`` CPUs, which is used while MON handles a call from that CPU.

The memory must be out of the way of U-Boot proper and the OS, since MON stays
there while they run.

Configuration
-------------

.. list-table::
   :header-rows: 1

   * - Option
     - Meaning
   * - ``CONFIG_SUPPORT_MON``
     - Selected by an SoC which has support for MON
   * - ``CONFIG_MON``
     - Enables MON. It needs ``CONFIG_SPL``
   * - ``CONFIG_MON_FDT``, ``CONFIG_MON_BARE``
     - How MON finds its devices (see above)
   * - ``CONFIG_MON_SWITCH_TO_EL2``
     - Start U-Boot proper at EL2, which is the default, rather than at EL3
   * - ``CONFIG_MON_TEXT_BASE``
     - Address which MON is linked and runs at
   * - ``CONFIG_MON_MAX_SIZE``
     - Maximum size of MON, including its stacks
   * - ``CONFIG_MON_CPUS``
     - Number of CPUs which MON supports
   * - ``CONFIG_COUNTER_FREQUENCY``
     - Frequency of the generic timer. This is not specific to MON
   * - ``CONFIG_MON_GICD_BASE``, ``CONFIG_MON_GICR_BASE``
     - Addresses of the GICv3, with ``CONFIG_MON_BARE``
   * - ``CONFIG_MON_LDSCRIPT``
     - Linker script for MON

MON is listed in ``scripts/xpl_phases`` as a resident phase with no devicetree
of its own::

    mon	-		resident

Building
--------

No board enables MON by default. To build a board with it, enable
``CONFIG_MON`` and leave out BL31. With buildman:

.. code-block:: bash

    buildman -a MON --board firefly-rk3399

or, for the build which does not use the devicetree:

.. code-block:: bash

    buildman -a MON,MON_BARE,~MON_FDT --board firefly-rk3399

With make:

.. code-block:: bash

    make firefly-rk3399_defconfig
    ./scripts/config --enable MON
    make olddefconfig
    make CROSS_COMPILE=aarch64-linux-gnu-

MON is built in the ``mon/`` subdirectory, as ``mon/u-boot-mon`` and
``mon/u-boot-mon.bin``. On Rockchip boards the FIT holds it where BL31 would
otherwise be.

Checking that it works
----------------------

With ``CONFIG_CMD_SMC`` enabled, ask for the PSCI version from the U-Boot
command line::

    => smc 0x84000000
    Res:  0x2 0x0 0x0 0x0

When Linux boots it reports the same version, and that it started at EL2::

    psci: PSCIv0.2 detected in firmware.
    CPU: All CPU(s) started at EL2

A reset through PSCI can be tried with ``smc 0x84000009``.

Supporting a new SoC
--------------------

#. Select ``CONFIG_SUPPORT_MON`` from the SoC's Kconfig option
#. Provide defaults for ``CONFIG_MON_TEXT_BASE`` and ``CONFIG_MON_CPUS``,
   along with the addresses of the GICv3 for ``CONFIG_MON_BARE``, and make
   sure that ``CONFIG_COUNTER_FREQUENCY`` is set
#. Provide ``mon_board_init()`` if the SoC has security settings which stop
   the non-secure side from using parts of it
#. Provide the PSCI calls which the SoC supports, such as
   ``psci_system_reset()``
#. Arrange for the image to hold MON where BL31 would be, when ``CONFIG_MON``
   is enabled

Code which is only for MON can be built with ``ifdef CONFIG_MON_BUILD`` in a
Makefile.

Limitations
-----------

- Only the RK3399 is supported
- Only the boot CPU runs. ``CPU_ON`` is not implemented, so the OS cannot start
  the others
- Powering off and suspending are not implemented
- Only a GICv3 is set up
- MON has no console, so it cannot report a problem
- There is no support for a trusted OS (BL32) such as OP-TEE

Driver model
------------

MON does not use driver model. It would be useful, for example to give MON a
console with the existing serial drivers, to use a sysreset driver in place of
code written for each SoC, or to set up the interrupt controller with a proper
driver. A third build option, alongside ``CONFIG_MON_FDT`` and
``CONFIG_MON_BARE``, could provide it.

Starting driver model in MON is much the same as in SPL: it needs global data,
a small heap within MON's own memory and a devicetree, which can be the one
SPL already passes. The difficulties come from MON being resident, so that it
runs later, when the OS calls it:

Global data
    On ARMv8 the pointer to global data is kept in register ``x18``. The OS
    may use ``x18`` for its own purposes, so on each SMC MON would have to
    switch ``x18`` to its own global data, and back again before it returns.
    The PSCI code saves ``x18`` but does not do this.

Several CPUs
    Once the other CPUs are running, more than one of them can make an SMC at
    the same time. Driver model is not written for this, so a handler which
    uses a device would need a lock, or the use of driver model would have to
    be limited to start-up.

No caches
    MON runs with the MMU and caches off. Driver model works like this, but
    walking its lists of devices is slow, so a call which the OS makes often
    would take noticeably longer.

Kconfig options
    MON would need its own versions of the options for driver model, the
    devicetree, the heap and each driver. Adding an option such as
    ``CONFIG_MON_FOO`` makes ``CONFIG_FOO`` an option which has a version for
    each phase, so it is no longer passed to phases which have no version of
    their own. This is harmless where ``CONFIG_SPL_FOO`` already exists, but
    otherwise it changes SPL and TPL. Defining the options with ``phases``
    (see :doc:`spl`) avoids this.

Drivers
    There is no driver which sets up a GICv3 from the secure state: MON does
    this with its own code, which finds the device by hand. With driver model
    a proper driver could be added, bound from the devicetree like any other,
    and the same could be done for other interrupt controllers.

Size
    Driver model and a few drivers are several times the size of MON as it
    is now. This is fine where MON has plenty of memory, but some SoCs only
    have a small amount of secure memory.

Source code
-----------

.. list-table::
   :header-rows: 1

   * - File
     - Contents
   * - ``arch/arm/cpu/armv8/mon_start.S``
     - Entry point
   * - ``arch/arm/cpu/armv8/mon.c``
     - Set-up of the timer and the interrupt controller
   * - ``arch/arm/cpu/armv8/psci.S``
     - Exception vectors and PSCI dispatch, shared with U-Boot proper
   * - ``arch/arm/cpu/armv8/transition.S``
     - Switch to a lower exception level, shared with U-Boot proper
   * - ``arch/arm/include/asm/mon.h``
     - Entry point which U-Boot proper uses to hand EL3 back to MON
   * - ``arch/arm/cpu/armv8/u-boot-mon.lds``
     - Linker script
   * - ``arch/arm/mach-rockchip/rk3399/mon.c``
     - RK3399 security settings and reset
   * - ``common/spl/Kconfig.mon``
     - Options
