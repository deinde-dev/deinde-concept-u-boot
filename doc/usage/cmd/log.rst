.. SPDX-License-Identifier: GPL-2.0+:

.. index::
   single: log (command)

log command
===========

Synopsis
--------

::

    log level [<level>]
    log categories
    log drivers
    log filter-list [-d <driver>]
    log filter-add [-A | -D] [-c <category>] [-d <driver>] [-f <files>]
                   [-F <funcs>] [-l <level> | -L <level>] [-p]
    log filter-remove [-d <driver>] (-a | <num>)
    log format [<fmt>]
    log rec <category> <level> <file> <line> <func> <message>

Description
-----------

The log command controls U-Boot's logging system. Each log record carries a
category saying which part of U-Boot produced it, a level saying how
important it is, the file, line and function it came from, and the message
itself. The logging system hands each record to every log driver which is
willing to take it. See :doc:`../../develop/logging` for the design, for how
code produces a record and for the meaning of each level.

The sub-commands show what the build has, control how much of it reaches a
driver, choose which fields the console driver shows, and produce a record
for testing.

log level
~~~~~~~~~

With no argument this lists the levels, from the most important to the least,
marking the default one with '(default)'. The list stops at
CONFIG_LOG_MAX_LEVEL, since records below that are left out of the build
altogether.

With an argument it sets the default level, which is the level a driver uses
when it has no filter of its own: a record less important than the default is
dropped. The argument is either a level name, in lower case, or the number
beside it in the list, counting from 0 for EMERG.

log categories
~~~~~~~~~~~~~~

This lists the categories a record may belong to, one per line. The list
starts with the driver-model uclasses, since every uclass is a category too,
and ends with the categories the logging system declares itself.

log drivers
~~~~~~~~~~~

This lists the log drivers in the build, one per line. The console driver
writes records to the console; the file driver writes them to a file and is
available on sandbox only; the syslog driver sends them over the network.

log filter-list
~~~~~~~~~~~~~~~

This lists the filters attached to a driver, the console driver unless -d
names another one. Each filter takes two or more lines::

    <num>: ALLOW|DENY <=|>= <level>
         Categories: <category>...
         Files: <files>
         Functions: <funcs>

The number is the one 'log filter-remove' takes. ALLOW or DENY says what
becomes of a record the filter matches, and the level test shows which way
the filter compares a record against its level, '<=' where it came from -l
and '>=' where it came from -L. The remaining lines appear only where the
filter asks for them, and a filter with no restriction at all matches every
record.

Filters are consulted in the order listed and the first one to match decides,
so a record which matches nothing is dropped.

log filter-add
~~~~~~~~~~~~~~

This attaches a filter to a driver. A record matches the filter when it
satisfies every condition given, and the options are:

-A
    allow a matching record. This is the default.

-c <category>
    match this category; repeat the option for up to five of them

-d <driver>
    attach the filter to this driver rather than to the console

-D
    deny a matching record

-f <files>
    match a record from one of these files, as a comma-separated list

-F <funcs>
    match a record from one of these functions, as a comma-separated list

-l <level>
    match a record at or above this level in importance

-L <level>
    match a record at or below this level in importance

-p
    show the new filter's number

A deny filter is added at the head of the driver's list and an allow filter
at the tail, so a deny filter is consulted first however the two are added.

Adding any filter replaces the default-level check for that driver, so a deny
filter on its own silences everything: add an allow filter as well to say
what should still get through.

log filter-remove
~~~~~~~~~~~~~~~~~

This removes the filter with the given number from a driver, or every filter
on it with -a. Use -d to name a driver other than the console.

log format
~~~~~~~~~~

With no argument this shows which fields the console driver includes in a
record. With an argument it sets them. The argument is 'default' for the
fields CONFIG_LOGF_FILE, CONFIG_LOGF_LINE and CONFIG_LOGF_FUNC ask for, or
'all' for every field, or a string of these letters:

== ============================================
c  category
l  level
F  file
L  line number
f  function, padded to CONFIG_LOGF_FUNC_PAD
m  message
== ============================================

The fields always appear in that order, whatever order the letters are given
in. The other drivers are not affected.

log rec
~~~~~~~

This produces a single log record with the category, level, file, line,
function and message given, as if U-Boot's own code had produced it. It is a
way of seeing what the levels, filters and format do without having to
rebuild.

The level is either a name or a number. The category may be any name 'log
categories' lists; an unknown one is reported as 'none' rather than refused.
As with any other record, the result is dropped where the levels and filters
do not let it through.

Example
-------

This shows the levels on sandbox, where CONFIG_LOG_MAX_LEVEL is 9 and the
default is INFO::

    => log level
    EMERG
    ALERT
    CRIT
    ERR
    WARNING
    NOTICE
    INFO (default)
    DEBUG
    CONTENT
    IO
    => log drivers
    console
    file

The categories are listed in the same way, starting with the uclasses::

    => log categories
    root
    demo
    test
    ...
    expo
    console
    test

With every field asked for, a record shows where it came from. The second
record here is dropped, since DEBUG is less important than the default
level, and the command still succeeds::

    => log format all
    => log rec arch notice file.c 123 probe hello
    NOTICE.arch,file.c:123-               probe() hello
    => log rec arch debug file.c 124 probe quiet
    => echo $?
    0
    => log format
    Log format: clFLfm
    => log format default
    => log format
    Log format: fm

Filters cut down what a driver emits. These three stop SPI and MMC records,
let everything up to INFO through, and drop anything at DEBUG or beyond from
two files::

    => log filter-add -p -D -c spi -c mmc
    0
    => log filter-add -p -A -l info
    1
    => log filter-add -p -D -f drivers/core/lists.c,drivers/core/ofnode.c -L 7
    2
    => log filter-list
    2  : DENY >= DEBUG
         Files: drivers/core/lists.c,drivers/core/ofnode.c
    0  : DENY <= IO
         Categories: spi mmc
    1  : ALLOW <= INFO
    => log filter-remove 2
    => log filter-remove -a
    => log filter-list

Giving no sub-command, or one which is not recognised, shows the usage
message::

    => log bogus
    log - log system

    Usage:
    log level [<level>] - get/set log level
    log categories - list log categories
    log drivers - list log drivers
    ...

Return value
------------

The return value $? is 0 (true) where the sub-command does what it was asked
and 1 (false) otherwise: an unknown level, category or driver, a filter
number which is not a number or names no filter, a letter 'log format' does
not know, a missing or unrecognised sub-command, or an option a sub-command
does not take.

A record which the levels or the filters drop is not a failure, so 'log rec'
returns 0 whether or not the record appears.

Configuration
-------------

The command is only available if CONFIG_CMD_LOG=y, which needs CONFIG_LOG=y.
CONFIG_LOG_MAX_LEVEL sets how many levels the build keeps,
CONFIG_LOG_DEFAULT_LEVEL the default level it starts with, and
CONFIG_LOGF_FILE, CONFIG_LOGF_LINE and CONFIG_LOGF_FUNC the fields
'log format default' selects.

See also
--------

* :doc:`trace<trace>` for recording which functions U-Boot calls, rather than
  the messages it produces
* :doc:`bootstage<bootstage>` for timing records, which are collected and
  reported separately from log records
