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
scriptdir=$topdir/$curdir
versionfile=$topdir/adm/gdp-version.sh

# Get the version number
. $versionfile
VER=$GDP_VERSION_MAJOR.$GDP_VERSION_MINOR.$GDP_VERSION_PATCH

# Setup the files that checkinstall needs. No way to specify
#   the location. Sad.
cp $scriptdir/description-pak $topdir
cp $scriptdir/postinstall-pak $topdir

# Build package
fakeroot checkinstall -D --install=no --fstrans=yes -y \
            --pkgname=$PACKAGE --pkgversion=$VER \
            --pkglicense="See /LICENSE" --pkggroup="libs" \
            --maintainer="eric@cs.berkeley.edu, mor@eecs.berkeley.edu" \
            --requires="libevent-dev, libssl-dev, libjansson-dev, 
                        libavahi-client-dev, libavahi-common-dev, 
                        libavahi-client3, avahi-daemon" \
            --nodoc --strip=no --stripso=no --addso=yes --gzman=yes \
            --deldoc=yes --deldesc=yes \
            make -C $topdir $INSTALL_TARGET

# cleanup
rm -f $topdir/description-pak
rm -f $topdir/postinstall-pak
