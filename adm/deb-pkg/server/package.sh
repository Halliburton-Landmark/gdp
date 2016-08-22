#!/bin/sh

# Create a debian package that includes:
# - gdplogd

PACKAGE="gdp-server"
INSTALL_TARGET="install-gdplogd"

curdir=`dirname $0`
topdir="`( cd $curdir/../../../ && pwd )`"
tmpdir="/tmp/"$PACKAGE"_"$VER
scriptdir=$topdir/$curdir
versionfile=$topdir/adm/gdp-version.sh

# Get the version number
. $versionfile
VER=$GDP_VERSION_MAJOR.$GDP_VERSION_MINOR.$GDP_VERSION_PATCH


# Create a postinstall-pak automatically.
mkdir -p $tmpdir
cp -a $topdir/adm $tmpdir/
rm -rf $tmpdir/adm/docker-net $tmpdir/adm/deb-pkg
tar -C $tmpdir -cvzf $tmpdir/adm.tar.gz adm

# from https://community.linuxmint.com/tutorial/view/1998
printf "#!/bin/bash
PAYLOAD_LINE=\`awk '/^__PAYLOAD_BELOW__/ {print NR + 1; exit 0; }' \$0\`
tail -n+\$PAYLOAD_LINE \$0 | tar -xvz
# custom installation command here
cd adm && ./init-gdp-server.sh
exit 0
__PAYLOAD_BELOW__\n" > $tmpdir/postinstall-pak

cat $tmpdir/adm.tar.gz >> $tmpdir/postinstall-pak
chmod +x $tmpdir/postinstall-pak
cp $tmpdir/postinstall-pak $topdir
rm -rf $tmpdir


# Setup the files that checkinstall needs. No way to specify
#   the location. Sad.
cp $scriptdir/description-pak $topdir
cp $scriptdir/preremove-pak $topdir

# Build package
fakeroot checkinstall -D --install=no --fstrans=yes -y \
            --pkgname=$PACKAGE --pkgversion=$VER \
            --pkglicense="See /LICENSE" --pkggroup="misc" \
            --maintainer="eric@cs.berkeley.edu, mor@eecs.berkeley.edu" \
            --requires="libevent-dev, libssl-dev, libjansson-dev, 
                        avahi-daemon, libavahi-client3, rsyslog" \
            --nodoc --strip=no --stripso=no --addso=yes --gzman=yes \
            --deldoc=yes --deldesc=yes \
            make -C $topdir $INSTALL_TARGET

# cleanup
rm -f $topdir/description-pak
rm -f $topdir/postinstall-pak
rm -f $topdir/preremove-pak
