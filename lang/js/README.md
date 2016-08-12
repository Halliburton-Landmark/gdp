JavaScript GDP access application programs and support libraries
===============================================================

Authors
-------
* Alec Dara-Abrams: 11/04/2015 - 12/2015
* Christopher Brooks: 12/2015 - present

Resources
--------
* [GDP API documentation](https://docs.google.com/document/d/1MdJ47NEfUQdJlTyAXwotZp8aJbXchRIi3VgwOz4LWuU/edit?usp=sharing) 
* [Discussion of the GDP JS interface](http://www.terraswarm.org/swarmos/wiki/Main/GDPJavaScriptInterface)

See the apps/ and tests/ subdirectory README's to get started here.

Files
-----
* Makefile: Recursive make for gdpjs/, apps/ and tests/.
  Currently, apps/ and tests/ do not build anything.

* README.txt: This file.  Also, see README's in our subdirectories.

* apps/: JS standalone applications programs.  In particular, apps/writer-test.js
  and apps/reader-test.js - both hand translations of corresponding gdp/apps/ 
  C programs.  These should also provide good examples of access to GDP from
  Node.js JavaScript.  Run with Node.js .  See apps/README.txt .

* gdpjs/: JS and C support libraries.  Has a local Makefile that does build things.


* libs/: Running make in gdpjs/ populates the libs/ directory.  If $PTII
  is set, then shared libraries for other platforms are copied from $PTII/lib.


* node_modules/: Node.js modules required by these JS programs and
  libraries.  Loaded into our source repository via "npm install
  <module_name>" .

* tests/: Not working yet. Testing script for apps/writer-test.js and apps/reader-test.js
  Consider:  
        cd ./tests/
	make run
  
Updating the GDP Version Number
==============================

If the GDP version number in ../../gdp/gdp_version.h changes, the make the following changes:

1. gdpjs/Makefile: Update:
        # Version of the GDP Library, should match ../../../gdp/Makefile
	GDPLIBMAJVER=	0
	GDPLIBMINVER=	7

2. gdpjs/gdpjs.js: Update
        var libgdp = ffi.Library(GDP_DIR + '/libs/libgdp.0.6', {

3. Update package.json:
        "version": "0.6.1",

4. Run make all_noavahi


Testing using Ptolemy II
========================
If necessary, update $PTII/lib:

        cp libs/libgdpjs.1.0.dylib $PTII/lib
        svn commit -m "Updated to gdp0.7.0." $PTII/lib/libgdpjs.1.0.dylib

Then run the model using Node:

        (cd $PTII/org/terraswarm/accessor/accessors/web/gdp/test/auto; node ../../../hosts/node/nodeHostInvoke.js -timeout 6000 gdp/test/auto/GDPLogCreateAppendReadJS)

The Node Host Composite Accessor creates a log on edu.berkeley.eecs.gdp-01.gdplogd, appends to it and reads from it.


Install the npm @terrswarm/gdp package on the npm server.
========================================================

We are using an account named
'[terraswarm](https://www.npmjs.com/~terraswarm)' on the npmjs
repository to manage the @terraswarm/gdp package.

To update the @terraswarm/gdp package on npmjs:

1.  Update libgdp and libep:
        (cd ../..; make all_noavahi)
2.  Update libgdpjs:
        make all_noavahi
3.  Update the patch number in package.json
4.  Login to npm
        npm login

        Username: terraswarm
        Password: See ~terra/.npmpass on terra
        Email: terraswarm-software@terraswarm.org 
5.  Publish:
        npm publish --access public


