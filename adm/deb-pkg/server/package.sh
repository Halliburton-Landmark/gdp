#!/bin/sh

# Create a debian package that includes:
# - gdplogd

PACKAGE="gdp-server"
INSTALL_TARGET="install-gdplogd"

curdir=`dirname $0`
topdir="`( cd $curdir/../../../ && pwd )`"
scriptdir=$topdir/adm/deb-pkg/server
common=$topdir/adm/deb-pkg/common
cd $topdir

. adm/gdp-version.sh
VER=$GDP_VERSION_MAJOR.$GDP_VERSION_MINOR.$GDP_VERSION_PATCH

# Setup the files that checkinstall needs. No way to specify
#   the location. Sad.
cp $scriptdir/description-pak .
cp $scriptdir/preremove-pak .
cat $common/postinstall-pak $scriptdir/postinstall-pak > postinstall-pak
chmod +x postinstall-pak

# Build package
fakeroot checkinstall -D --install=no --fstrans=yes -y \
	    --pkgname=$PACKAGE \
	    --pkgversion=$VER \
	    --pkglicense="See /LICENSE" \
	    --pkggroup="misc" \
	    --maintainer="eric@cs.berkeley.edu, mor@eecs.berkeley.edu" \
	    --requires="libevent-pthreads-2.0-5,
			libssl1.0.0,
			libjansson4,
			libprotobuf-c1,
			avahi-daemon,
			libavahi-client3,
			rsyslog" \
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
rm -f preremove-pak
