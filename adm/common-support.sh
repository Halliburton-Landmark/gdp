#!/bin/sh
{ test -r /usr/local/etc/gdp.conf.sh && . /usr/local/etc/gdp.conf.sh; } ||
	{ test -r /etc/gdp.conf.sh && . /etc/gdp.conf.sh; }

#
#  These macros are intended to be sourced into other shell files
#

#################### MANIFEST CONSTANTS ####################

Reset='[0m'    # Text Reset

# Regular           Bold                Underline           High Intensity      BoldHigh Intens     Background          High Intensity Backgrounds
Bla='[0;30m';     BBla='[1;30m';    UBla='[4;30m';    IBla='[0;90m';    BIBla='[1;90m';   On_Bla='[40m';    On_IBla='[0;100m';
Red='[0;31m';     BRed='[1;31m';    URed='[4;31m';    IRed='[0;91m';    BIRed='[1;91m';   On_Red='[41m';    On_IRed='[0;101m';
Gre='[0;32m';     BGre='[1;32m';    UGre='[4;32m';    IGre='[0;92m';    BIGre='[1;92m';   On_Gre='[42m';    On_IGre='[0;102m';
Yel='[0;33m';     BYel='[1;33m';    UYel='[4;33m';    IYel='[0;93m';    BIYel='[1;93m';   On_Yel='[43m';    On_IYel='[0;103m';
Blu='[0;34m';     BBlu='[1;34m';    UBlu='[4;34m';    IBlu='[0;94m';    BIBlu='[1;94m';   On_Blu='[44m';    On_IBlu='[0;104m';
Pur='[0;35m';     BPur='[1;35m';    UPur='[4;35m';    IPur='[0;95m';    BIPur='[1;95m';   On_Pur='[45m';    On_IPur='[0;105m';
Cya='[0;36m';     BCya='[1;36m';    UCya='[4;36m';    ICya='[0;96m';    BICya='[1;96m';   On_Cya='[46m';    On_ICya='[0;106m';
Whi='[0;37m';     BWhi='[1;37m';    UWhi='[4;37m';    IWhi='[0;97m';    BIWhi='[1;97m';   On_Whi='[47m';    On_IWhi='[0;107m';

# matches sysexits.h (see man sysexits)
EX_OK=0
EX_USAGE=64
EX_UNAVAILABLE=69
EX_SOFTWARE=70
EX_OSFILE=72
EX_CANTCREATE=73
EX_NOPERM=77
EX_CONFIG=78

#################### FUNCTIONS ####################

: ${quiet:=false}

# Error/Information messages
info() {
    if ! $quiet; then
	echo "${Gre}${On_Bla}[INFO] $1${Reset}" 2>&1
    fi
}

warn() {
    if ! $quiet; then
	echo "${Yel}${On_Bla}[WARN] $1${Reset}" 2>&1
    fi
}

error() {
    if ! $quiet; then
	echo "${Red}${On_Bla}[ERROR] $1${Reset}" 2>&1
    fi
}

fatal() {
	echo "${Whi}${On_Red}[FATAL] $1${Reset}" 2>&1
	exit ${2:-$EX_UNAVAILABLE}
}

action() {
	echo "${Bla}${On_Yel}[ACTION REQUIRED] $1${Reset}" 2>&1
}

# Read a password from the terminal
case `echo "foo\c"`,`echo -n bar` in
    *c*,-n*)	echo_n=		echo_c=		;;
    *c*,*)	echo_n=-n	echo_c=		;;
    *)		echo_n=		echo_c='\c'	;;
esac

#
#  Read a password without echo.
#  The first parameter is the name of the variable to set.
#  The second parameter is an optional prompt.
#
read_passwd() {
	local var=$1
	local prompt="${2:-Password}"

	stty -echo
	echo $echo_n "${prompt}:" $echo_c
	read $var
	echo ""
	stty echo
}

# Create a directory as the user gdp
mkdir_gdp() {
	test -d $1 && return
	if [ -e $1 ]; then
		warn "$1 exists but is not a directory"
		return
	fi
	info "Creating $1 as ${GDP_USER}:${GDP_GROUP}"
	sudo mkdir -p $1
	sudo chmod ${2:-0775} $1
	sudo chown ${GDP_USER}:${GDP_GROUP} $1
}

# Create a file as the user gdp
mkfile_gdp() {
	test -f $1 && return
	if [ -e $1 ]; then
		warn "$1 exists but is not a file"
		return
	fi
	info "Creating $1 as ${GDP_USER}:${GDP_GROUP}"
	sudo cp /dev/null $1
	sudo chmod ${2:-0664} $1
	sudo chown ${GDP_USER}:${GDP_GROUP} $1
}

package() {
    info "Checking package $1..."
    case "${PKGMGR:-unknown}" in
	"debian")
	    if dpkg --get-selections | grep --quiet $1; then
		info "$1 is already installed. skipping."
	    else
		sudo apt-get install -y $@
	    fi
	    ;;

	"yum")
	    if rpm -qa | grep --quiet $1; then
		info "$1 is already installed. skipping."
	    else
		sudo yum install -y $@
	    fi
	    ;;

	"brew")
	    if brew list | grep --quiet $1; then
		info "$1 is already installed. skipping."
	    else
		brew install --build-bottle $@ || brew upgrade $@
	    fi
	    ;;

	"macports")
	    if port -q installed $1 | grep -q "."; then
		info "$1 is already installed. skipping."
	    else
		sudo port install $1
	    fi
	    ;;

	"freebsd")
	    export PATH="/sbin:/usr/sbin:$PATH"
	    if sudo pkg info -q $1; then
		info "$1 is already installed. skipping."
	    else
		sudo pkg install $@
	    fi
	    ;;

	"gentoo")
	    if equery list $1 >& /dev/null; then
		info "$1 is already installed. skipping."
	    else
		sudo emerge $1
	    fi
	    ;;

	*)
	    fatal "unrecognized PKGMGR $PKGMGR"
	    ;;
    esac
}

# configure defaults
configure_defaults() {
    : ${GDP_ROOT:=/usr}
    if [ "$GDP_ROOT" = "/usr" ]
    then
	    : ${GDP_ETC:=/etc/gdp}
    elif [ "$GDP_ROOT" = "/usr/local" -o "$GDP_ROOT" = "/opt/local" ]
    then
	    : ${GDP_ETC:=$GDP_ROOT/etc/gdp}
    else
	    : ${GDP_ETC:=$GDP_ROOT/etc}
    fi
    : ${EP_PARAMS:=$GDP_ETC/params}
    : ${GDP_USER:=gdp}
    : ${GDP_GROUP:=$GDP_USER}
    : ${GDP_LOG_DIR:=/var/log/gdp}
    : ${GDP_VAR:=/var/swarm/gdp}
    : ${GDP_KEYS_DIR:=$GDP_ETC/keys}
    : ${GDPLOGD_DATADIR:=$GDP_VAR/glogs}
}


dot_to_int() {
	full=$1
	major=`echo $full | sed 's/\..*//'`
	full=`echo $full | sed -e 's/[^.]*//' -e 's/^\.//'`
	minor=`echo $full | sed 's/\..*//'`
	full=`echo $full | sed -e 's/[^.]*//' -e 's/^\.//'`
	patch=`echo $full | sed 's/\..*//'`
	test -z "$minor" && minor=0
	test -z "$patch" && patch=0
	printf "%02d%02d%02d" "$major" "$minor" "$patch"
}

###
###  figure out operating system and version number
###
OS=""
OSVER=""

fatal_osver() {
	fatal "Must be running $1 or later (have $VERSION_ID)"
}

warn_unsupported() {
	msg=${1:-"try anyway"}
	warn "$OS is not a supported platform, but I'll $msg"
}

set_os() {
    if [ -f "/etc/os-release" ]; then
	. /etc/os-release
	OS="${ID-}"
	OSVER="${VERSION_ID-}"
    fi
    if [ -z "$OS" -a -f "/etc/lsb-release" ]; then
	. /etc/lsb-release
	OS="${DISTRIB_ID-}"
	OSVER="${DISTRIB_VERSION-}"
    fi
    if [ "$OS" ]; then
	# it is set --- do nothing
	true
    elif [ -f "/etc/centos-release" ]; then
	OS="centos"
	#OSVER=???
    elif [ -f "/etc/redhat-release" ]; then
	OS="redhat"
	OSVER=`sed -e 's/.* release //' -e 's/ .*//' /etc/redhat-release`
    else
	OS=`uname -s`
    fi
    OS=`echo $OS | tr '[A-Z]' '[a-z]'`
    if [ "$OS" = "linux" ]; then
	OS=`head -1 /etc/issue | sed 's/ .*//' | tr '[A-Z]' '[a-z]'`
    elif [ "$OS" = "darwin" ]; then
	    OSVER=`sw_vers |
		    sed -e '/ProductVersion:/!d' -e 's/^.*[ 	][ 	]*//'`
    elif [ "$OS" = "freebsd" ]; then
	    _major=`uname -r | sed -e 's/\..*//'`
	    _minor=`uname -r | sed -e 's/[0-9]*\.//' -e 's/-.*//'`
	    OSVER="$_major.$_minor"
    fi

    if [ -z "$OSVER" ]; then
	OSVER="0"
    else
	# clean up OSVER to make it a single integer
	OSVER=`dot_to_int $OSVER`
    fi
}

check_os() {
    case "$OS" in
	"debian")
	    if expr $OSVER \< 80000 > /dev/null
	    then
		    fatal_osver "Debian 8 (Jessie)"
	    fi
	    ;;

	"ubuntu")
	    if expr $OSVER \< 140400 > /dev/null
	    then
		    fatal_osver "Ubuntu 14.04"
	    fi
	    ;;

	"raspbian")
	    warn_unsupported "assume it is debian-based"
	    ;;

	"centos" | "freebsd" | "darwin" | "redhat" | "gentoo")
	    warn_unsupported
	    ;;

	*)
	    fatal "Oops, we don't support $OS"
    esac
}


# check to make sure we understand this OS and OSVER; choose PKGMGR & INITsys
set_pkgmgr() {
    : ${PKGMGR:=$OS}
    INITguess=""
    case $PKGMGR in
      "debian"|"ubuntu")
	    PKGMGR=debian
	    if expr $OSVER \>= 160400 > /dev/null
	    then
		    INITguess=systemd
	    else
		    INITguess=upstart
	    fi
	    ;;

      "raspbian")
	    PKGMGR=debian
	    ;;

      "redhat")
	    PKGMGR=yum
	    if expr $OSVER \< 070000 > /dev/null
	    then
		    INITguess=systemd
	    else
		    INITguess=upstart
	    fi
	    ;;

      "darwin")
	    if type port > /dev/null 2>&1 && port installed | grep -q .; then
		    PKGMGR=macports
	    fi
	    if type brew > /dev/null 2>&1 && [ ! -z "`brew config`" ]
	    then
		if [ "$PKGMGR" = "macports" ]; then
		    PKGMGR=brewports
		    warn "You seem to have both macports and homebrew installed."
		    warn "They conflict with each other, and you may break all your"
		    warn "packages if you try to use them at the same time."
		    warn "Please choose one or the other.  Macports seems to work better."
		    fatal "Set envar PKGMGR to 'brew' or 'macports' to choose."
		else
		    PKGMGR=brew
		fi
	    fi
	    if [ "$PKGMGR" = "darwin" ]; then
		warn "You must install macports or homebrew."
		fatal "See README-compiling.md (Operating System Quirks) for details."
	    fi
	    ;;

      "centos")
	    PKGMGR=yum
	    ;;
    esac

    # determine what init system we are using (heuristic!)
    if [ -z "$INITguess" ]
    then
	    case $OS in
	      "debian" | "ubuntu" | "centos" | "redhat" | "gentoo")
		    # some linux variant; see if we can figure out systemd
		    proc1exe=`sudo stat /proc/1/exe | grep 'File: '`
		    if echo "$proc1exe" | grep -q "systemd"
		    then
			    INITguess="systemd"
		    fi
		    ;;
	    esac
    fi
}


#################### END OF FUNCTIONS ####################

configure_defaults
set_os
check_os
if [ -z "$PKGMGR" ]
then
    set_pkgmgr
fi

# see if we should use our guess
test -z "$INITSYS" && INITSYS=${INITguess:-unknown}

if ! $quiet
then
    info "System Info: OS=$OS, OSVER=$OSVER, PKGMGR=$PKGMGR, INITSYS=$INITSYS"
fi
