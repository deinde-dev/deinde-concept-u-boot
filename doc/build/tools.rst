.. SPDX-License-Identifier: GPL-2.0+
.. sectionauthor:: Bin Meng <bmeng.cn@gmail.com>

Host tools
==========

Building tools for Linux
------------------------

To allow distributions to distribute all possible tools in a generic way,
avoiding the need of specific building tools for each machine, a tools-only
defconfig file is provided.

Using this, we can build the tools by doing::

   $ make tools-only_defconfig
   $ make tools-only

Building tools for Windows
--------------------------
If you wish to generate Windows versions of the utilities in the tools directory
you can use MSYS2, a software distro and building platform for Windows.

Download the MSYS2 installer from https://www.msys2.org. Make sure you have
installed all required packages below in order to build these host tools::

   * gcc (9.1.0)
   * make (4.2.1)
   * bison (3.4.2)
   * diffutils (3.7)
   * openssl-devel (1.1.1.d)

Note that the version numbers in parentheses above are the package versions at
the time of writing this document.

There are 3 MSYS subsystems installed: MSYS2, MinGW32 and MinGW64. Each
subsystem provides an environment to build Windows applications. The MSYS2
environment is for building POSIX compliant software on Windows using an
emulation layer. The MinGW32/64 subsystems are for building native Windows
applications using a linux toolchain (gcc, bash, etc), targeting respectively
32 and 64 bit Windows.

Launch the MSYS2 shell of the MSYS2 environment, and do the following::

   $ make tools-only_defconfig
   $ make tools-only


Building without Python
-----------------------

The tools-only builds pylibfdt by default. To disable this, use the
NO_PYTHON variable::

   NO_PYTHON=1 make tools-only_defconfig tools-only

.. _python_tool_checkouts:

Running Python tools from a checkout
------------------------------------

Several Python tools, such as binman, buildman and dtoc, are maintained as
separate projects and installed with pip. If you work on one of them, you can
use a checkout instead, by telling U-Boot where to find it in
``~/.config/u-boot/tools.conf``::

    [tools]
    # Directory holding a checkout of each tool, e.g. ~/dev/dtoc
    dir = ~/dev

    # Use a different checkout for one tool
    dtoc = ~/dev/dtoc-test

A checkout is the top directory of the tool's git repository, which holds the
package directory, e.g. ``~/dev/dtoc/dtoc``. The ``U_BOOT_TOOLS_DIR``
environment variable, if set, is used instead of the file, as the directory
holding a checkout of each tool.

With this, ``tools/binman/binman``, ``tools/buildman/buildman`` and
``tools/dtoc/dtoc`` run the checkout instead of saying how to install the
package, and ``tools/qconfig.py`` finds kconfiglib in a buildman checkout. The
build runs a binman or dtoc on the PATH in preference to these, so to use the
checkout there, remove the installed copy, or point the build at the checkout,
e.g. ``make DTOC=~/dev/dtoc/dtoc/main.py``.
