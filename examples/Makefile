# SPDX-License-Identifier: GPL-2.0+

# 'make clean' does not read the config, so it visits this directory only if
# the line is outside the ifdef; EXAMPLES_STANDALONE depends on EXAMPLES anyway
subdir-$(EXAMPLES_STANDALONE) += standalone

ifdef CONFIG_EXAMPLES

ifdef FTRACE
subdir-ccflags-y += -finstrument-functions -DFTRACE
endif

# examples/legacy_api/crt0.S is bare-metal only; skip on sandbox.
ifndef CONFIG_SANDBOX
subdir-$(CONFIG_LEGACY_API) += legacy_api
subdir-$(CONFIG_ULIB) += ulib
endif
endif
