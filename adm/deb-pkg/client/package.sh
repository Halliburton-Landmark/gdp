#!/bin/sh

# Create a debian package that includes:
# - GDP C library
# - EP C library
# - Documentation
# - GDP applications

PACKAGE="gdp-client"
INSTALL_TARGET="install-client"

curdir=`dirname $0`
topdir="`( cd $curdir/../../../ && pwd )`"
scriptdir=$topdir/adm/deb-pkg/client
common=$topdir/adm/deb-pkg/common
cd $topdir

# Get the version number
. adm/gdp-version.sh
VER=$GDP_VERSION_MAJOR.$GDP_VERSION_MINOR.$GDP_VERSION_PATCH

# Setup the files that checkinstall needs. No way to specify
#   the location. Sad.
cp $scriptdir/description-pak .
cat $common/postinstall-pak > postinstall-pak
chmod +x postinstall-pak

# Build package
#	For binaries only, requires libevent-pthreads-2.0.5,
#		libprotobuf-c1, libavahi-client3.
#	For developers (those writing programs that use the GDP),
#		the others dependencies are also required.
fakeroot checkinstall -D --install=no --fstrans=yes -y \
	    --pkgname=$PACKAGE \
	    --pkgversion=$VER \
	    --pkglicense="See /LICENSE" \
	    --pkggroup="libs" \
	    --maintainer="eric@cs.berkeley.edu, mor@eecs.berkeley.edu" \
	    --requires="libevent-dev,
			libssl-dev,
			libprotobuf-c1,
			uuid-dev,
			libavahi-client-dev,
			libavahi-common-dev,
			libavahi-client3,
			libmariadb-dev,
			avahi-daemon" \
	    --nodoc \
	    --strip=no \
	    --stripso=no \
	    --addso=yes \
	    --gzman=yes \
	    --deldoc=yes \
	    --deldesc=yes \
	    make -C $topdir $INSTALL_TARGET

# cleanup
rm -f description-pak
rm -f postinstall-pak
