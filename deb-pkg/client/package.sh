#!/bin/sh

# Create a debian package that includes:
# - GDP C library
# - EP C library
# - Documentation
# - GDP applications

if [ $# -gt 0 ]; then
    VER=$1
    MAJVER=`echo $VER | cut -d '.' -f 1`
    MINVER=`echo $VER | cut -d '.' -f 2`
else
    echo "Usage: $0 <version (format: X.Y)>"
    exit 1
fi


PACKAGE="gdp-client"
INSTALL_TARGET="install-client"

curdir=`dirname $0`
topdir="`( cd $curdir/../../ && pwd )`"
tmpdir="/tmp/"$PACKAGE"_"$VER
scriptdir=$topdir/$curdir

# invoke 'make'
cd $topdir && make all

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
