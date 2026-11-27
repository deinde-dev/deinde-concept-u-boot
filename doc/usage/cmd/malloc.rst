.. SPDX-License-Identifier: GPL-2.0+:

.. index::
   single: malloc (command)

malloc command
==============

Synopsis
--------

::

    malloc info
    malloc dump
    malloc leak [start | end]
    malloc log [start | stop | dump]

Description
-----------

The malloc command shows information about the malloc heap. The size of the
heap is set by CONFIG_SYS_MALLOC_LEN.

info
    Shows memory-allocation statistics: the size of the heap, the amount of
    it in use, the largest block still free and the number of calls to
    malloc(), free() and realloc(). With CONFIG_MCHECK_HEAP_PROTECTION it
    also shows how many allocations the mcheck library is tracking, which is
    the number in use.

dump
    Walks the heap and shows each chunk's address, its size in hex and its
    status. A chunk which is free is marked ``free`` and one in use has no
    label. Three entries are not chunks: the heap's own header, the ``top``
    chunk which holds the memory not yet handed out, and the ``end`` of the
    heap. With CONFIG_MCHECK_HEAP_PROTECTION the function which asked for
    the chunk is shown too, where it is known. A count of the bytes used and
    free closes the dump.

leak
    Reports allocations which have not been freed. The ``start`` form takes
    a snapshot of the chunks in use; ``end`` lists the chunks allocated
    since the snapshot and then drops it. With no argument the command
    counts the new allocations and keeps the snapshot, so a long sequence
    can be narrowed down a step at a time. Each new allocation is shown as
    its chunk address, its size in hex and, with
    CONFIG_MCHECK_HEAP_PROTECTION, the function which asked for it.

log
    Records the traffic through malloc(). The ``start`` form begins
    recording and ``stop`` ends it. The ``dump`` form, or a bare ``malloc
    log``, shows what was recorded: a sequence number, the type of the call,
    the address, the size and the caller. A free() has no size. The log
    lives in memory belonging to the host, so recording does not itself
    disturb the heap.

Example
-------

This shows the statistics on sandbox::

    => malloc info
    total bytes   = 128 MiB
    in use bytes  = 1.1 MiB
    largest free  = 126.9 MiB
    malloc count  = 2491
    free count    = 1498
    realloc count = 4
    mcheck count  = 1113

The dump has a line for each chunk in the heap, so it is long. This shows the
first few lines, a free chunk from the middle and the last few::

    => malloc dump
    Heap dump: 165e3000 - 1e5e5000
         Address        Size  Status
    ----------------------------------
        165e3000          10  (chunk header)
        165e3010          c0        log_init:472 <-board_init_r:807 <-sandbox_flow:799
        165e3190       20090        membuf_new:423 <-console_record_init:1029 <-board_init_r:807
        ...
        166571b0          80  free
        ...
        166f29d0     7ef25f0  top
        1e5e5000              end
    ----------------------------------
    Used: 10f730 bytes in 1113 chunks
    Free: 7ef2880 bytes in 6 chunks + top

Setting an environment variable allocates memory for the name and for the
value, which the leak check finds. The whole sequence goes on one line since
the shell allocates memory of its own while it parses a line: a snapshot
taken on a line by itself reports that as a leak::

    => malloc leak start; setenv fred wilma; malloc leak; malloc leak end
    Heap snapshot: 1134 allocs
    2 new allocs
      166f38a0 90 memdup_nul:364 <-hsearch_r:403 <-env_do_env_set:132
      166f3930 90 memdup_nul:364 <-hsearch_r:403 <-env_do_env_set:132
    2 leaked allocs

The traffic log shows the same work, including the allocations which are
freed again::

    => malloc log start; setenv fred wilma; malloc log stop; malloc log
    Malloc logging started
    Malloc logging stopped
    Malloc log: 80 entries (max 65536, total 80)
     Seq  Type         Address      Size  Caller
    ----  --------  ----------  --------  ------
       0  alloc        66f2400         6  env_do_env_set:114 <-cmd_invoke_rep:542 <-cmd_process:688
       1  alloc        66f28b0         5  memdup_nul:364 <-hsearch_r:403 <-env_do_env_set:132
       2  alloc        66f2940         6  memdup_nul:364 <-hsearch_r:403 <-env_do_env_set:132
       3  alloc        66f29d0        16  env_attr_walk:28 <-env_attr_lookup:184 <-env_callback_init:66
       4  free         66f29d0         0  env_attr_walk:28 <-env_attr_lookup:184 <-env_callback_init:66
       ...

Configuration
-------------

The command is available if CONFIG_CMD_MALLOC=y, which needs
CONFIG_MALLOC_DEBUG=y. The leak sub-command is available if
CONFIG_CMD_MALLOC_LEAK=y, which is for sandbox only, and the log sub-command
if CONFIG_CMD_MALLOC_LOG=y, which needs CONFIG_MCHECK_LOG=y.

Return value
------------

The return value $? is 0 (true) if the sub-command does its work. It is 1
(false) if the sub-command is missing or is not one of those above, if a
sub-command is given an argument it does not take, or if a leak check is
asked for when no snapshot has been taken.

See also
--------

* :doc:`meminfo<meminfo>` for the region of memory the heap sits in
* :doc:`bdinfo<bdinfo>` for the early malloc pool, which is used before
  relocation
