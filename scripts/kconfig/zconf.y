%{
/*
 * Copyright (C) 2002 Roman Zippel <zippel@linux-m68k.org>
 * Released under the terms of the GNU GPL v2.0.
 */

#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "lkc.h"

#define printd(mask, fmt...) if (cdebug & (mask)) printf(fmt)

#define PRINTD		0x0001
#define DEBUG_PARSE	0x0002

int cdebug = PRINTD;

int yylex(void);
static void yyerror(const char *err);
static void zconfprint(const char *err, ...);
static void zconf_error(const char *err, ...);
static bool zconf_endtoken(const struct kconf_id *id, int starttoken, int endtoken);

struct symbol *symbol_hash[SYMBOL_HASHSIZE];

static struct menu *current_menu, *current_entry;

%}
%expect 30

%union
{
	char *string;
	struct file *file;
	struct symbol *symbol;
	struct expr *expr;
	struct menu *menu;
	const struct kconf_id *id;
	enum variable_flavor flavor;
}

%token <id>T_MAINMENU
%token <id>T_MENU
%token <id>T_ENDMENU
%token <id>T_SOURCE
%token <id>T_CHOICE
%token <id>T_ENDCHOICE
%token <id>T_COMMENT
%token <id>T_CONFIG
%token <id>T_MENUCONFIG
%token <id>T_HELP
%token <string> T_HELPTEXT
%token <id>T_IF
%token <id>T_ENDIF
%token <id>T_DEPENDS
%token <id>T_OPTIONAL
%token <id>T_PROMPT
%token <id>T_TYPE
%token <id>T_DEFAULT
%token <id>T_SELECT
%token <id>T_IMPLY
%token <id>T_PHASES
%token <id>T_RANGE
%token <id>T_VISIBLE
%token <id>T_OPTION
%token <id>T_ON
%token <string> T_WORD
%token <string> T_WORD_QUOTE
%token T_UNEQUAL
%token T_LESS
%token T_LESS_EQUAL
%token T_GREATER
%token T_GREATER_EQUAL
%token T_CLOSE_PAREN
%token T_OPEN_PAREN
%token T_EOL
%token <string> T_VARIABLE
%token <flavor> T_ASSIGN
%token <string> T_ASSIGN_VAL

%left T_OR
%left T_AND
%left T_EQUAL T_UNEQUAL
%left T_LESS T_LESS_EQUAL T_GREATER T_GREATER_EQUAL
%nonassoc T_NOT

%type <string> prompt
%type <symbol> nonconst_symbol
%type <symbol> symbol
%type <expr> expr
%type <expr> if_expr
%type <id> end
%type <id> option_name
%type <menu> if_entry menu_entry choice_entry
%type <string> symbol_option_arg word_opt assign_val

%destructor {
	fprintf(stderr, "%s:%d: missing end statement for this entry\n",
		$$->file->name, $$->lineno);
	if (current_menu == $$)
		menu_end_menu();
} if_entry menu_entry choice_entry

%{
/* Include kconf_id.c here so it can see the token constants. */
#include "kconf_id.c"
%}

%%
input: nl start | start;

start: mainmenu_stmt stmt_list | stmt_list;

/* mainmenu entry */

mainmenu_stmt: T_MAINMENU prompt T_EOL
{
	menu_add_prompt(P_MENU, $2, NULL);
};

stmt_list:
	  /* empty */
	| stmt_list common_stmt
	| stmt_list choice_stmt
	| stmt_list menu_stmt
	| stmt_list end			{ zconf_error("unexpected end statement"); }
	| stmt_list T_WORD error T_EOL	{ zconf_error("unknown statement \"%s\"", $2); }
	| stmt_list option_name error T_EOL
{
	zconf_error("unexpected option \"%s\"", $2->name);
}
	| stmt_list error T_EOL		{ zconf_error("invalid statement"); }
;

option_name:
	T_DEPENDS | T_PROMPT | T_TYPE | T_SELECT | T_IMPLY | T_OPTIONAL | T_RANGE | T_DEFAULT | T_VISIBLE
;

common_stmt:
	  T_EOL
	| if_stmt
	| comment_stmt
	| config_stmt
	| menuconfig_stmt
	| source_stmt
	| assignment_stmt
;

option_error:
	  T_WORD error T_EOL		{ zconf_error("unknown option \"%s\"", $1); }
	| error T_EOL			{ zconf_error("invalid option"); }
;


/* config/menuconfig entry */

config_entry_start: T_CONFIG nonconst_symbol T_EOL
{
	$2->flags |= SYMBOL_OPTIONAL;
	menu_add_entry($2);
	printd(DEBUG_PARSE, "%s:%d:config %s\n", zconf_curname(), zconf_lineno(), $2->name);
};

config_stmt: config_entry_start config_option_list
{
	printd(DEBUG_PARSE, "%s:%d:endconfig\n", zconf_curname(), zconf_lineno());
};

menuconfig_entry_start: T_MENUCONFIG nonconst_symbol T_EOL
{
	$2->flags |= SYMBOL_OPTIONAL;
	menu_add_entry($2);
	printd(DEBUG_PARSE, "%s:%d:menuconfig %s\n", zconf_curname(), zconf_lineno(), $2->name);
};

menuconfig_stmt: menuconfig_entry_start config_option_list
{
	if (current_entry->prompt)
		current_entry->prompt->type = P_MENU;
	else
		zconfprint("warning: menuconfig statement without prompt");
	printd(DEBUG_PARSE, "%s:%d:endconfig\n", zconf_curname(), zconf_lineno());
};

config_option_list:
	  /* empty */
	| config_option_list config_option
	| config_option_list symbol_option
	| config_option_list depends
	| config_option_list help
	| config_option_list option_error
	| config_option_list T_EOL
;

config_option: T_TYPE prompt_stmt_opt T_EOL
{
	menu_set_type($1->stype);
	printd(DEBUG_PARSE, "%s:%d:type(%u)\n",
		zconf_curname(), zconf_lineno(),
		$1->stype);
};

config_option: T_PROMPT prompt if_expr T_EOL
{
	menu_add_prompt(P_PROMPT, $2, $3);
	printd(DEBUG_PARSE, "%s:%d:prompt\n", zconf_curname(), zconf_lineno());
};

config_option: T_DEFAULT expr if_expr T_EOL
{
	menu_add_expr(P_DEFAULT, $2, $3);
	if ($1->stype != S_UNKNOWN)
		menu_set_type($1->stype);
	printd(DEBUG_PARSE, "%s:%d:default(%u)\n",
		zconf_curname(), zconf_lineno(),
		$1->stype);
};

config_option: T_SELECT nonconst_symbol if_expr T_EOL
{
	menu_add_symbol(P_SELECT, $2, $3);
	printd(DEBUG_PARSE, "%s:%d:select\n", zconf_curname(), zconf_lineno());
};

config_option: T_IMPLY nonconst_symbol if_expr T_EOL
{
	menu_add_symbol(P_IMPLY, $2, $3);
	printd(DEBUG_PARSE, "%s:%d:imply\n", zconf_curname(), zconf_lineno());
};

/* U-Boot: phases in which the option exists, each with its own symbol */
config_option: T_PHASES phase_list T_EOL
{
	printd(DEBUG_PARSE, "%s:%d:phases\n", zconf_curname(), zconf_lineno());
};

phase_list:
	  phase
	| phase_list phase
;

phase:
	  T_WORD		{ menu_add_phase($1, false); free($1); }
	| T_NOT T_WORD		{ menu_add_phase($2, true); free($2); }
;

config_option: T_RANGE symbol symbol if_expr T_EOL
{
	menu_add_expr(P_RANGE, expr_alloc_comp(E_RANGE,$2, $3), $4);
	printd(DEBUG_PARSE, "%s:%d:range\n", zconf_curname(), zconf_lineno());
};

symbol_option: T_OPTION symbol_option_list T_EOL
;

symbol_option_list:
	  /* empty */
	| symbol_option_list T_WORD symbol_option_arg
{
	const struct kconf_id *id = kconf_id_lookup($2, strlen($2));
	if (id && id->flags & TF_OPTION) {
		menu_add_option(id->token, $3);
		free($3);
	}
	else
		zconfprint("warning: ignoring unknown option %s", $2);
	free($2);
};

symbol_option_arg:
	  /* empty */		{ $$ = NULL; }
	| T_EQUAL prompt	{ $$ = $2; }
;

/* choice entry */

choice: T_CHOICE word_opt T_EOL
{
	struct symbol *sym = sym_lookup($2, SYMBOL_CHOICE);
	sym->flags |= SYMBOL_NO_WRITE;
	menu_add_entry(sym);
	menu_add_expr(P_CHOICE, NULL, NULL);
	free($2);
	printd(DEBUG_PARSE, "%s:%d:choice\n", zconf_curname(), zconf_lineno());
};

choice_entry: choice choice_option_list
{
	$$ = menu_add_menu();
};

choice_end: end
{
	if (zconf_endtoken($1, T_CHOICE, T_ENDCHOICE)) {
		menu_end_menu();
		printd(DEBUG_PARSE, "%s:%d:endchoice\n", zconf_curname(), zconf_lineno());
	}
};

choice_stmt: choice_entry choice_block choice_end
;

choice_option_list:
	  /* empty */
	| choice_option_list choice_option
	| choice_option_list depends
	| choice_option_list help
	| choice_option_list T_EOL
	| choice_option_list option_error
;

choice_option: T_PROMPT prompt if_expr T_EOL
{
	menu_add_prompt(P_PROMPT, $2, $3);
	printd(DEBUG_PARSE, "%s:%d:prompt\n", zconf_curname(), zconf_lineno());
};

choice_option: T_TYPE prompt_stmt_opt T_EOL
{
	if ($1->stype == S_BOOLEAN || $1->stype == S_TRISTATE) {
		menu_set_type($1->stype);
		printd(DEBUG_PARSE, "%s:%d:type(%u)\n",
			zconf_curname(), zconf_lineno(),
			$1->stype);
	} else
		YYERROR;
};

choice_option: T_OPTIONAL T_EOL
{
	current_entry->sym->flags |= SYMBOL_OPTIONAL;
	printd(DEBUG_PARSE, "%s:%d:optional\n", zconf_curname(), zconf_lineno());
};

choice_option: T_DEFAULT nonconst_symbol if_expr T_EOL
{
	if ($1->stype == S_UNKNOWN) {
		menu_add_symbol(P_DEFAULT, $2, $3);
		printd(DEBUG_PARSE, "%s:%d:default\n",
			zconf_curname(), zconf_lineno());
	} else
		YYERROR;
};

choice_block:
	  /* empty */
	| choice_block common_stmt
;

/* if entry */

if_entry: T_IF expr T_EOL
{
	printd(DEBUG_PARSE, "%s:%d:if\n", zconf_curname(), zconf_lineno());
	menu_add_entry(NULL);
	menu_add_dep($2);
	$$ = menu_add_menu();
};

if_end: end
{
	if (zconf_endtoken($1, T_IF, T_ENDIF)) {
		menu_end_menu();
		printd(DEBUG_PARSE, "%s:%d:endif\n", zconf_curname(), zconf_lineno());
	}
};

if_stmt: if_entry if_block if_end
;

if_block:
	  /* empty */
	| if_block common_stmt
	| if_block menu_stmt
	| if_block choice_stmt
;

/* menu entry */

menu: T_MENU prompt T_EOL
{
	menu_add_entry(NULL);
	menu_add_prompt(P_MENU, $2, NULL);
	printd(DEBUG_PARSE, "%s:%d:menu\n", zconf_curname(), zconf_lineno());
};

menu_entry: menu visibility_list depends_list
{
	$$ = menu_add_menu();
};

menu_end: end
{
	if (zconf_endtoken($1, T_MENU, T_ENDMENU)) {
		menu_end_menu();
		printd(DEBUG_PARSE, "%s:%d:endmenu\n", zconf_curname(), zconf_lineno());
	}
};

menu_stmt: menu_entry menu_block menu_end
;

menu_block:
	  /* empty */
	| menu_block common_stmt
	| menu_block menu_stmt
	| menu_block choice_stmt
;

source_stmt: T_SOURCE prompt T_EOL
{
	printd(DEBUG_PARSE, "%s:%d:source %s\n", zconf_curname(), zconf_lineno(), $2);
	zconf_nextfile($2);
	free($2);
};

/* comment entry */

comment: T_COMMENT prompt T_EOL
{
	menu_add_entry(NULL);
	menu_add_prompt(P_COMMENT, $2, NULL);
	printd(DEBUG_PARSE, "%s:%d:comment\n", zconf_curname(), zconf_lineno());
};

comment_stmt: comment depends_list
;

/* help option */

help_start: T_HELP T_EOL
{
	printd(DEBUG_PARSE, "%s:%d:help\n", zconf_curname(), zconf_lineno());
	zconf_starthelp();
};

help: help_start T_HELPTEXT
{
	if (current_entry->help) {
		free(current_entry->help);
		zconfprint("warning: '%s' defined with more than one help text -- only the last one will be used",
			   current_entry->sym->name ?: "<choice>");
	}

	/* Is the help text empty or all whitespace? */
	if ($2[strspn($2, " \f\n\r\t\v")] == '\0')
		zconfprint("warning: '%s' defined with blank help text",
			   current_entry->sym->name ?: "<choice>");

	current_entry->help = $2;
};

/* depends option */

depends_list:
	  /* empty */
	| depends_list depends
	| depends_list T_EOL
	| depends_list option_error
;

depends: T_DEPENDS T_ON expr T_EOL
{
	menu_add_dep($3);
	printd(DEBUG_PARSE, "%s:%d:depends on\n", zconf_curname(), zconf_lineno());
};

/* visibility option */

visibility_list:
	  /* empty */
	| visibility_list visible
	| visibility_list T_EOL
;

visible: T_VISIBLE if_expr
{
	menu_add_visibility($2);
};

/* prompt statement */

prompt_stmt_opt:
	  /* empty */
	| prompt if_expr
{
	menu_add_prompt(P_PROMPT, $1, $2);
};

prompt:	  T_WORD
	| T_WORD_QUOTE
;

end:	  T_ENDMENU T_EOL	{ $$ = $1; }
	| T_ENDCHOICE T_EOL	{ $$ = $1; }
	| T_ENDIF T_EOL		{ $$ = $1; }
;

nl:
	  T_EOL
	| nl T_EOL
;

if_expr:  /* empty */			{ $$ = NULL; }
	| T_IF expr			{ $$ = $2; }
;

expr:	  symbol				{ $$ = expr_alloc_symbol($1); }
	| symbol T_LESS symbol			{ $$ = expr_alloc_comp(E_LTH, $1, $3); }
	| symbol T_LESS_EQUAL symbol		{ $$ = expr_alloc_comp(E_LEQ, $1, $3); }
	| symbol T_GREATER symbol		{ $$ = expr_alloc_comp(E_GTH, $1, $3); }
	| symbol T_GREATER_EQUAL symbol		{ $$ = expr_alloc_comp(E_GEQ, $1, $3); }
	| symbol T_EQUAL symbol			{ $$ = expr_alloc_comp(E_EQUAL, $1, $3); }
	| symbol T_UNEQUAL symbol		{ $$ = expr_alloc_comp(E_UNEQUAL, $1, $3); }
	| T_OPEN_PAREN expr T_CLOSE_PAREN	{ $$ = $2; }
	| T_NOT expr				{ $$ = expr_alloc_one(E_NOT, $2); }
	| expr T_OR expr			{ $$ = expr_alloc_two(E_OR, $1, $3); }
	| expr T_AND expr			{ $$ = expr_alloc_two(E_AND, $1, $3); }
;

/* For symbol definitions, selects, etc., where quotes are not accepted */
nonconst_symbol: T_WORD { $$ = sym_lookup($1, 0); free($1); };

symbol:	  nonconst_symbol
	| T_WORD_QUOTE	{ $$ = sym_lookup($1, SYMBOL_CONST); free($1); }
;

word_opt: /* empty */			{ $$ = NULL; }
	| T_WORD

/* assignment statement */

assignment_stmt:  T_VARIABLE T_ASSIGN assign_val T_EOL	{ variable_add($1, $3, $2); free($1); free($3); }

assign_val:
	/* empty */		{ $$ = xstrdup(""); };
	| T_ASSIGN_VAL
;

%%

void conf_parse(const char *name)
{
	struct symbol *sym;
	int i;

	zconf_initscan(name);

	_menu_init();

	if (getenv("ZCONF_DEBUG"))
		yydebug = 1;
	yyparse();

	/* Variables are expanded in the parse phase. We can free them here. */
	variable_all_del();

	if (yynerrs)
		exit(1);
	if (!modules_sym)
		modules_sym = sym_find( "n" );

	if (!menu_has_prompt(&rootmenu)) {
		current_entry = &rootmenu;
		menu_add_prompt(P_MENU, "Main menu", NULL);
	}

	u_boot_expand_phases();

	menu_finalize(&rootmenu);
	for_all_symbols(i, sym) {
		if (sym_check_deps(sym))
			yynerrs++;
	}
	if (yynerrs)
		exit(1);
	sym_set_change_count(1);
}

static const char *zconf_tokenname(int token)
{
	switch (token) {
	case T_MENU:		return "menu";
	case T_ENDMENU:		return "endmenu";
	case T_CHOICE:		return "choice";
	case T_ENDCHOICE:	return "endchoice";
	case T_IF:		return "if";
	case T_ENDIF:		return "endif";
	case T_DEPENDS:		return "depends";
	case T_PHASES:		return "phases";
	case T_VISIBLE:		return "visible";
	}
	return "<token>";
}

static bool zconf_endtoken(const struct kconf_id *id, int starttoken, int endtoken)
{
	if (id->token != endtoken) {
		zconf_error("unexpected '%s' within %s block",
			id->name, zconf_tokenname(starttoken));
		yynerrs++;
		return false;
	}
	if (current_menu->file != current_file) {
		zconf_error("'%s' in different file than '%s'",
			id->name, zconf_tokenname(starttoken));
		fprintf(stderr, "%s:%d: location of the '%s'\n",
			current_menu->file->name, current_menu->lineno,
			zconf_tokenname(starttoken));
		yynerrs++;
		return false;
	}
	return true;
}

static void zconfprint(const char *err, ...)
{
	va_list ap;

	fprintf(stderr, "%s:%d: ", zconf_curname(), zconf_lineno());
	va_start(ap, err);
	vfprintf(stderr, err, ap);
	va_end(ap);
	fprintf(stderr, "\n");
}

static void zconf_error(const char *err, ...)
{
	va_list ap;

	yynerrs++;
	fprintf(stderr, "%s:%d: ", zconf_curname(), zconf_lineno());
	va_start(ap, err);
	vfprintf(stderr, err, ap);
	va_end(ap);
	fprintf(stderr, "\n");
}

static void yyerror(const char *err)
{
	fprintf(stderr, "%s:%d: %s\n", zconf_curname(), zconf_lineno() + 1, err);
}

static void print_quoted_string(FILE *out, const char *str)
{
	const char *p;
	int len;

	putc('"', out);
	while ((p = strchr(str, '"'))) {
		len = p - str;
		if (len)
			fprintf(out, "%.*s", len, str);
		fputs("\\\"", out);
		str = p + 1;
	}
	fputs(str, out);
	putc('"', out);
}

static void dump_expr_helper(void *data, struct symbol *sym, const char *str)
{
	FILE *out = data;

	/* Show a string in quotes, as it is written in the Kconfig file */
	if (sym && (sym->flags & SYMBOL_CONST) && sym != &symbol_yes &&
	    sym != &symbol_mod && sym != &symbol_no)
		print_quoted_string(out, str);
	else
		fputs(str, out);
}

static void dump_expr(struct expr *e, FILE *out)
{
	expr_print(e, dump_expr_helper, out, E_NONE);
}

/* Print a condition, if there is one, and end the line */
static void print_cond(FILE *out, struct expr *cond)
{
	if (!expr_is_yes(cond)) {
		fputs(" if ", out);
		dump_expr(cond, out);
	}
	fputc('\n', out);
}

static void print_depends(FILE *out, struct expr *dep)
{
	if (!expr_is_yes(dep)) {
		fputs("\tdepends on ", out);
		dump_expr(dep, out);
		fputc('\n', out);
	}
}

static void print_help(FILE *out, const char *help)
{
	int len = strlen(help);

	while (len && help[len - 1] == '\n')
		len--;
	fputs("\thelp\n", out);
	while (len) {
		const char *eol = memchr(help, '\n', len);
		int linelen = eol ? eol - help : len;

		if (linelen)
			fprintf(out, "\t  %.*s", linelen, help);
		fputc('\n', out);
		if (!eol)
			break;
		help += linelen + 1;
		len -= linelen + 1;
	}
}

static void print_symbol(FILE *out, struct menu *menu)
{
	struct symbol *sym = menu->sym;
	struct property *prop;

	if (sym_is_choice(sym))
		fprintf(out, "\nchoice\n");
	else if (menu->prompt && menu->prompt->type == P_MENU)
		fprintf(out, "\nmenuconfig %s\n", sym->name);
	else
		fprintf(out, "\nconfig %s\n", sym->name);
	switch (sym->type) {
	case S_BOOLEAN:
		fputs("\tbool\n", out);
		break;
	case S_TRISTATE:
		fputs("\ttristate\n", out);
		break;
	case S_STRING:
		fputs("\tstring\n", out);
		break;
	case S_INT:
		fputs("\tint\n", out);
		break;
	case S_HEX:
		fputs("\thex\n", out);
		break;
	default:
		break;
	}
	if (sym_is_choice(sym) && sym_is_optional(sym))
		fputs("\toptional\n", out);
	print_depends(out, menu->dep);
	for (prop = sym->prop; prop; prop = prop->next) {
		if (prop->menu != menu)
			continue;
		switch (prop->type) {
		case P_PROMPT:
		case P_MENU:
			fputs("\tprompt ", out);
			print_quoted_string(out, prop->text);
			print_cond(out, prop->visible.expr);
			break;
		case P_DEFAULT:
			fputs("\tdefault ", out);
			dump_expr(prop->expr, out);
			print_cond(out, prop->visible.expr);
			break;
		case P_CHOICE:
		case P_SYMBOL:
			break;
		case P_SELECT:
			fputs("\tselect ", out);
			dump_expr(prop->expr, out);
			print_cond(out, prop->visible.expr);
			break;
		case P_IMPLY:
			fputs("\timply ", out);
			dump_expr(prop->expr, out);
			print_cond(out, prop->visible.expr);
			break;
		case P_RANGE:
			fprintf(out, "\trange %s %s", prop->expr->left.sym->name,
				prop->expr->right.sym->name);
			print_cond(out, prop->visible.expr);
			break;
		default:
			fprintf(out, "\tunknown prop %d!\n", prop->type);
			break;
		}
	}
	if (menu->help)
		print_help(out, menu->help);
}

void zconfdump(FILE *out)
{
	struct property *prop;
	struct symbol *sym;
	struct menu *menu;

	menu = rootmenu.list;
	while (menu) {
		if ((sym = menu->sym))
			print_symbol(out, menu);
		else if ((prop = menu->prompt)) {
			switch (prop->type) {
			case P_COMMENT:
				fputs("\ncomment ", out);
				print_quoted_string(out, prop->text);
				fputs("\n", out);
				break;
			case P_MENU:
				fputs("\nmenu ", out);
				print_quoted_string(out, prop->text);
				fputs("\n", out);
				break;
			default:
				;
			}
			if (!expr_is_yes(menu->visibility)) {
				fputs("\tvisible if ", out);
				dump_expr(menu->visibility, out);
				fputc('\n', out);
			}
			print_depends(out, prop->visible.expr);
		}

		if (menu->list)
			menu = menu->list;
		else if (menu->next)
			menu = menu->next;
		else while ((menu = menu->parent)) {
			if (menu->sym && sym_is_choice(menu->sym))
				fputs("\nendchoice\n", out);
			else if (!menu->sym && menu != &rootmenu &&
				 menu->prompt && menu->prompt->type == P_MENU)
				fputs("\nendmenu\n", out);
			if (menu->next) {
				menu = menu->next;
				break;
			}
		}
	}
}

#include "zconf.lex.c"
#include "util.c"
#include "confdata.c"
#include "expr.c"
#include "symbol.c"
#include "menu.c"
#include "phase.c"
#include "preprocess.c"
