# SPDX-License-Identifier: GPL-2.0
#
# Copyright 2026 Canonical Ltd
#
"""Find the boards which build a file, or particular lines of it

This answers 'which boards build this?' without building anything. The
Makefiles say which Kconfig options a file depends on, qconfig's database
says which boards have those options, and unifdef, run with each board's
configuration, says which of the file's lines survive the preprocessor.

It is quick enough to use for every file in a patch, but it is an estimate
rather than a record: a scan (see multiboard.py) is the authority, since it
looks at what a build actually compiled.
"""

from collections import namedtuple
import contextlib
import json
import os
import re
import subprocess
import sys
import tempfile

from u_boot_pylib import tout

# One build phase of a board
#
# name: name to show, e.g. 'SPL'
# prefix: what $(PHASE_) expands to in that phase, e.g. 'SPL_'
# option: option which turns the phase on, or None for U-Boot proper
# build: symbols the phase defines, e.g. CONFIG_SPL_BUILD
Phase = namedtuple('Phase', ['name', 'prefix', 'option', 'build'])

PHASES = (
    Phase('U-Boot', '', None, ()),
    Phase('SPL', 'SPL_', 'SPL', ('CONFIG_XPL_BUILD', 'CONFIG_SPL_BUILD')),
    Phase('TPL', 'TPL_', 'TPL', ('CONFIG_XPL_BUILD', 'CONFIG_TPL_BUILD')),
    Phase('VPL', 'VPL_', 'VPL', ('CONFIG_XPL_BUILD', 'CONFIG_VPL_BUILD')),
)

# A condition on a Kconfig option, from a Makefile rule or an 'ifdef' around it
#
# option: the option, without 'CONFIG_'
# relative: True if it says $(PHASE_), so that each phase has its own form of
#     the option, e.g. SPL_CLK in SPL
# negate: True if the option must be off (or differ from value)
# value: tuple of values one of which the option must have, or None for 'is
#     on'
Cond = namedtuple('Cond', ['option', 'relative', 'negate', 'value'])

# A string option which must have a particular value, from where a file sits
# in the tree, e.g. CONFIG_SYS_ARCH must be "arm" for arch/arm/
Need = namedtuple('Need', ['option', 'value'])

# The answer for one file
#
# path: file, relative to the source tree
# lines: (first, last) line range asked about, or None for the whole file
# boards: dict mapping each board to the list of phase names which build it
# note: why there is no answer, or None
Result = namedtuple('Result', ['path', 'lines', 'boards', 'note'])

# A Makefile rule such as 'obj-$(CONFIG_$(PHASE_)CLK) += clk-uclass.o', with
# the kind of rule, any phase macro, the option (if any) and the targets
RE_MAKE_RULE = re.compile(
    r'^\s*([a-z]+)-(?:\$\(CONFIG_(?:\$\((\w+)\))?(\w+)\)|y)\s*[:+]?=\s*(.*)$')

# The parts of a composite object, e.g. 'spinand-objs := core.o otp.o' or
# 'spi-nor-$(CONFIG_FOO) += extra.o', with the name, any phase macro, any
# option and the parts
RE_MAKE_COMPOSITE = re.compile(
    r'^([\w-]+?)-(?:objs|y|\$\(CONFIG_(?:\$\((\w+)\))?(\w+)\))'
    r'\s*[:+]?=\s*(.*)$')

# Rules which build things into the image, as opposed to composites and
# flags; anything else of the form 'name-y' is a composite object
MAKE_KINDS = ('obj', 'lib', 'extra', 'libs', 'head', 'machine', 'plat',
              'core', 'subdir', 'always')

# Make variables which the .config sets, and the option each comes from
MAKE_VARS = {'ARCH': 'SYS_ARCH', 'CPU': 'SYS_CPU', 'SOC': 'SYS_SOC',
             'BOARD': 'SYS_BOARD', 'VENDOR': 'SYS_VENDOR'}

# 'ifneq (,$(filter $(SOC), mx5 mx6))' and 'ifeq ($(SOC),mx6)'
RE_MAKE_FILTER = re.compile(
    r'^(ifn?eq)\s*\(\s*,\s*\$\(filter\s+\$\((\w+)\)\s*,\s*([^)]*)\)\s*\)$')
RE_MAKE_VAR_EQ = re.compile(
    r'^(ifn?eq)\s*\(\s*\$\((\w+)\)\s*,\s*([^)]*?)\s*\)$')

# Makefile conditionals on an option: 'ifdef CONFIG_FOO' and
# 'ifeq ($(CONFIG_$(PHASE_)FOO),y)'
RE_MAKE_IFDEF = re.compile(r'^(ifn?def)\s+CONFIG_(?:\$\((\w+)\))?(\w+)$')
RE_MAKE_IFEQ = re.compile(
    r'^(ifn?eq)\s*\(\s*\$\(CONFIG_(?:\$\((\w+)\))?(\w+)\)\s*,\s*([^)]*?)\s*\)$')
RE_MAKE_IF = re.compile(r'^if(n?def|n?eq)\b')

# Symbols which a phase defines rather than the .config
BUILD_SYMBOLS = ('CONFIG_XPL_BUILD', 'CONFIG_SPL_BUILD', 'CONFIG_TPL_BUILD',
                 'CONFIG_VPL_BUILD')

# The one phase macro still in use; others (XPL_, SPL_TPL_) expand to nothing
PHASE_MACRO = 'PHASE_'

# A line which starts a Kconfig entry, or ends one without starting another;
# these are in the first column, which keeps help text from matching
RE_KCONFIG_ENTRY = re.compile(
    r'^(?:(?:menu)?config\s+(\w+)|choice|endchoice|menu\b|endmenu|if\b|'
    r'endif|source|comment)')

# Mentions of an option in a source file, in any of the forms the
# preprocessor might test
RE_CONFIG = re.compile(r'\bCONFIG_(\w+)')

# CONFIG_IS_ENABLED(FOO) and CONFIG_VAL(FOO) with a single argument, which is
# all that appears in #if lines; IS_ENABLED(CONFIG_FOO) too
RE_IS_ENABLED = re.compile(r'\bCONFIG_IS_ENABLED\(\s*(\w+)\s*\)')
RE_VAL = re.compile(r'\bCONFIG_VAL\(\s*(\w+)\s*\)')
RE_PLAIN_IS_ENABLED = re.compile(r'\bIS_ENABLED\(\s*CONFIG_(\w+)\s*\)')

# Devicetree files, which a board reaches through CONFIG_DEFAULT_DEVICE_TREE
DT_SUFFIXES = ('.dts', '.dtsi')

# How many levels of devicetree includes to follow
DT_INCLUDE_DEPTH = 4

# Files which are run through unifdef to look at particular lines
UNIFDEF_SUFFIXES = ('.c', '.S', '.h')


def make_cond(line):
    """Work out the condition a Makefile 'if' line tests

    Args:
        line (str): The line, stripped, with any leading 'else ' removed

    Return:
        Cond: The condition, or None if it is not one this understands
    """
    match = RE_MAKE_IFDEF.match(line)
    if match:
        kind, macro, option = match.groups()
        return Cond(option, macro == PHASE_MACRO, kind == 'ifndef', None)
    match = RE_MAKE_IFEQ.match(line)
    if match:
        kind, macro, option, value = match.groups()
        relative = macro == PHASE_MACRO
        if value in ('', 'y'):
            # 'ifeq ($(CONFIG_FOO),)' tests that FOO is off
            return Cond(option, relative, (kind == 'ifeq') == (value == ''),
                        None)
        return Cond(option, relative, kind == 'ifneq', (value,))
    # $(SOC), $(CPU) and the like come from the .config, as SYS_SOC etc.
    match = RE_MAKE_FILTER.match(line)
    if match and match.group(2) in MAKE_VARS:
        kind, var, values = match.groups()
        return Cond(MAKE_VARS[var], False, kind == 'ifeq',
                    tuple(values.split()))
    match = RE_MAKE_VAR_EQ.match(line)
    if match and match.group(2) in MAKE_VARS:
        kind, var, value = match.groups()
        return Cond(MAKE_VARS[var], False, kind == 'ifneq', (value,))
    return None


def walk_makefile(lines):
    """Go through a Makefile, tracking the conditions on each line

    Each branch of an 'ifdef' chain holds when its own test does and every
    earlier one in the chain does not. A test this does not understand, such
    as 'ifeq ($(CC),clang)', is left out, which errs towards building.

    Args:
        lines (list of str): Lines of the Makefile, stripped and joined

    Yields:
        tuple:
            str: A line which is not a conditional
            list of Cond: Conditions which must hold for it to apply
    """
    # Each entry is the tests made so far in a chain and the conditions for
    # the branch being read
    stack = []
    for line in lines:
        if RE_MAKE_IF.match(line):
            cond = make_cond(line)
            stack.append(([cond], [cond] if cond else []))
        elif line == 'else' or line.startswith('else '):
            if not stack:
                continue
            tests, _ = stack[-1]
            conds = [test._replace(negate=not test.negate)
                     for test in tests if test]
            rest = line[len('else'):].strip()
            if rest:
                cond = make_cond(rest)
                tests.append(cond)
                if cond:
                    conds.append(cond)
            stack[-1] = (tests, conds)
        elif line == 'endif':
            if stack:
                stack.pop()
        elif line:
            yield line, [cond for _, conds in stack for cond in conds]


def makefile_rules(text, target):
    """Find the conditions under which a Makefile builds a target

    A target may be named by several rules, each inside its own 'ifdef'
    blocks, and any one of them is enough to build it.

    Args:
        text (str): Contents of the Makefile
        target (str): Object or directory, e.g. 'clk-uclass.o' or 'clk/'

    Return:
        list of tuple: One tuple of Cond for each way the target is built;
            empty if none is. An empty tuple means unconditionally
    """
    # Rules run on with a backslash, so join those lines first; '${FOO}' is
    # the same as '$(FOO)'
    text = re.sub(r'\$\{(\w+)\}', r'$(\1)', text.replace('\\\n', ' '))
    lines = [line.split('#', 1)[0].strip() for line in text.splitlines()]

    # A composite object, e.g. 'spinand-objs := core.o otp.o', is built when
    # a rule names 'spinand.o'; its parts may have conditions of their own
    names = {target: [()]}
    for line, conds in walk_makefile(lines):
        match = RE_MAKE_COMPOSITE.match(line)
        if (match and match.group(1) not in MAKE_KINDS and
                target in match.group(4).split()):
            own = conds[:]
            if match.group(3):
                own.append(Cond(match.group(3),
                                match.group(2) == PHASE_MACRO, False, None))
            names.setdefault(f'{match.group(1)}.o', []).append(tuple(own))

    alts = []
    for line, conds in walk_makefile(lines):
        match = RE_MAKE_RULE.match(line)
        if not match:
            continue
        kind, macro, option, rhs = match.groups()
        if kind not in MAKE_KINDS:
            continue
        for word in rhs.split():
            # arch Makefiles name a machine directory without 'mach-'
            if kind == 'machine':
                word = f'mach-{word}/'
            if word in names:
                rule = conds[:]
                if option:
                    rule.append(Cond(option, macro == PHASE_MACRO, False,
                                     None))
                alts += [tuple(rule) + part for part in names[word]]
                break
    return alts


def read_makefile(srcdir, dirname):
    """Read a directory's Makefile

    Args:
        srcdir (str): Source tree
        dirname (str): Directory, relative to the source tree ('' for the top)

    Return:
        str: Its contents, or '' if there is none
    """
    makefile = os.path.join(srcdir, dirname, 'Makefile')
    if not os.path.exists(makefile):
        return ''
    with open(makefile, encoding='utf-8', errors='replace') as inf:
        return inf.read()


def file_rules(srcdir, path):
    """Find the conditions under which a file is built

    The file, and each directory holding it, must be named by some Makefile
    above it: its parent's usually, but the top-level Makefile names
    directories by their full path ('libs-$(CONFIG_CMD_DOOM) += lib/doom/')
    and others name a path a few levels down ('obj-y += ddr/marvell/a38x/').

    Args:
        srcdir (str): Source tree
        path (str): File, relative to the source tree

    Return:
        list of list: One entry for the file and each directory which some
            Makefile names, innermost first, holding the alternatives from
            makefile_rules(); None if no Makefile builds the file itself, e.g.
            a .c file which another one includes
    """
    stem, ext = os.path.splitext(path)
    if ext not in ('.c', '.S'):
        return []
    # The directories above the file, innermost first, ending with the top
    dirs = []
    dirname = os.path.dirname(path)
    while dirname:
        dirs.append(dirname)
        dirname = os.path.dirname(dirname)
    dirs.append('')
    makefiles = {dirname: read_makefile(srcdir, dirname) for dirname in dirs}

    def named(target, above):
        """Collect the rules naming a target in the Makefiles above it"""
        alts = []
        for dirname in above:
            rel = target[len(dirname) + 1:] if dirname else target
            alts += makefile_rules(makefiles[dirname], rel)
        return alts

    levels = []
    alts = named(f'{stem}.o', dirs)
    if not alts:
        return None
    levels.append(alts)
    for i, dirname in enumerate(dirs[:-1]):
        # A directory which no Makefile names may be reached some other way,
        # e.g. from an arch Makefile, so says nothing
        alts = named(f'{dirname}/', dirs[i + 1:])
        if alts:
            levels.append(alts)
    return levels


def path_needs(srcdir, path):
    """Work out the string options a file's place in the tree implies

    A file under arch/arm/ is only built for an ARM board, one under
    board/acme/rocket/ only for that board, and so on. The .config records
    each of these as a string, which is more reliable than the Makefiles.

    Args:
        srcdir (str): Source tree
        path (str): File, relative to the source tree

    Return:
        list of Need: Options which must have the given values
    """
    parts = path.split('/')
    needs = []
    if parts[0] == 'arch' and len(parts) > 2:
        needs.append(Need('SYS_ARCH', parts[1]))
        if parts[2] == 'cpu' and len(parts) > 4:
            needs.append(Need('SYS_CPU', parts[3]))
    elif parts[0] == 'board' and len(parts) > 2:
        if len(parts) > 3 and os.path.isdir(os.path.join(srcdir, *parts[:3])):
            needs.append(Need('SYS_VENDOR', parts[1]))
            if parts[2] != 'common':
                needs.append(Need('SYS_BOARD', parts[2]))
        else:
            needs.append(Need('SYS_BOARD', parts[1]))
    return needs


def has_option(config, option):
    """Check whether a board's configuration turns an option on

    Args:
        config (dict): Maps 'CONFIG_xxx' to its value, as in qconfig.db
        option (str): Option without 'CONFIG_'

    Return:
        bool: True if the option is set to anything but 'n'
    """
    value = config.get(f'CONFIG_{option}')
    return value is not None and value != 'n'


def cond_holds(cond, config, phase):
    """Check a condition for one board and phase

    Args:
        cond (Cond): Condition to check
        config (dict): The board's configuration, from qconfig.db
        phase (Phase): Phase being built

    Return:
        bool: True if it holds
    """
    name = 'CONFIG_' + (phase.prefix if cond.relative else '') + cond.option
    if name in BUILD_SYMBOLS:
        value = 'y' if name in phase.build else None
    else:
        value = config.get(name)
        if value == 'n':
            value = None
    if cond.value is None:
        held = value is not None
    else:
        held = value is not None and value.strip('"') in [
            want.strip('"') for want in cond.value]
    return held != cond.negate


def build_phases(config, levels, needs):
    """Work out which phases of a board build a file

    Args:
        config (dict): The board's configuration, from qconfig.db
        levels (list of list): Result from file_rules()
        needs (list of Need): String options the file's location implies

    Return:
        list of Phase: Phases which build the file
    """
    for need in needs:
        if config.get(f'CONFIG_{need.option}') != f'"{need.value}"':
            return []
    out = []
    for phase in PHASES:
        if phase.option and not has_option(config, phase.option):
            continue
        if all(any(all(cond_holds(cond, config, phase) for cond in alt)
                   for alt in alts) for alts in levels):
            out.append(phase)
    return out


def phase_source(text, phase):
    """Rewrite the macros unifdef cannot expand, for one phase

    CONFIG_IS_ENABLED(FOO) means CONFIG_SPL_FOO in SPL and CONFIG_FOO in
    U-Boot proper, and so on. Each line keeps its length in lines, so line
    numbers still match the original.

    Args:
        text (str): Source file
        phase (Phase): Phase to rewrite for

    Return:
        str: Source with those macros replaced by plain CONFIG options
    """
    text = RE_IS_ENABLED.sub(
        lambda m: f'defined(CONFIG_{phase.prefix}{m.group(1)})', text)
    text = RE_VAL.sub(lambda m: f'CONFIG_{phase.prefix}{m.group(1)}', text)
    return RE_PLAIN_IS_ENABLED.sub(
        lambda m: f'defined(CONFIG_{m.group(1)})', text)


def directive_symbols(source):
    """List the options which preprocessor directives in a file test

    Only directives matter to unifdef, so options mentioned in C code are
    left out; otherwise every board would look different to every other and
    unifdef would run far more often than it needs to.

    Args:
        source (str): Source text, as rewritten by phase_source()

    Return:
        set of str: Options, with 'CONFIG_'
    """
    out = set()
    directive = False
    for line in source.splitlines():
        if line.lstrip().startswith('#'):
            directive = True
        if directive:
            out.update(f'CONFIG_{name}' for name in RE_CONFIG.findall(line))
            # A directive runs on to the next line after a backslash
            directive = line.rstrip().endswith('\\')
    return out


def unifdef_defs(config, symbols, phase):
    """Write the unifdef definitions for one board and phase

    Only the options the file mentions are written, each either defined to
    its value or explicitly undefined, so unifdef resolves every condition
    which depends on them.

    Args:
        config (dict): The board's configuration, from qconfig.db
        symbols (set of str): Options the file mentions, with 'CONFIG_'
        phase (Phase): Phase being built

    Return:
        str: Lines of '#define' and '#undef'
    """
    out = []
    for name in sorted(symbols):
        value = config.get(name)
        if name in phase.build:
            out.append(f'#define {name} 1')
        elif value is None or value == 'n':
            out.append(f'#undef {name}')
        elif value == 'y':
            out.append(f'#define {name} 1')
        else:
            out.append(f'#define {name} {value}')
    return '\n'.join(out) + '\n'


def active_lines(source, defs, unifdef='unifdef', is_asm=False):
    """Run unifdef and say which lines survive

    Args:
        source (str): Source text, as rewritten by phase_source()
        defs (str): Definitions, from unifdef_defs()
        unifdef (str): unifdef program to run
        is_asm (bool): True for an assembler file, which is not C

    Return:
        set of int: Line numbers, from 1, which are kept
    """
    with tempfile.TemporaryDirectory(prefix='codman-boards-') as tmp:
        src = os.path.join(tmp, 'src')
        cfg = os.path.join(tmp, 'defs')
        with open(src, 'w', encoding='utf-8') as outf:
            outf.write(source)
        with open(cfg, 'w', encoding='utf-8') as outf:
            outf.write(defs)
        cmd = [unifdef, '-n', '-f', cfg]
        if is_asm:
            cmd.append('-t')
        res = subprocess.run(cmd + [src], capture_output=True, text=True,
                             encoding='utf-8', errors='ignore', check=False)
    if res.returncode > 1 and 'Obfuscated' not in res.stderr:
        # unifdef could not follow the file, so take every line as kept
        # rather than say no board builds it
        return set(range(1, source.count('\n') + 2))
    kept = set()
    lineno = 1
    for line in res.stdout.splitlines():
        match = re.match(r'^#line (\d+)', line)
        if match:
            lineno = int(match.group(1))
        else:
            kept.add(lineno)
            lineno += 1
    return kept


# pylint: disable-next=too-many-arguments,too-many-locals
def line_boards(srcdir, path, lines, candidates, configs, unifdef='unifdef'):
    """Narrow the boards which build a file to those which build some lines

    Boards which agree on every option the file mentions get the same answer,
    so unifdef runs once per distinct combination rather than once per
    board.

    Args:
        srcdir (str): Source tree
        path (str): File, relative to the source tree
        lines (tuple): (first, last) lines to look at
        candidates (dict): Maps board to the list of Phase which build the file
        configs (dict): Maps board to its configuration, from qconfig.db
        unifdef (str): unifdef program to run

    Return:
        dict: Maps each board to the list of phase names which build at least
            one of the lines
    """
    with open(os.path.join(srcdir, path), encoding='utf-8',
              errors='ignore') as inf:
        text = inf.read()
    want = set(range(lines[0], lines[1] + 1))
    is_asm = path.endswith('.S')
    # The rewritten source and the options it tests depend only on the phase
    prepared = {}
    for phase in PHASES:
        source = phase_source(text, phase)
        prepared[phase] = (source, directive_symbols(source) |
                           set(phase.build))
    cache = {}
    out = {}
    for board, phases in sorted(candidates.items()):
        for phase in phases:
            source, symbols = prepared[phase]
            defs = unifdef_defs(configs[board], symbols, phase)
            key = (phase.name, defs)
            if key not in cache:
                cache[key] = bool(want & active_lines(source, defs, unifdef,
                                                      is_asm))
            if cache[key]:
                out.setdefault(board, []).append(phase.name)
    return out


def kconfig_owners(srcdir, path, lines):
    """Find the Kconfig entries which some lines of a Kconfig file sit in

    Args:
        srcdir (str): Source tree
        path (str): Kconfig file, relative to the source tree
        lines (tuple): (first, last) lines to look at, or None for all

    Return:
        set of str: Options, without 'CONFIG_'
    """
    with open(os.path.join(srcdir, path), encoding='utf-8',
              errors='ignore') as inf:
        text = inf.read().splitlines()
    owners = set()
    owner = None
    for num, line in enumerate(text, 1):
        match = RE_KCONFIG_ENTRY.match(line)
        if match:
            owner = match.group(1)
        if owner and (not lines or lines[0] <= num <= lines[1]):
            owners.add(owner)
    return owners


def dt_names(srcdir, path):
    """Find the devicetree names which pull in a .dts or .dtsi file

    A .dtsi reaches a board through a chain of includes, or by being named
    '<name>-u-boot.dtsi', which the build picks up for '<name>.dts' without
    any include at all.

    Args:
        srcdir (str): Source tree
        path (str): A .dts or .dtsi file, relative to the source tree

    Return:
        set of str: Devicetree names, without directory or suffix
    """
    dirname = os.path.dirname(path)
    names = set()
    todo = [os.path.basename(path)]
    seen = set()
    for _ in range(DT_INCLUDE_DEPTH):
        nxt = []
        for fname in todo:
            if fname in seen:
                continue
            seen.add(fname)
            if fname.endswith('.dts'):
                names.add(fname[:-len('.dts')])
                continue
            if fname.endswith('-u-boot.dtsi'):
                names.add(fname[:-len('-u-boot.dtsi')])
            res = subprocess.run(
                ['git', 'grep', '-l', '-F', f'"{fname}"', '--', dirname],
                cwd=srcdir, capture_output=True, text=True, check=False)
            nxt += [os.path.basename(name) for name in res.stdout.split()]
        todo = nxt
    return names


def dt_boards(names, configs):
    """Find the boards which use some devicetrees

    Args:
        names (set of str): Devicetree names, from dt_names()
        configs (dict): Maps board to its configuration, from qconfig.db

    Return:
        dict: Maps each board to ['U-Boot']
    """
    out = {}
    for board, config in configs.items():
        trees = (config.get('CONFIG_DEFAULT_DEVICE_TREE', '').strip('"') +
                 ' ' + config.get('CONFIG_OF_LIST', '').strip('"'))
        if any(tree.rsplit('/', 1)[-1] in names for tree in trees.split()):
            out[board] = ['U-Boot']
    return out


def includers(srcdir, path):
    """Find the source files beside a file which include it

    Args:
        srcdir (str): Source tree
        path (str): Header, or source file which is not built on its own

    Return:
        list of str: .c and .S files in the same directory which include it
    """
    dirname, name = os.path.split(path)
    res = subprocess.run(
        ['git', 'grep', '-l', '-E', f'#include [<"]({re.escape(name)})[>"]',
         '--', f'{dirname}/*.c', f'{dirname}/*.S'],
        cwd=srcdir, capture_output=True, text=True, check=False)
    return [name for name in res.stdout.split() if name != path]


def file_boards(srcdir, path, configs):
    """Find the boards which build a file, and in which phases

    Args:
        srcdir (str): Source tree
        path (str): File, relative to the source tree
        configs (dict): Maps board to its configuration, from qconfig.db

    Return:
        dict: Maps board to the list of Phase which build the file
    """
    levels = file_rules(srcdir, path)
    if levels is None:
        return {}
    needs = path_needs(srcdir, path)
    out = {}
    for board, config in configs.items():
        phases = build_phases(config, levels, needs)
        if phases:
            out[board] = phases
    return out


# pylint: disable-next=too-many-return-statements
def find_boards(srcdir, path, lines, configs, unifdef='unifdef'):
    """Find the boards which build a file, or some lines of it

    Args:
        srcdir (str): Source tree
        path (str): File, relative to the source tree
        lines (tuple): (first, last) lines to look at, or None for any
        configs (dict): Maps board to its configuration, from qconfig.db
        unifdef (str): unifdef program to run

    Return:
        Result: The boards, or a note saying why there is no answer
    """
    if not os.path.exists(os.path.join(srcdir, path)):
        return Result(path, lines, {}, 'no such file')
    if path.startswith('configs/') and path.endswith('_defconfig'):
        board = os.path.basename(path)[:-len('_defconfig')]
        return Result(path, lines, {board: ['U-Boot']}, None)
    if os.path.basename(path).startswith('Kconfig'):
        # A Kconfig entry matters to the boards which turn its option on
        owners = kconfig_owners(srcdir, path, lines)
        return Result(path, lines,
                      {board: ['U-Boot'] for board, config in configs.items()
                       if any(has_option(config, owner)
                              for owner in owners)}, None)
    if path.endswith(DT_SUFFIXES):
        return Result(path, lines,
                      dt_boards(dt_names(srcdir, path), configs), None)
    if path.endswith('.h') and '/include/' in f'/{path}':
        return Result(path, lines, {},
                      'a shared header has too many users to say')
    candidates = {}
    if not path.endswith('.h'):
        candidates = file_boards(srcdir, path, configs)
    if path.endswith('.h') or file_rules(srcdir, path) is None:
        # Built only as part of the files which include it
        users = includers(srcdir, path)
        if not users:
            return Result(path, lines, {},
                          'no Makefile builds it and nothing beside it '
                          'includes it')
        merged = {}
        for user in users:
            for board, phases in file_boards(srcdir, user, configs).items():
                merged.setdefault(board, set()).update(phases)
        candidates = {board: sorted(phases, key=PHASES.index)
                      for board, phases in merged.items()}
    if lines and path.endswith(UNIFDEF_SUFFIXES):
        return Result(path, lines, line_boards(srcdir, path, lines,
                                               candidates, configs, unifdef),
                      None)
    return Result(path, lines,
                  {board: [phase.name for phase in phases]
                   for board, phases in candidates.items()}, None)


def parse_spec(spec):
    """Split 'path[:first[-last]]' into its parts

    Args:
        spec (str): File, optionally with a line or range of lines

    Return:
        tuple:
            str: File
            tuple: (first, last), or None if no lines were given
    """
    match = re.match(r'^(.*?):(\d+)(?:-(\d+))?$', spec)
    if not match:
        return spec, None
    first = int(match.group(2))
    last = int(match.group(3) or first)
    return match.group(1), (first, max(first, last))


def load_configs(srcdir, jobs=None):
    """Read every board's configuration, rebuilding qconfig.db if stale

    Args:
        srcdir (str): Source tree
        jobs (int): Threads to use if the database needs rebuilding

    Return:
        dict: Maps board name (without '_defconfig') to its configuration
    """
    # pylint: disable=import-outside-toplevel
    import qconfig

    old = os.getcwd()
    os.chdir(srcdir)
    try:
        # Keep qconfig's progress off stdout, which may be read by a program
        with contextlib.redirect_stdout(sys.stderr):
            _, _, config_db, _ = qconfig.ensure_database(jobs or os.cpu_count())
    finally:
        os.chdir(old)
    return {name[:-len('_defconfig')] if name.endswith('_defconfig')
            else name: config for name, config in config_db.items() if name}


def show_results(results, configs, fmt):
    """Print the boards for each file

    Args:
        results (list of Result): One per file asked about
        configs (dict): Maps board to its configuration, for the architecture
        fmt (str): 'table', 'list' or 'json'
    """
    def arch(board):
        return configs.get(board, {}).get('CONFIG_SYS_ARCH', '').strip('"')

    if fmt == 'json':
        print(json.dumps([{'path': res.path,
                           'lines': list(res.lines) if res.lines else None,
                           'boards': {board: {'arch': arch(board),
                                              'phases': phases}
                                      for board, phases in res.boards.items()},
                           'note': res.note}
                          for res in results], indent=1))
        return
    for res in results:
        if fmt == 'list':
            for board in sorted(res.boards):
                print(board)
            continue
        where = res.path
        if res.lines:
            where += f':{res.lines[0]}'
            if res.lines[1] != res.lines[0]:
                where += f'-{res.lines[1]}'
        if res.note:
            print(f'{where}: {res.note}')
            continue
        print(f'{len(res.boards)} boards build {where}:')
        for board in sorted(res.boards):
            print(f'  {board:40} {arch(board):10} '
                  f'{", ".join(res.boards[board])}')


def do_boards(args):
    """Handle the 'boards' command

    Args:
        args (Namespace): Parsed arguments with 'specs', 'arch', 'format',
            'source', 'unifdef' and 'jobs' attributes

    Return:
        int: 0 on success, 1 if any file could not be answered
    """
    srcdir = os.path.realpath(args.source)
    configs = load_configs(srcdir, args.jobs)
    if args.arch:
        configs = {board: config for board, config in configs.items()
                   if config.get('CONFIG_SYS_ARCH') == f'"{args.arch}"'}
    results = []
    for spec in args.specs:
        path, lines = parse_spec(spec)
        path = os.path.relpath(os.path.realpath(os.path.join(srcdir, path))
                               if not os.path.isabs(path) else path, srcdir)
        results.append(find_boards(srcdir, path, lines, configs,
                                   args.unifdef))
    show_results(results, configs, args.format)
    if args.format == 'list':
        # The list has nowhere to say why a file has no boards
        for res in results:
            if res.note:
                tout.warning(f'{res.path}: {res.note}')
    return 1 if any(res.note for res in results) else 0
