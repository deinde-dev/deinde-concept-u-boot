# SPDX-License-Identifier: GPL-2.0+
#
# Copyright (c) 2013 Google, Inc

obj-y	+= device.o fdtaddr.o lists.o root.o uclass.o util.o tag.o

# With of-platdata and no devicetree, a driver which nothing refers to is left
# out of the image, so drivers cannot be looked up by name
ifneq ($(CONFIG_OF_PLATDATA)-$(CONFIG_OF_REAL),y-)
obj-y	+= drv-name.o
endif

obj-$(CONFIG_ACPIGEN) += acpi.o
obj-$(CONFIG_DEVRES) += devres.o
obj-$(CONFIG_DM_DEVICE_REMOVE)	+= device-remove.o
obj-$(CONFIG_SIMPLE_BUS)	+= simple-bus.o
obj-$(CONFIG_SIMPLE_PM_BUS)	+= simple-pm-bus.o
obj-$(CONFIG_DM)	+= dump.o
obj-$(CONFIG_REGMAP)	+= regmap.o
obj-$(CONFIG_SYSCON)	+= syscon-uclass.o
obj-$(CONFIG_OF_LIVE) += of_access.o of_addr.o
ifndef CONFIG_DM_DEV_READ_INLINE
obj-$(CONFIG_OF_CONTROL) += read.o
endif
obj-$(CONFIG_OF_PLATDATA) += read.o
obj-$(CONFIG_OF_CONTROL) += of_extra.o read_extra.o
obj-$(CONFIG_OF_REAL) += ofnode.o ofnode_graph.o

ccflags-$(CONFIG_DM_DEBUG) += -DDEBUG
