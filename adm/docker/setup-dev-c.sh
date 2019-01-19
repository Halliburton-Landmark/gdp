#!/usr/bin/env sh

#
#  Script to set up the source tree.
#	Installs the client code.
#	Removes the source tree, except for adm, which is used by other
#		builds.
#

set -e

cd /src/gdp
make install-client install-doc
srcfiles=`ls | grep -v '^adm$'`
rm -r $srcfiles
