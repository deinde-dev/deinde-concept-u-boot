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
 * SPL_DM_SERIAL and TPL_DM_SERIAL, each with its own value. Symbols which the
 * entry refers to are those of the phase where they exist, so SPL_DM_SERIAL
 * depends on SPL_DM
 *
 * The entries for each phase other than the primary one go in a menu of
 * their own, which mirrors the main menu, so that enclosing menus and 'if'
 * blocks work in the same way.
 *
 * Copyright 2026 Simon Glass <sjg@chromium.org>
 */

#include <ctype.h>
#include <stdarg.h>
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

/* Entry which is being translated, for use in messages */
static struct menu *phase_cur;

/* Report an error in an entry; conf stops once the phases are expanded */
static void phase_error(const struct menu *menu, const char *fmt, ...)
{
	va_list ap;

	fprintf(stderr, "%s:%d:error: ", menu->file->name, menu->lineno);
	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
	fputc('\n', stderr);
	yynerrs++;
}

/* Get the name of a phase, e.g. SPL, from its bit number */
static const char *phase_name(int phase)
{
	return phase == PHASE_PRIMARY ? primary_name : xpl_name[phase - 1];
}

/**
 * phase_xlate_sym() - Find the symbol which an entry means in a given phase
 *
 * A phased entry refers to other options by their base name. For each phase,
 * this means the symbol for that phase, if the option has one. An option with
 * a single symbol is the same in every phase. If it has a twin which is
 * written out by hand, that is used, and if only some other phase has a twin,
 * the option is off, since it is not passed to this phase.
 *
 * @sym: Symbol mentioned in the entry
 * @phase: Bit number of the phase to translate for
 * Return: symbol to use, which is 'n' if the option is not in that phase
 */
static struct symbol *phase_xlate_sym(struct symbol *sym, int phase)
{
	struct symbol *twin;
	int i;

	if (!sym || !sym->name || (sym->flags & (SYMBOL_CONST | SYMBOL_CHOICE)))
		return sym;

	if (sym->phases) {
		if (!(sym->phases & PHASE_BIT(phase)))
			return &symbol_no;
		if (phase == PHASE_PRIMARY)
			return sym;

		return phase_instance(sym, phase, true);
	}
	if (phase == PHASE_PRIMARY)
		return sym;

	/* Use a twin which is written out by hand, if there is one */
	twin = phase_instance(sym, phase, false);
	if (twin && phase_sym_defined(twin))
		return twin;

	/* This includes a twin for the host tools, which follow the phases */
	for (i = 1; i <= num_phases + 1; i++) {
		twin = phase_instance(sym, i, false);
		if (twin && phase_sym_defined(twin))
			return &symbol_no;
	}

	return sym;
}

/**
 * phase_xlate_value() - Translate a symbol whose value is needed
 *
 * In a comparison, a range or the default of an option which is not a bool,
 * 'n' makes no sense, so the option must be in the phase.
 *
 * @sym: Symbol mentioned in the entry
 * @phase: Bit number of the phase to translate for
 * Return: symbol to use
 */
static struct symbol *phase_xlate_value(struct symbol *sym, int phase)
{
	struct symbol *xlate = phase_xlate_sym(sym, phase);

	if (xlate == &symbol_no && sym != &symbol_no)
		phase_error(phase_cur,
			    "'%s' is not in the %s phase, so its value cannot be used",
			    sym->name, phase_name(phase));

	return xlate;
}

/* Make a copy of an expression, as it applies in a given phase */
static struct expr *phase_xlate_expr(const struct expr *org, int phase)
{
	struct expr *e;

	if (!org)
		return NULL;

	e = xmalloc(sizeof(*e));
	*e = *org;
	switch (org->type) {
	case E_SYMBOL:
		e->left.sym = phase_xlate_sym(org->left.sym, phase);
		break;
	case E_NOT:
		e->left.expr = phase_xlate_expr(org->left.expr, phase);
		break;
	case E_AND:
	case E_OR:
		e->left.expr = phase_xlate_expr(org->left.expr, phase);
		e->right.expr = phase_xlate_expr(org->right.expr, phase);
		break;
	case E_EQUAL:
	case E_UNEQUAL:
	case E_LTH:
	case E_LEQ:
	case E_GTH:
	case E_GEQ:
	case E_RANGE:
		e->left.sym = phase_xlate_value(org->left.sym, phase);
		e->right.sym = phase_xlate_value(org->right.sym, phase);
		break;
	default:
		free(e);
		e = expr_copy(org);
		break;
	}

	return e;
}

/**
 * phase_copy_prop() - Copy a property to a symbol, translating it for a phase
 *
 * @prop: Property to copy
 * @sym: Symbol to add the copy to, NULL if it is for a menu
 * @menu: Menu entry which the copy belongs to
 * @phase: Bit number of the phase to translate for
 * Return: new property, or NULL if it selects something which is not in
 *	this phase
 */
static struct property *phase_copy_prop(const struct property *prop,
					struct symbol *sym, struct menu *menu,
					int phase)
{
	struct property *copy;
	struct expr *expr;

	if (prop->type == P_SYMBOL) {
		expr = expr_alloc_symbol(sym);
	} else if (prop->type == P_DEFAULT && sym && sym->type != S_BOOLEAN &&
		   sym->type != S_TRISTATE && prop->expr->type == E_SYMBOL) {
		expr = expr_alloc_symbol(phase_xlate_value(prop->expr->left.sym,
							   phase));
	} else {
		expr = phase_xlate_expr(prop->expr, phase);
		if ((prop->type == P_SELECT || prop->type == P_IMPLY) &&
		    expr->left.sym == &symbol_no) {
			expr_free(expr);
			return NULL;
		}
	}

	copy = prop_alloc(prop->type, sym);
	copy->text = prop->text;
	copy->expr = expr;
	copy->visible.expr = phase_xlate_expr(prop->visible.expr, phase);
	copy->menu = menu;
	copy->file = prop->file;
	copy->lineno = prop->lineno;

	return copy;
}

static struct menu *phase_alloc_menu(struct menu *org, int phase)
{
	struct menu *menu;

	phase_cur = org;
	menu = xmalloc(sizeof(*menu));
	memset(menu, 0, sizeof(*menu));
	menu->file = org->file;
	menu->lineno = org->lineno;
	menu->help = org->help;
	menu->dep = phase_xlate_expr(org->dep, phase);
	menu->visibility = phase_xlate_expr(org->visibility, phase);

	return menu;
}

/*
 * Create the entry for one phase's symbol, from a phased entry. For the
 * primary phase the symbol is that of the entry itself
 */
static struct menu *phase_clone_entry(struct menu *org, int phase)
{
	struct symbol *base = org->sym;
	struct property *prop, *copy;
	struct symbol *sym;
	struct menu *menu;

	if (phase == PHASE_PRIMARY) {
		sym = base;
	} else {
		sym = phase_instance(base, phase, true);
		sym->flags |= SYMBOL_OPTIONAL;
		if (sym->type == S_UNKNOWN)
			sym->type = base->type;
		else if (base->type != S_UNKNOWN && sym->type != base->type)
			menu_warn(org, "type of '%s' differs from '%s'",
				  sym->name, base->name);
	}

	menu = phase_alloc_menu(org, phase);
	menu->sym = sym;
	for (prop = base->prop; prop; prop = prop->next) {
		if (prop->menu != org)
			continue;
		copy = phase_copy_prop(prop, sym, menu, phase);
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

	menu = phase_alloc_menu(org, phase);
	menu->list = phase_clone_list(org->list, menu, phase);
	if (!menu->list) {
		expr_free(menu->dep);
		expr_free(menu->visibility);
		free(menu);
		return NULL;
	}
	if (org->prompt) {
		phase_cur = org;
		menu->prompt = phase_copy_prop(org->prompt, NULL, menu, phase);
	}

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
 * phase_fix_primary() - Update the phased entries for the primary phase
 *
 * Replace each entry with a copy which is translated for the primary phase,
 * since it may mention options which are not in that phase. Remove an entry
 * which does not include the primary phase.
 *
 * @listp: Pointer to the first entry in the list to update
 */
static void phase_fix_primary(struct menu **listp)
{
	struct menu *org, *menu;

	while (*listp) {
		org = *listp;
		if (org->list)
			phase_fix_primary(&org->list);
		if (!org->sym || !org->phases) {
			listp = &org->next;
			continue;
		}
		if (!(org->phases & PHASE_BIT(PHASE_PRIMARY))) {
			*listp = org->next;
			continue;
		}
		menu = phase_clone_entry(org, PHASE_PRIMARY);
		menu->parent = org->parent;
		menu->next = org->next;
		*listp = menu;
		listp = &menu->next;
	}
}

/* Remove the properties of the phased entries, which are all copied by now */
static void phase_drop_props(struct symbol *sym)
{
	struct property **propp = &sym->prop;

	while (*propp) {
		if ((*propp)->menu && (*propp)->menu->phases)
			*propp = (*propp)->next;
		else
			propp = &(*propp)->next;
	}
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

	phase_fix_primary(&rootmenu.list);

	for (tailp = &rootmenu.list; *tailp; tailp = &(*tailp)->next)
		;
	for (i = 0; i < num_phases; i++) {
		if (mirror[i]) {
			*tailp = mirror[i];
			tailp = &mirror[i]->next;
		}
	}

	for_all_symbols(i, sym) {
		if (!sym->phases)
			continue;
		phase_drop_props(sym);

		/* An option not in the primary phase has no symbol there */
		if (!(sym->phases & PHASE_BIT(PHASE_PRIMARY)))
			sym->type = S_UNKNOWN;
	}
}
