<!-- Use "pandoc -sS -o README-compiling.html README-compiling.md" to process this to HTML -->

COMPILING THE GLOBAL DATAPLANE SOFTWARE
=======================================

This directory contains the source code for the Global Data Plane
(GDP) with several language bindings.  The primary binding is for
C (and will work with C++).  The others include Python, Java, and
Javascript.

NOTE: these instructions assume you are starting with the source
distribution.  This is not appropriate if you are installing the
Debian package.  Since you are reading this, you presumably already
have the source code.  If not, you can get the GDP source distribution
using one of the following commands:

		git clone https://repo.eecs.berkeley.edu/git/projects/swarmlab/gdp.git
		git clone repoman@repo.eecs.berkeley.edu:projects/swarmlab/gdp.git

The second is only available if you have registered your public
ssh key with `repo.eecs.berkeley.edu`.  For the moment that repository
is not open; contact Eric Allman <eric@cs.berkeley.edu> to get access.

At the moment, compiles work on many platforms, including Debian,
RedHat, MacOS, and FreeBSD.  However, some other GDP-related packages
work on Debian only (which includes Ubuntu), so you may have
difficulties outside the main source tree.

Installing Requisite Packages
-----------------------------

When compiling from source code, there is no distinction between
client and server packages; both are compiled every time.  For this
reason, you must install all requisite packages before compiling.
The easiest way to do this is to run the `adm/gdp-setup.sh`
script.

Note that on some systems you may need to install the compile suite
as well.  For example, on MacOS you must install XCode in order to
get the compilers, libraries, and build tools you will need.

Compilation
-----------

Compiling the primary code tree should just be a matter of typing
`make` in the root of the `gdp` tree.  If you want to clear out
old cruft, use `make clean`.  You can install the packages in the
system tree using `make install`.  If you prefer installing into
something other than the main tree, set the `LOCALROOT` variable
on the `make` command line.  For example:

		make clean install LOCALROOT=/usr/local

It is not necessary to install the code for testing and debugging.

If you are going to be debugging it can be convenient to use
`O=` on the `make` command line.  This will turn off optimization,
which makes debuggers more understandable.

Note: gcc on linux has a bug that causes it to complain about
non-constant expressions in an initializer when the `-std=c99`
flag is given.  Those same expressions are constant in Clang
and even in gcc without the `-std=c99` flag.  As a result of
this problem, we don't use the `-std=c99` flag by default, but
this means that not all features of C99 are available.
If you want full C99, use `STD=-std=c99` on the make command
line.

Further note: At least some versions of gcc give warnings
about ignored return values even when the function call has
been explicitly voided.  We know about this and do not
consider it to be a bug in the GDP code.  If these warnings
bother you we recommend installing clang and using that
compiler.  (Hint: it gives much better error messages and
catches things that gcc does not.)

### Other language bindings

In addition to C, there is support for Python, Java, and
Javascript.  These bindings are all in the `lang` subtree.
See the instructions in those directories for compiling.

Next Steps
----------

If you just want to use the GDP client programs, continue
reading README.md

If you intend to install and maintain your own GDP routers
and/or log servers, please continue with README-admin.md.

If you plan on debugging the GDP code itself, continue with
README-developers.md.

Directory Structure
-------------------

The following is a brief explanation of the subdirectories
contained in this source tree.

* ep --- A library of C utility functions.  This is a stripped
	down version of a library I wrote several years ago.
	If you look at the code you'll see vestiges of some
	of the stripped out functions.  I plan on cleaning
	this version up and releasing it again.

* gdp --- A library for GDP manipulation.  This is the library
	that applications must link to access the GDP.

* gdplogd --- The GDP log daemon.  This implements physical
	(on disk) logs for the GDP.  The implementation is
	still fairly simplistic.  It depends on a routing
	layer (currently gdp_router, in a separate repository).

* scgilib --- An updated version of the SCGI code from
	[`http://www.xamuel.com/scgilib/`](http://www.xameul.com/scgilib/).
	SCGI permits a web server to access outside programs by opening
	a socket in a manner much more efficient than basic
	CGI fork/exec.  This is only used for the REST interface.

* apps --- Application programs, including tests.

* doc --- Some documentation, woefully incomplete.

* examples --- Some example programs, intended be usable as
	tutorials.

* lang --- sub-directories with language-specific application
	programs and supporting code.

* lang/java --- Java-specific apps and libraries.

* lang/js --- JavaScript-specific apps and libraries.  Also contains
	the Node.js/JS GDP RESTful interface code.  See associated
	README files for details.

* lang/python --- Python-specific apps and libraries.  See the
	associated README file for details.

<!-- vim: set ai sw=4 sts=4 ts=4 : -->
