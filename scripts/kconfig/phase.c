// SPDX-License-Identifier: GPL-2.0+
/*
 * U-Boot: support for options which are defined once for several phases
 *
 * A config entry can have a 'phases' line listing the phases in which the
 * option exists, e.g.:
 *
 *	config DM_SERIAL
 *		bool "Enable Driver Model for serial drivers"
 *		phases ppl spl tpl
 *		depends on DM
 *
 * Copyright 2026 Simon Glass <sjg@chromium.org>
 */

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include "lkc.h"

/* Bit in a phase mask for the primary phase; xpl_name[n] is bit n + 1 */
#define PHASE_PRIMARY	0
#define PHASE_BIT(n)	(1U << (n))

/* Check whether a word is the name of a phase in lower case, e.g. spl */
static bool phase_name_is(const char *phase, const char *word)
{
	for (; *phase && tolower(*phase) == *word; phase++, word++)
		;

	return !*phase && !*word;
}

/**
 * u_boot_phase_mask() - Look up a word used in a 'phases' line
 *
 * @name: Name of a phase in lower case, as listed in scripts/xpl_phases, or
 *	'all' for every phase
 * Return: mask of the phases which the word selects, 0 if it is not known
 */
static unsigned int u_boot_phase_mask(const char *name)
{
	int i;

	if (u_boot_read_phases("xpl_phases"))
		exit(1);
	if (!strcmp(name, "all"))
		return PHASE_BIT(num_phases + 1) - 1;
	if (phase_name_is(primary_name, name))
		return PHASE_BIT(PHASE_PRIMARY);
	for (i = 0; i < num_phases; i++) {
		if (phase_name_is(xpl_name[i], name))
			return PHASE_BIT(i + 1);
	}

	return 0;
}

/* Add a phase to the entry being parsed, or leave it out if @negate */
void menu_add_phase(const char *name, bool negate)
{
	unsigned int mask;

	if (!current_entry->sym || sym_is_choice(current_entry->sym)) {
		zconf_error("'phases' is only supported in a config entry");
		return;
	}
	mask = u_boot_phase_mask(name);
	if (!mask) {
		zconf_error("unknown phase '%s'", name);
		return;
	}
	if (!negate) {
		current_entry->phases |= mask;
		return;
	}

	/* A list which starts by leaving out a phase begins with all of them */
	if (!current_entry->phases)
		current_entry->phases = u_boot_phase_mask("all");
	current_entry->phases &= ~mask;
	if (!current_entry->phases)
		zconf_error("'phases' leaves the option with no phases");
}
