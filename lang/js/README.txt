// JavaScript GDP access application programs and support libraries
//
// Alec Dara-Abrams
// 2014-11-04

// Currently, some GDP API documentation is available at:
// https://docs.google.com/document/d/1MdJ47NEfUQdJlTyAXwotZp8aJbXchRIi3VgwOz4LWuU/edit?usp=sharing 

// Currently, discussion of the GDP JS interface is on the TSRC Wiki at:
// http://www.terraswarm.org/swarmos/wiki/Main/GDPJavaScriptInterface

// See the apps/ and tests/ subdirectory README's to get started here.


// ===================================================================
Makefile

Recursive make for gdpjs/, apps/ and tests/.
Currently, apps/ and tests/ do not build anything.


// ===================================================================
README.txt
This file.  Also, see README's in our subdirectories.


// ===================================================================
apps/

JS standalone applications programs.  In particular, apps/writer-test.js
and apps/reader-test.js - both hand translations of corresponding gdp/apps/ 
C programs.  These should also provide good examples of access to GDP from
Node.js JavaScript.  Run with Node.js .  See apps/README.txt .


// ===================================================================
gdpjs/

JS and C support libraries.  Has a local Makefile that does build things.


// ===================================================================
libs/

Will hold shared and dynamic libraries built down in gdpjs/


// ===================================================================
node_modules/

Node.js modules required by these JS programs and libraries.
Loaded into our source repository via "npm install <module_name>" .

TBD: these modules might be platform/architecture specific.
If so, modify our makefiles to dynamically install them and
modify our source repository accordingly.
However, there may then be introduced the possibility of npm module
version discrepancies among various local builds.


// ===================================================================
tests/

Testing script for apps/writer-test.js and apps/reader-test.js
Consider:  
    cd ./tests/
    make run


// ===================================================================

Updating the GDP Version Number

If the GDP version number in ../../gdp/gdp_version.h changes, the make the following changes:

1) gdpjs/Makefile: Update
# Version of the GDP Library, should match ../../../gdp/Makefile
GDPLIBMAJVER=	0
GDPLIBMINVER=	6

2) gdpjs/gdpjs.js: Update
var libgdp = ffi.Library(GDP_DIR + '/libs/libgdp.0.6', {

3) Update package.json:
    "version": "0.6.1",

4) Run make all_noavahi


Testing using Ptolemy II
Update $PTII/lib:

  cp libs/libgdpjs.1.0.dylib $PTII/lib
  svn commit -m "Updated to gdp0.6-1." $PTII/lib/libgdpjs.1.0.dylib

See https://www.terraswarm.org/accessors/wiki/Main/GDPJS for how to
1) start the GDP Daemons and initialize the log,
2) create a log and
3) add a record.

Then run the model using Node:

(cd $PTII/org/terraswarm/accessor/accessors/web/gdp/test/auto; node ../../../hosts/node/nodeHostInvoke.js -timeout 6000 gdp/test/auto/GDPLogRead)

The string "foo" should appear in the output:
 >>> recno 1, len 3, ts 2016-08-04T16:11:23.208583000Z
00000000 66 6f 6f
         f  o  o
         
