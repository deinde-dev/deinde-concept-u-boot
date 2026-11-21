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
 * This creates the symbol DM_SERIAL for the primary phase ('ppl') along with
 * SPL_DM_SERIAL and TPL_DM_SERIAL, each with its own value.
 *
 * The entries for each phase other than the primary one go in a menu of
 * their own, which mirrors the main menu, so that enclosing menus and 'if'
 * blocks work in the same way.
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

/* Check whether a symbol is defined by a config entry, not just mentioned */
static bool phase_sym_defined(const struct symbol *sym)
{
	struct property *prop;

	for (prop = sym->prop; prop; prop = prop->next) {
		if (prop->type == P_SYMBOL)
			return true;
	}

	return false;
}

/* Get the symbol for a phase's version of an option, e.g. SPL_FOO */
static struct symbol *phase_instance(const struct symbol *sym, int phase,
				     bool create)
{
	char name[SYMBOL_MAXLENGTH];

	snprintf(name, sizeof(name), "%s_%s", xpl_name[phase - 1], sym->name);

	return create ? sym_lookup(name, 0) : sym_find(name);
}

/**
 * phase_copy_prop() - Copy a property to the symbol for a phase
 *
 * @prop: Property to copy
 * @sym: Symbol to add the copy to, NULL if it is for a menu
 * @menu: Menu entry which the copy belongs to
 * Return: new property
 */
static struct property *phase_copy_prop(const struct property *prop,
					struct symbol *sym, struct menu *menu)
{
	struct property *copy;

	copy = prop_alloc(prop->type, sym);
	copy->text = prop->text;
	if (prop->type == P_SYMBOL)
		copy->expr = expr_alloc_symbol(sym);
	else
		copy->expr = expr_copy(prop->expr);
	copy->visible.expr = expr_copy(prop->visible.expr);
	copy->menu = menu;
	copy->file = prop->file;
	copy->lineno = prop->lineno;

	return copy;
}

static struct menu *phase_alloc_menu(const struct menu *org)
{
	struct menu *menu;

	menu = xmalloc(sizeof(*menu));
	memset(menu, 0, sizeof(*menu));
	menu->file = org->file;
	menu->lineno = org->lineno;
	menu->help = org->help;
	menu->dep = expr_copy(org->dep);
	menu->visibility = expr_copy(org->visibility);

	return menu;
}

/* Create the entry for one phase's symbol, from a phased entry */
static struct menu *phase_clone_entry(struct menu *org, int phase)
{
	struct symbol *base = org->sym;
	struct property *prop, *copy;
	struct symbol *sym;
	struct menu *menu;

	sym = phase_instance(base, phase, true);
	sym->flags |= SYMBOL_OPTIONAL;
	if (sym->type == S_UNKNOWN)
		sym->type = base->type;
	else if (base->type != S_UNKNOWN && sym->type != base->type)
		menu_warn(org, "type of '%s' differs from '%s'", sym->name,
			  base->name);

	menu = phase_alloc_menu(org);
	menu->sym = sym;
	for (prop = base->prop; prop; prop = prop->next) {
		if (prop->menu != org)
			continue;
		copy = phase_copy_prop(prop, sym, menu);
		if (prop == org->prompt)
			menu->prompt = copy;
	}

	return menu;
}

static struct menu *phase_clone_list(struct menu *list, struct menu *parent,
				     int phase);

/* Copy a menu or 'if' block for a phase, if it holds any phased entries */
static struct menu *phase_clone_menu(struct menu *org, int phase)
{
	struct menu *menu;

	menu = phase_alloc_menu(org);
	menu->list = phase_clone_list(org->list, menu, phase);
	if (!menu->list) {
		expr_free(menu->dep);
		expr_free(menu->visibility);
		free(menu);
		return NULL;
	}
	if (org->prompt)
		menu->prompt = phase_copy_prop(org->prompt, NULL, menu);

	return menu;
}

/**
 * phase_clone_list() - Copy the parts of a menu which are in a phase
 *
 * @list: First entry in the list to copy
 * @parent: Parent for the new entries
 * @phase: Bit number of the phase to copy for
 * Return: first entry in the new list, NULL if there is nothing in the phase
 */
static struct menu *phase_clone_list(struct menu *list, struct menu *parent,
				     int phase)
{
	struct menu *head = NULL, **tailp = &head;
	struct menu *org, *menu;

	for (org = list; org; org = org->next) {
		menu = NULL;
		if (org->sym) {
			if (org->phases & PHASE_BIT(phase))
				menu = phase_clone_entry(org, phase);
		} else if (org->phases & PHASE_BIT(phase)) {
			menu = phase_clone_menu(org, phase);
		}
		if (menu) {
			menu->parent = parent;
			*tailp = menu;
			tailp = &menu->next;
		}
	}

	return head;
}

/**
 * phase_collect() - Collect the phases of each symbol
 *
 * This also records the phases of the entries inside each menu and 'if'
 * block, so that one with nothing for a phase can be skipped.
 *
 * @list: First entry in the list to check
 * Return: mask of the phases of the entries in the list
 */
static unsigned int phase_collect(struct menu *list)
{
	unsigned int mask = 0, inside;
	struct menu *menu;

	for (menu = list; menu; menu = menu->next) {
		if (menu->sym) {
			menu->sym->phases |= menu->phases;
			mask |= menu->phases;
		}
		if (menu->list) {
			inside = phase_collect(menu->list);
			if (!menu->sym)
				menu->phases = inside;
			mask |= inside;
		}
	}

	return mask;
}

/**
 * u_boot_expand_phases() - Create the symbols and menus for phased entries
 *
 * This is called once the Kconfig files are parsed, before the menu tree is
 * finalised. For each phase other than the primary one it adds a menu at the
 * end of the main menu, holding that phase's version of each phased entry.
 */
void u_boot_expand_phases(void)
{
	struct menu *mirror[MAX_XPLS] = { NULL };
	struct menu *menu, **tailp;
	struct property *prop;
	struct symbol *sym;
	char *text;
	int i;

	if (!phase_collect(rootmenu.list))
		return;

	for (i = 0; i < num_phases; i++) {
		menu = xmalloc(sizeof(*menu));
		memset(menu, 0, sizeof(*menu));
		menu->list = phase_clone_list(rootmenu.list, menu, i + 1);
		if (!menu->list) {
			free(menu);
			continue;
		}
		menu->parent = &rootmenu;
		menu->file = rootmenu.list->file;
		menu->lineno = rootmenu.list->lineno;

		/* The options of a phase depend on the phase being enabled */
		sym = sym_find(xpl_name[i]);
		if (sym && phase_sym_defined(sym))
			menu->dep = expr_alloc_symbol(sym);

		text = xmalloc(strlen(xpl_name[i]) + 20);
		sprintf(text, "%s phase options", xpl_name[i]);
		prop = prop_alloc(P_MENU, NULL);
		prop->text = text;
		prop->menu = menu;
		menu->prompt = prop;
		mirror[i] = menu;
	}

	for (tailp = &rootmenu.list; *tailp; tailp = &(*tailp)->next)
		;
	for (i = 0; i < num_phases; i++) {
		if (mirror[i]) {
			*tailp = mirror[i];
			tailp = &mirror[i]->next;
		}
	}
}
