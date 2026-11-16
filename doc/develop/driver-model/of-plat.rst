.. SPDX-License-Identifier: GPL-2.0+

Compiled-in Device Tree / Platform Data
=======================================


Introduction
------------

Device tree is the standard configuration method in U-Boot. It is used to
define what devices are in the system and provide configuration information
to these devices.

The overhead of adding devicetree access to U-Boot is fairly modest,
approximately 3KB on Thumb 2 (plus the size of the DT itself). This means
that in most cases it is best to use devicetree for configuration.

However there are some very constrained environments where U-Boot needs to
work. These include SPL with severe memory limitations. For example, some
SoCs require a 16KB SPL image which must include a full MMC stack. In this
case the overhead of devicetree access may be too great.

It is possible to create platform data manually by defining C structures
for it, and reference that data in a `U_BOOT_DRVINFO()` declaration. This
bypasses the use of devicetree completely, effectively creating a parallel
configuration mechanism. But it is an available option for SPL.

As an alternative, the 'of-platdata' feature is provided. This converts the
devicetree contents into C code which can be compiled into the SPL binary.
This saves the 3KB of code overhead and perhaps a few hundred more bytes due
to more efficient storage of the data.


How it works
------------

The feature is enabled by CONFIG_SPL_OF_PLATDATA or CONFIG_TPL_OF_PLATDATA.
This is only available in SPL/TPL and should be tested with:

.. code-block:: c

    #if IS_ENABLED(CONFIG_OF_PLATDATA)

A tool called 'dtoc' converts a devicetree file into a `U_BOOT_DRVINFO()`
declaration for each node, along with a struct holding the properties which
the node's driver reads.

dtoc is maintained as a separate project, at https://github.com/deinde-dev/dtoc,
and is installed as the dtoc package::

    pip install dtoc

The U-Boot build runs the ``dtoc`` on the PATH, or another copy if set with
``make DTOC=/path/to/dtoc``. Its documentation is at
https://dtoc.readthedocs.io/

As an example, consider this MMC node:

.. code-block:: none

    sdmmc: dwmmc@ff0c0000 {
            compatible = "rockchip,rk3288-dw-mshc";
            clock-freq-min-max = <400000 150000000>;
            clocks = <&cru HCLK_SDMMC>, <&cru SCLK_SDMMC>,
                     <&cru SCLK_SDMMC_DRV>, <&cru SCLK_SDMMC_SAMPLE>;
            clock-names = "biu", "ciu", "ciu_drv", "ciu_sample";
            fifo-depth = <0x100>;
            interrupts = <GIC_SPI 32 IRQ_TYPE_LEVEL_HIGH>;
            reg = <0xff0c0000 0x4000>;
            bus-width = <4>;
            cap-mmc-highspeed;
            cap-sd-highspeed;
            card-detect-delay = <200>;
            disable-wp;
            num-slots = <1>;
            pinctrl-names = "default";
            pinctrl-0 = <&sdmmc_clk>, <&sdmmc_cmd>, <&sdmmc_cd>, <&sdmmc_bus4>;
            vmmc-supply = <&vcc_sd>;
            status = "okay";
            bootph-all;
    };

dtoc finds the driver for the node from its compatible strings, trying each
in turn against the `of_match` table of every driver in the source tree, as
driver model does at run time with a devicetree. It then scans the driver for
the `dev_prop_...()` helpers which it uses to read its properties, described
below, and generates a struct with a member for each of those properties.
The rest of the node is left out. It also generates a device declaration:

.. code-block:: c

    /* Node /dwmmc@ff0c0000 index 2 */
    U_BOOT_DRVINFO(dwmmc_at_ff0c0000) = {
            DM_DRVINFO_DRIVER(rockchip_rk3288_dw_mshc),
            .plat           = &dtv_dwmmc_at_ff0c0000,
            .parent_idx     = -1,
    };

The device is bound at run time from this record. `DM_DRVINFO_DRIVER()` names
the driver, as given to `U_BOOT_DRIVER()`, and sets the record's `drv` member
to point to it, so the driver need not be looked up by name. 'plat' points to
the generated data, which the driver reads in place. See
`What dtoc generates`_ for an example of the struct and its data.

A devicetree for SPL or TPL often has nodes whose driver is not built in that
phase, such as a pin controller which is there only because its children are
needed. The build lists the drivers which each phase has, in `dt-drivers.h`,
and `DM_DRVINFO_DRIVER()` gives NULL for a driver which is not in the list.
Such a record binds no device.

A node whose driver reads no properties has a record with no data. The record
is still needed so that the device is bound, and so that the indexes which
other nodes use to refer to it stay the same.

The parent_idx is the index of the parent `driver_info` structure within its
linker list (instantiated by the `U_BOOT_DRVINFO()` macro). This is used to
support `dev_get_parent()`.

Note that the `of_to_plat()` method is still called with of-platdata, so it
must not try to read the devicetree, which is not there: the devicetree access
code is not compiled in. A board can therefore only use of-platdata if all the
drivers it needs in that phase support it.

.. note::

   dtoc used to generate a struct for each node holding all of its properties,
   named after the node's first compatible string, which driver model copied
   into the start of the driver's plat data. A driver then needed a
   `dtplat` member at the start of its plat struct, separate code to read it
   and a `DM_DRIVER_ALIAS()` for each further compatible string. All of that
   has been removed. dtoc still supports it for other trees, but U-Boot runs
   dtoc with `--props-only`.


Converting of-platdata to a useful form
---------------------------------------

The generated data should be used only to fill in the driver's own plat
struct, which the rest of the driver then uses. With the dev_prop helpers this
happens in the `of_to_plat()` method, which is called with of-platdata as well
as with a devicetree, so the same code handles both.


How to structure your driver
----------------------------

Drivers should always support devicetree as an option. The of-platdata
feature is intended as an add-on to existing drivers.

Reading properties with the dev_prop helpers
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The simplest way to support of-platdata is to read properties with the
helpers in `dm/prop.h`. Each one reads a property with `dev_read_...()`
normally, and from the C structure generated by dtoc when of-platdata is
enabled, with the same default in both cases. So the driver has just one
`of_to_plat()` method and no `#if`:

.. code-block:: c

    #include <dm/prop.h>

    static int mmc_of_to_plat(struct udevice *dev)
    {
            struct mmc_plat *plat = dev_get_plat(dev);
            DTPLAT(mmc_drv, dt, dev);

            plat->base = dev_prop_addr(dev, dt);
            plat->fifo_depth = dev_prop_u32_default(dev, dt, fifo_depth,
                                                    "fifo-depth", 0);
            plat->removable = !dev_prop_bool(dev, dt, non_removable,
                                             "non-removable");

            return dev_prop_clk(dev, dt, clocks, "clocks", 0, &plat->clk);
    }

`DTPLAT()` names the driver, as given to `U_BOOT_DRIVER()`, and declares a
pointer to the device's generated data, or nothing when of-platdata is not
in use. Each property is given as its C member name and as its devicetree
name, since the C preprocessor cannot convert one to the other. dtoc checks
that they match.

dtoc scans the driver for these helpers and generates `struct dtd_drv_<driver>`
holding just the properties they read, whichever compatible string each node
uses. For a property which may be absent,
the struct has a flag in its `has` member saying whether the node has it, so
the default is used otherwise. The data is not copied into the driver's
plat struct, which therefore needs no of-platdata member; it is read in place
with `dev_get_dtplat()`.

The helpers are listed below. Each returns the same values as the
`dev_read_...()` function it wraps, including errors, so the driver's error
handling is the same with and without of-platdata, apart from the limitations
described below. See :doc:`/api/dm` for the details.

dev_prop_addr(), dev_prop_addr_ptr()
    Address from the 'reg' property, as an address or a pointer

dev_prop_u32()
    32-bit integer; returns -EINVAL if the property is absent

dev_prop_u32_default()
    32-bit integer, or a default if the property is absent

dev_prop_bool()
    True if the property is present

dev_prop_u32_array()
    Array of 32-bit integers; returns -EINVAL if the property is absent

dev_prop_u32_array_ptr()
    Pointer to an array of 32-bit integers, for a driver which only reads it;
    with of-platdata nothing is copied

dev_prop_u32_array_rw()
    Pointer to an array of 32-bit integers which the driver may write to; with
    of-platdata nothing is copied and the array is the only writable part of
    the generated data

dev_prop_size()
    Size of a property in bytes, e.g. to allocate space for an array

dev_prop_regmap()
    Regmap with a range for each address/size pair in the 'reg' property

dev_prop_pci_devfn()
    PCI device and function from the first cell of the 'reg' property

dev_prop_driver_data()
    The `.data` in the driver's `of_match` table for the node's compatible
    string; the driver needs `DM_HEADER()` for any headers this uses

dev_prop_clk()
    Clock from the 'clocks' property, by index

dev_prop_clk_id()
    ID of a clock in the 'clocks' property, by index, for a platform with no
    clock driver in the current phase

dev_prop_gpio()
    GPIO from a GPIO-list property, by index

dev_prop_irq()
    Interrupt from the 'interrupts-extended' property, by index

dev_prop_dev()
    Device which a phandle with no arguments refers to

dev_prop_string()
    String, or NULL if absent

Declaring the driver
''''''''''''''''''''

The driver declaration needs no `#if` either:

.. code-block:: c

    static __maybe_unused const struct udevice_id mmc_ids[] = {
            { .compatible = "vendor,mmc" },
            { }
    };

    U_BOOT_DRIVER(mmc_drv) = {
            .name           = "mmc_drv",
            .id             = UCLASS_MMC,
            .of_match       = of_match_ptr(mmc_ids),
            .of_to_plat     = of_to_plat_ptr(mmc_of_to_plat),
            .probe          = mmc_probe,
            .priv_auto      = sizeof(struct mmc_priv),
            .plat_auto      = sizeof(struct mmc_plat),
    };

`of_match_ptr()` drops the compatible strings when there is no devicetree at
run time, since only dtoc needs them then, so the `udevice_id` table is marked
`__maybe_unused`, before `const` so that dtoc still finds it.
`of_to_plat_ptr()` keeps the method
with of-platdata, since that is where the helpers read the generated data, and
drops it only when OF_CONTROL is disabled. A driver which is also used without
OF_CONTROL, such as ns16550, puts its `of_to_plat()` method inside
`#if IS_ENABLED(CONFIG_OF_CONTROL)`, since the static function would otherwise
be unused.

`plat_auto` is the size of the driver's own plat struct, which has no member for
the generated data, so it is the same with and without of-platdata.

The driver's `.name` must be the name given to `U_BOOT_DRIVER()`, since dtoc
uses that in each `U_BOOT_DRVINFO()` record and driver model finds the driver
by its `.name`.

The driver does not need `DM_FLAG_PRE_RELOC` for of-platdata, since SPL binds
every device which dtoc generates, whatever its flags.

What dtoc generates
'''''''''''''''''''

As an example, the ns16550 driver reads seven properties. For
chromebook_jerry, dtoc generates this struct in `dt-structs-gen.h`:

.. code-block:: c

    struct dtd_drv_ns16550_serial {
            fdt32_t         clock_frequency;
            struct phandle_1_arg clocks[2];
            fdt64_t         reg[2];
            fdt32_t         reg_io_width;
            fdt32_t         reg_offset;
            fdt32_t         reg_shift;
            bool            skip_init;
            struct {
                    unsigned int clock_frequency:1;
                    unsigned int clocks:1;
                    unsigned int reg_io_width:1;
                    unsigned int reg_offset:1;
                    unsigned int reg_shift:1;
            } has;
    };

and this for the UART in `dt-plat.c`:

.. code-block:: c

    static const struct dtd_drv_ns16550_serial dtv_serial_at_ff690000 = {
            .clock_frequency        = 0x16e3600,
            .clocks                 = {
                            {0, {79}},
                            {0, {343}},},
            .reg                    = {0xff690000, 0x100},
            .reg_io_width           = 0x4,
            .reg_shift              = 0x2,
            .has = {
                    .clock_frequency = 1,
                    .clocks = 1,
                    .reg_io_width = 1,
                    .reg_shift = 1,
            },
    };
    U_BOOT_DRVINFO(serial_at_ff690000) = {
            DM_DRVINFO_DRIVER(ns16550_serial),
            .plat           = &dtv_serial_at_ff690000,
            .parent_idx     = -1,
    };

Properties which the driver does not read, such as 'interrupts' and 'status',
are left out. The node has no 'reg-offset' property, so its flag is clear and
`dev_prop_u32_default()` returns the default. Boolean properties need no flag,
since they are false when absent, and nor does 'reg', since dtoc reports an
error for a node which lacks it. Each member is sized for the node with the
most values. The type of 'reg' follows the parent's `#address-cells` and
`#size-cells`, which are both 2 at the root of the rk3288 devicetree, hence
`fdt64_t`.

The struct is generated for every driver which uses the helpers, even if no
node uses it, since the driver's source refers to it.

How the data reaches the driver
'''''''''''''''''''''''''''''''

The generated data is declared const, so the compiler can place it in
read-only memory and only one copy of it is needed. It is never copied into the
driver's plat data. Instead `DTPLAT()` finds it using a
`DTPLAT_GET_<driver>()` macro, which dtoc generates in `dt-prop.h`. Only
`dm/prop.h` includes this header, so only drivers which use the helpers see it.

When only one node uses the driver, dtoc puts that node's data in
`dt-prop.h` too, and the macro refers to it directly. The compiler can
then see the values when it compiles the driver, so it can drop code for
properties which the node does not have and use the values directly. On
chromebook_jerry, for example, this cuts the ns16550 driver's `of_to_plat()`
method from 190 to 68 bytes. The device does not point to the data in this
case.

Since the driver uses the node's data whatever the device, any device of the
driver must come from the devicetree. A device which C code binds at run time
would get the node's data too. To catch this, enable OF_PLATDATA_CHECK, which
reports an error for such a device. It is enabled by default for sandbox, but
not for other boards, since it adds a little code.

Otherwise the macro uses `dev_get_dtplat()`, which returns the data that the
device's `dtplat_` member points to. How the device gets its plat data, and
`dtplat_`, depends on whether of-platdata is used:

With a devicetree
    `DTPLAT()` declares nothing and each helper calls the matching
    `dev_read_...()` function. The plat struct is allocated from `plat_auto` as
    usual and there is no `dtplat_` member.

With of-platdata
    dtoc generates a `U_BOOT_DRVINFO()` record whose `plat` points to the
    generated data. When `device_bind_by_name()` binds the device, it sets
    `dtplat_` to the record's `plat`. For the only node of a driver, and for a
    driver which reads no properties, `plat` is NULL.

    If the driver has `plat_auto`, a zeroed plat struct of that size is
    allocated, as with a devicetree, and `of_to_plat()` fills it in. Otherwise
    the device has no plat data.

The generated data is never the device's plat data, so `dev_get_plat()` returns
only what the driver itself stores. Put anything the driver stores in its
`plat_auto` or `priv_auto` struct, and never write to the generated data.

Always use `DTPLAT()` to get the generated data, rather than
`dev_get_dtplat()`, which returns NULL for the only node of a driver.

Rules and limitations
'''''''''''''''''''''

dtoc finds the helpers by scanning the source code, which brings some rules:

- `DTPLAT()` must be in the same file as the helpers, since dtoc only looks for
  helpers in files which contain it. The properties are recorded against each
  driver named in a `DTPLAT()` in the file.

- The member name and property name must be written out in each call, as an
  identifier and a string literal. They cannot come from a variable or another
  macro.

- The first two arguments must be simple: the device as a plain expression
  with no brackets or commas, such as `dev`, and the variable declared by
  `DTPLAT()`. dtoc does not recognise a call such as
  `dev_prop_bool(dev_get_parent(dev), dt, ...)`.

- The member name must be the property name with each '-', ',' and '.' changed
  to '_', e.g. `u_boot_spl_fifo_mode` for "u-boot,spl-fifo-mode". dtoc reports
  an error if they do not match.

A call which dtoc does not recognise is left out of the struct, so the
of-platdata build fails with an error saying that `struct dtd_drv_<driver>` has
no such member. The devicetree build is not affected, so build a board which
uses of-platdata to check.

Some limitations follow from the generated data:

- Only the property types above are supported. A driver which needs another,
  such as a byte array, can add a helper to `dm/prop.h`, with matching support
  in dtoc. The per-driver struct holds only the properties read with the
  helpers, so a driver cannot read other properties from it directly.

- With of-platdata, `dev_prop_addr()` and `dev_prop_regmap()` use the
  addresses as they are in the devicetree, without translation through the
  'ranges' properties of parent nodes.

- With of-platdata, an array property shorter than the count passed to
  `dev_prop_u32_array()` is padded with zeroes rather than giving an error.
  For the same reason, `dev_prop_size()` gives the size of the array for the
  node with the most values.

- With of-platdata, `dev_prop_regmap()` gives each node as many ranges as the
  node with the most, so a node with fewer has empty ranges at the end.

The helpers need a version of dtoc which supports them. The version which
U-Boot needs is in `tools/dtoc/requirements.txt`.

Converting an existing driver
'''''''''''''''''''''''''''''

To convert a driver which reads the devicetree only when OF_REAL is enabled, so
that it works with of-platdata too:

#. Add `DTPLAT()` at the start of `of_to_plat()` and change each read of the
   devicetree there, whether with `dev_read_...()`, `ofnode_...()` or
   `fdtdec_...()`, to the matching `dev_prop_...()` helper. Drop any
   `if (IS_ENABLED(CONFIG_OF_REAL))` around these reads, so that the same code
   runs with of-platdata.

#. Check the defaults. Where the of-platdata code used a different default from
   the devicetree code, the converted driver has just one, normally the
   devicetree one, so a board which relied on the other may need the property
   in its devicetree. For example, the Rockchip SPI driver defaulted to 20MHz
   with of-platdata but 50MHz with a devicetree, so
   `rk3288-veyron-u-boot.dtsi` now sets 'spi-max-frequency' to 20MHz, since
   SPL cannot read the flash reliably at 50MHz.

#. Use `of_match_ptr()` and `of_to_plat_ptr()` in the driver declaration and
   make `plat_auto` unconditional, as above, marking the `udevice_id` table
   `__maybe_unused`.

#. Build a board which uses the driver with of-platdata, check the struct in
   `spl/include/generated/dt-structs-gen.h` and compare the SPL size before and
   after. Then boot it.

`drivers/spi/rk_spi.c`, `drivers/mmc/rockchip_dw_mmc.c` and
`drivers/serial/ns16550.c` are examples of converted drivers.

The `sandbox_dev_prop_test` driver in `drivers/misc/prop_sandbox.c` uses each
helper. The `dm_test_dev_prop` test checks that it reads the same values with a
devicetree (sandbox) and with both forms of of-platdata (sandbox_noinst and
sandbox_spl). When adding a helper, add a property which uses it to the
dev-prop-test node in `arch/sandbox/dts/sandbox.dtsi` and
`arch/sandbox/dts/test.dts`, read it in the driver and check it in the test.

Indexes
-------

U-Boot stores drivers, devices and many other things in linker_list structures.
These are sorted by name, so dtoc knows the order that they will appear when
the linker runs. Each driver_info is referenced by its index in the linker_list
array, called 'idx' in the code.

Indexes are used to reference devices using device_get_by_ofplat_idx(). This
allows phandles to work as expected.


Phases
------

U-Boot operates in several phases, typically TPL, SPL and U-Boot proper.
The latter does not use dtoc.

In some rare cases different drivers are used for two phases. For example,
in TPL it may not be necessary to use the full PCI subsystem, so a simple
driver can be used instead.

This works in the build system simply by compiling in one driver or the
other (e.g. PCI driver + uclass for SPL; simple_bus for TPL). But dtoc has
no way of knowing which code is compiled in for which phase, since it does
not inspect Makefiles or dependency graphs.

So to make this work for dtoc, we need to be able to explicitly mark
drivers with their phase. This is done by adding a macro to the driver::

   /* code in tpl.c only compiled into TPL */
   U_BOOT_DRIVER(pci_x86) = {
      .name   = "pci_x86",
      .id   = UCLASS_SIMPLE_BUS,
      .of_match = of_match_ptr(tpl_fake_pci_ids),
      DM_PHASE(tpl)
   };


   /* code in pci_x86.c compiled into SPL and U-Boot proper */
   U_BOOT_DRIVER(pci_x86) = {
      .name   = "pci_x86",
      .id   = UCLASS_PCI,
      .of_match = pci_x86_ids,
      .ops   = &pci_x86_ops,
   };


Notice that the second driver has the same name but no DM_PHASE(), so it will be
used for SPL and U-Boot.

Note also that this only affects the code generated by dtoc. You still need to
make sure that only the required driver is build into each phase.


Header files
------------

A driver which uses `dev_prop_driver_data()` has the `.data` value from its
`struct udevice_id` table written into the generated code. This is often an
enum. Since dtoc does not support searching for these, you must use the
`DM_HEADER()` macro to tell dtoc which header to include. This works as a macro
included in the driver definition::

   static const struct udevice_id apl_syscon_ids[] = {
      { .compatible = "intel,apl-punit", .data = X86_SYSCON_PUNIT },
      { }
   };

   U_BOOT_DRIVER(intel_apl_punit) = {
      .name       = "intel_apl_punit",
      .id         = UCLASS_SYSCON,
      .of_match   = apl_syscon_ids,
      .probe      = apl_punit_probe,
      DM_HEADER(<asm/cpu.h>)    /* for X86_SYSCON_PUNIT */
   };



Problems
--------

This section shows some common problems and how to fix them.

Driver not found
~~~~~~~~~~~~~~~~

In some cases you will you see something like this::

   WARNING: the driver rockchip_rk3188_grf was not found in the driver list

The driver list is a list of drivers, each with a name. The name is in the
U_BOOT_DRIVER() declaration, repeated twice, one in brackets and once as the
.name member. For example, in the following declaration the driver name is
`rockchip_rk3188_grf`::

  U_BOOT_DRIVER(rockchip_rk3188_grf) = {
       .name = "rockchip_rk3188_grf",
       .id = UCLASS_SYSCON,
       .of_match = rk3188_syscon_ids + 1,
       .bind = rk3188_syscon_bind_of_plat,
  };

The first name U_BOOT_DRIVER(xx) is used to create a linker symbol so that the
driver can be accessed at build-time without any overhead. The second one
(.name = "xx") is used at runtime when something wants to print out the driver
name.

The dtoc tool expects to be able to find a driver for each node in the
devicetree. For example, if the devicetree has::

   grf: grf@20008000 {
      compatible = "rockchip,rk3188-grf", "syscon";
      reg = <0x20008000 0x200>;
      bootph-pre-ram;
   };

then dtoc looks for a driver with "rockchip,rk3188-grf" in its `of_match`
table, then for one with "syscon". Each entry in the table must be on a single
line, with no comma before the closing brace, for dtoc to see it.

Missing .compatible or Missing .id
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Various things can cause dtoc to fail to find the driver and it tries to
warn about these. For example::

   vendor_soc_uart: Missing .compatible in drivers/serial/serial_vendor.c
                  : WARNING: the driver vendor_soc_uart was not found in the driver list

Without a compatible string a driver cannot be used by dtoc, even if the
compatible string is not actually needed at runtime.

Checks are also made to confirm that the referenced driver has a .compatible
member and a .id member. The first provides the array of compatible strings and
the second provides the uclass ID.

Missing parent
~~~~~~~~~~~~~~

When a device is used, its parent must be present as well. If you see an error
like::

   Node '/i2c@0/emul/emul0' requires parent node '/i2c@0/emul' but it is not in
      the valid list

it indicates that you are using a node whose parent is not present in the
devicetree. In this example, if you look at the device tree output
(e.g. fdtdump tpl/u-boot-tpl.dtb in your build directory), you may see something
like this::

   emul {
       emul0 {
           compatible = "sandbox,i2c-rtc-emul";
           #emul-cells = <0x00000000>;
           phandle = <0x00000003>;
       };
   };

In this example, 'emul0' exists but its parent 'emul' has no properties. These
have been dropped by fdtgrep in an effort to reduce the devicetree size. This
indicates that the two nodes have different phase settings. Looking at the
source .dts::

   i2c_emul: emul {
      bootph-pre-ram;
      reg = <0xff>;
      compatible = "sandbox,i2c-emul-parent";
      emul0: emul0 {
         bootph-all;
         compatible = "sandbox,i2c-rtc-emul";
         #emul-cells = <0>;
      };
   };

you can see that the child node 'emul0' usees 'bootph-all', indicating
that the node is present in all SPL builds, but its parent uses
'bootph-pre-ram' indicating it is only present in SPL, not TPL. For a TPL
build, this will fail with the above message. The fix is to change 'emul0' to
use the same 'bootph-pre-ram' condition, so that it is not present in TPL,
like its parent.

Records not from dtoc
~~~~~~~~~~~~~~~~~~~~~

When binding devices at run time, a device refers to its parent, and to other
devices through phandles, by the index of their `driver_info` record. dtoc
works out these indexes from the records it generates, so they are only correct
if there are no other records. `U_BOOT_DRVINFO()` and `U_BOOT_DRVINFOS()`
therefore give a build error with of-platdata, except in the files generated by
dtoc.

The build also checks each SPL or TPL image after linking, in case a record
arrives some other way::

   spl/u-boot-spl: driver_info records which are not from dtoc:
      at91sam9260_gpios
   With of-platdata, use the devicetree to create devices

The fix is to create the device from the devicetree instead, or to drop the
record when of-platdata is enabled.

Caveats
-------

There are various complications with this feature which mean it should only
be used when strictly necessary, i.e. in SPL with limited memory. Notable
caveats include:

   - Device tree does not describe data types. But the C code must define a
     type for each property. These are guessed using heuristics which
     are wrong in several fairly common cases. For example an 8-byte value
     is considered to be a 2-item integer array, and is byte-swapped. The
     dev_prop helpers avoid much of this, since each helper says what type it
     expects.

   - Naming of nodes and properties is automatic. This means that they follow
     the naming in the devicetree, which may result in C identifiers that
     look a bit strange.

   - A driver can only read the kinds of property for which there is a
     dev_prop helper.


Internals
---------

Generated files
~~~~~~~~~~~~~~~

When enabled, dtoc generates the following files. They are shown here for
SPL; TPL and VPL each have their own, under tpl/ and vpl/, since each phase has
its own devicetree and the phases are built in parallel:

spl/include/generated/dt-decl.h
   Contains declarations for the drivers and uclass drivers which the devices
   use, so that they can be located by name

spl/include/generated/dt-structs-gen.h
   Contains the struct definitions for the devicetree nodes that are used, plus
   a `struct dtd_drv_<driver>` for each driver which uses the dev_prop helpers

spl/include/generated/dt-prop.h
   Contains a `DTPLAT_GET_<driver>()` macro for each driver which uses the
   dev_prop helpers, along with the data for each such driver which has just
   one node. It is included only by `dm/prop.h`

spl/dts/dt-plat.c
   Contains the `U_BOOT_DRVINFO()` declarations that U-Boot uses to bind devices
   at start-up. See above for an example

spl/dts/dtoc.stamp
   Records when dtoc last ran. dtoc only writes one of the files above if its
   content has changed, so that the code which includes the headers is built
   again only when it needs to be

spl/dts/dtoc.stamp.d
   Lists the source files which dtoc relied on, so that the build runs it
   again if one of them changes. These are the files which declare a driver or
   a uclass driver, and each Makefile, since a new source file only matters
   once a Makefile builds it

The build itself generates one more:

spl/include/generated/dt-drivers.h
   Contains a `DM_HAS_DRIVER_<driver>` for each driver in the objects built
   for the phase. It is included only when compiling dt-plat.c, which is why
   that file is compiled after everything else

The dt-structs.h file includes the generated file
`(include/generated/dt-structs.h`) if CONFIG_SPL_OF_PLATDATA is enabled.
Otherwise (such as in U-Boot proper) these structs are not available. This
prevents them being used inadvertently. All direct usage must be bracketed with
`#if IS_ENABLED(CONFIG_OF_PLATDATA)`. Drivers which use the dev_prop helpers
need no `#if`, since `dm/prop.h` only refers to the structs with of-platdata.

The dt-plat.c file contains the device declarations and is built in
spl/dts/dt-plat.c.


CONFIG options
~~~~~~~~~~~~~~

Several CONFIG options are used to control the behaviour of of-platdata, all
available for both SPL and TPL:

OF_PLATDATA
   This is the main option which enables the of-platdata feature

OF_PLATDATA_PARENT
   This allows `device_get_parent()` to work. Without this, all devices exist as
   direct children of the root node. This option is highly desirable (if not
   always absolutely essential) for buses such as I2C.

OF_PLATDATA_CHECK
   This checks that each device of a driver which has just one node, and reads
   that node's data with the dev_prop helpers, was bound from the devicetree.
   See `How the data reaches the driver`_.

Data structures
~~~~~~~~~~~~~~~

An extra data structure is used with of-platdata:

`struct driver_rt`
   Run-time information for `struct driver_info` records. This holds a pointer
   to the device created by each record, so that a device can be found from
   its index with no searching or string comparison, e.g. for a phandle. This
   data structure is allocated on start-up, filled out when each device is
   bound and used by `device_get_by_ofplat_idx()`.

Other changes
~~~~~~~~~~~~~

Some other changes are made with of-platdata:

`gd->dm_driver_rt`
   This holds a pointer to a list of `struct driver_rt` records, one for each
   `struct driver_info`. The list is in alphabetical order by the name used
   in `U_BOOT_DRVINFO(name)` and indexed by idx, with the first record having
   an index of 0. This is accessed via macros so that it can be used inside
   IS_ENABLED(), rather than requiring #ifdefs in the C code when it is not
   present.

`struct udevice->node_`
   When OF_PLATDATA is enabled, there is no devicetree at runtime, so no need
   for this field. Its place is taken by `dtplat_`, which points to the
   generated data for the device, as returned by `dev_get_dtplat()`. This is
   used by drivers which read their properties with the dev_prop helpers.

`DM_PHASE`
   This macro is used to indicate which phase of U-Boot a driver is intended
   for. See above for details.

`DM_HEADER`
   This macro is used to indicate which header file dtoc should use to allow
   a driver declaration to compile correctly. See above for details.

`device_get_by_ofplat_idx()`
   There used to be a function called `device_get_by_driver_info()` which
   looked up a `struct driver_info` pointer and returned the `struct udevice`
   that was created from it. It was only available for use with of-platdata.
   This has been removed in favour of `device_get_by_ofplat_idx()` which uses
   `idx`, the index of the `struct driver_info` in the linker_list. Similarly,
   the `struct phandle_0_arg` (etc.) structs have been updated to use this
   index instead of a pointer to `struct driver_info`.

`DM_DRVINFO_GET`
   This has been removed since we now use indexes to obtain a driver from
   `struct phandle_0_arg` and the like.

Order of binding
   The idx value relies on alphabetical order, so that dtoc and U-Boot's
   linker_lists agree on it. Devices are bound in order of idx. dtoc names
   each record after the full path of its node, e.g. `i2c_at_0_rtc_at_43`,
   so that the name of a parent is a prefix of the names of its children and
   comes before them. Devices are therefore bound in a single pass, with each
   parent bound before its children, so that a child can find its parent in
   its bind() method if needed.

The dm_populate_phandle_data() function that was previous needed has now been
removed, since dtoc can address the drivers directly from dt-plat.c and does
not need to fix up things at runtime.

The pylibfdt Python module is used to access the devicetree.


Credits
-------

This is an implementation of an idea by Tom Rini <trini@konsulko.com>.


Future work
-----------
- Consider programmatically reading binding files instead of devicetree
  contents
- Add dev_prop helpers for more property types, such as byte arrays and
  lists of strings


.. Simon Glass <sjg@chromium.org>
.. Google, Inc
.. 6/6/16
.. Updated Independence Day 2016
.. Updated 1st October 2020
.. Updated 5th February 2021
