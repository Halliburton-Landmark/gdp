#!/bin/sh

#
#  These macros are intended to be sourced into other shell files
#

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

info() {
	echo "${Gre}${On_Bla}[INFO] $1${Reset}"
}

warn() {
	echo "${Yel}${On_Bla}[WARN] $1${Reset}"
}

fatal() {
	echo "${Whi}${On_Red}[FATAL] $1${Reset}"
	exit 1
}

platform() {
    local  __resultvar=$1
    local result=""
    local arch=`arch`

    if [ -f "/etc/os-release" ]; then
	. /etc/os-release
	result="${ID-}"
    fi
    if [ -z "$result" -a -f "/etc/lsb-release" ]; then
	. /etc/lsb-release
	result="${DISTRIB_ID-}"
    fi
    if [ "$result" ]; then
	# do nothing
	true
    elif [ -f "/etc/centos-release" ]; then
	result="centos"
    elif [ -f "/etc/redhat-release" ]; then
	result="redhat"
    else
        result=`uname -s`
    fi
    result=`echo $result | tr '[A-Z]' '[a-z]'`
    if [ "$result" = "linux" ]; then
	result=`head -1 /etc/issue | sed 's/ .*//' | tr '[A-Z]' '[a-z]'`
    fi

    eval $__resultvar="$result"
}
