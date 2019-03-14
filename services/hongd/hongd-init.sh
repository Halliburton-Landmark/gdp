#!/bin/sh

#
#  Initialize HONGD Service database
#
#	We're assuming MariaDB here, although MySQL can work.  The issue
#	(as of this writing) is about licenses, not functionality.  That
#	may (probably will) change in the future, since it appears that
#	recent versions of MariaDB have better support for replication.
#

set -e
cd `dirname $0`/../..
: ${GDP_SRC_ROOT:=`pwd`}
cd $GDP_SRC_ROOT
if [ "$GDP_SRC_ROOT" = "/" -a -d "gdp/adm" ]
then
	# running in a Docker container
	GDP_SRC_ROOT="/gdp"
	cd $GDP_SRC_ROOT
fi
. adm/common-support.sh

debug=false
install_mariadb=false
args=`getopt Di $*`

usage() {
	echo "Usage: $0 [-D] [-i]" >&2
	exit $EX_USAGE
}

if [ $? != 0 ]; then
	usage
fi
eval set -- $args
while true
do
	case "$1" in
	  -D)
		debug=true
		;;
	  -i)
		install_mariadb=true
		;;
	  --)
		shift
		break;;
	esac
	shift
done

: ${GDP_ETC:=/etc/gdp}

info "Installing Human-Oriented Name to GDPname Directory Service (HONGD)."

# you should probably be able to import these
creation_service_name="gdp_creation_service"
creation_service_pw_file="${GDP_ETC}/creation_service_pw.txt"
$debug || sudo mkdir -p $GDP_ETC
if [ -r $creation_service_pw_file ]
then
	creation_service_pw=`cat $creation_service_pw_file`
else
	# if we do not have a password, create one and save it
	warn "Creating new password for $creation_service_name"
	creation_service_pw=`dd if=/dev/random bs=1 count=9 2>/dev/null | base64`
	if $debug
	then
		info "New password is $creation_service_pw (in $creation_service_pw_file)"
	else
		(umask 0077; echo "$creation_service_pw" > $creation_service_pw_file)
		info "New password is in $creation_service_pw_file"
	fi
fi

# these need to be well known, at least in your trust domain
gdp_user_name="gdp_user"
gdp_user_pw="gdp_user"

#
#  We need the Fully Qualified Domain Name because MariaDB/MySQL uses
#  it for authentication.  Unfortunately some systems require several
#  steps to set it properly, so often it is left unqualified.  We do
#  what we can.
#
set_fqdn() {
	fqdn=`hostname -f`
	case "$fqdn" in
	    *.*)
		# hostname is fully qualified (probably)
		return 0
		;;
	    "")
		fatal "Hostname not set --- cannot proceed."
		;;
	    *)
		warn "Cannot find domain name for host $fqdn."
		warn "Suggest adjusting /etc/hosts on your system."
		return 1
		;;
	esac
}


#
#  Install appropriate packages for MariaDB.  On some systems this can
#  require additional operations to make sure the package is current.
#
install_mariadb_packages() {
	info "Installing required packages"
	case "$OS" in
	   "ubuntu" | "debian" | "raspbian")
		sudo apt-get update
		sudo apt-get clean
		package mariadb-server
		;;

	   "darwin")
		sudo port selfupdate
		: ${GDP_MARIADB_VERSION:="10.3"}
		package mariadb-${GDP_MARIADB_VERSION}-server
		sudo port select mysql mariadb-$GDP_MARIADB_VEFRSION
		sudo port load mariadb-${GDP_MARIADB_VERSION}-server
		;;

	   "freebsd")
		sudo pkg update
		: ${GDP_MARIADB_VERSION:="103"}
		package mariadb${GDP_MARIADB_VERSION}-server
		package base64
		;;

	   *)
		fatal "%0: unknown OS $OS"
		;;
	esac
}


# needs to be customized for other OSes
control_service() {
	cmd=$1
	svc=$2
	case "$OS" in
	  "ubuntu" | "debian" | "raspbian")
		sudo -s service $cmd $svc
		;;
	  *)
		fatal "%0: unknown OS $OS"
		;;
	esac
}


#
#  Read a new password.
#  Uses specific prompts.
#
read_new_password() {
	local var=$1
	local prompt="${2:-new password}"
	local passwd
	read_passwd passwd "Enter $prompt"
	local passwd_compare
	read_passwd passwd_compare "Re-enter $prompt"
	if [ "$passwd" != "$passwd_compare" ]
	then
		error "Sorry, passwords must match"
		return 1
	fi
	eval "${var}=\$passwd"
	return 0
}


#
#  This sets up the Human-GDP name database.  If necessary it will
#  try to set up the MariaDB system schema using initialize_mariadb.
#  It should be OK to call this even if HONGD database is already
#  set up, but it will prompt you for a password that won't be needed.
#
create_hongd_db() {
	# determine if mariadb or mysql are already up and running
	if ps -alx | grep mysqld | grep -vq grep
	then
		# it looks like a server is running
		warn "It appears MySQL or MariaDB is already running; I'll use that."
	else
		# apparently nothing running
		info "Starting up MariaDB/MySQL"
		$debug || control_service start mysql
	fi

	info "Setting up Human-Oriented Name to GDPname Directory database."
	hongd_sql=`cat << XYZZY
		-- Schema for the external -> internal log name mapping

		-- The database is pretty simple....
		CREATE DATABASE IF NOT EXISTS gdp_hongd
			DEFAULT CHARACTER SET 'utf8';
		USE gdp_hongd;
		CREATE TABLE IF NOT EXISTS human_to_gdp (
			hname VARCHAR(255) PRIMARY KEY,
			gname BINARY(32));

		-- Minimally privileged user for doing reads, well known
		-- password.  Anonymous users kick out too many warnings.
		CREATE USER IF NOT EXISTS '$gdp_user_name'@'%'
			IDENTIFIED BY '$gdp_user_pw';
		GRANT SELECT (hname, gname)
			ON human_to_gdp
			TO '$gdp_user_name'@'%';

		-- Privileged user for doing updates
		-- (should figure out a better way of managing the password)
		CREATE USER IF NOT EXISTS '$creation_service_name'@'%'
			IDENTIFIED BY '$creation_service_pw';
		GRANT SELECT, INSERT
			ON human_to_gdp
			TO '$creation_service_name'@'%';

		-- Administrative role
		CREATE ROLE IF NOT EXISTS 'gdp_admin';

		-- Convenience script to query service
		DELIMITER //
		CREATE OR REPLACE PROCEDURE
			hname2gname(pat VARCHAR(255))
		  BEGIN
			SELECT hname, HEX(gname)
			FROM human_to_gdp
			WHERE hname LIKE IFNULL(pat, '%');
		  END //
		DELIMITER ;
		GRANT EXECUTE ON PROCEDURE hname2gname TO 'gdp_admin';
XYZZY
	`
	pwfile=`basename "$creation_service_pw_file"`
	if $debug
	then
		info "Will run:"
		echo "$hongd_sql"
	fi
	if $debug ||  echo "$hongd_sql" | sudo mysql
	then
		if ! $debug
		then
			(umask 0137 && echo $creation_service_pw > $creation_service_pw_file)
			sudo chown gdp:gdp $creation_service_pw_file
		fi
		action "Copy $creation_service_pw_file"
		action "  to /etc/gdp/$pwfile"
		action "  on the system running the log creation service."
		action "  It should be owned by gdp:gdp, mode 640."
	else
		fatal "Unable to initialize HONGD database."
	fi
}


#
#  Now is the time to make work actually happen.
#

set_fqdn
$debug && echo fqdn = $fqdn
$install_mariadb && install_mariadb_packages
create_hongd_db

info "Please read the following instructions."

echo $echo_n "${Bla}${On_Whi}$echo_c"
cat <<- XYZZY

	All GDP client hosts that want to use Human-Oriented Names (hint: this will
	be almost all of them) need to have a pointer to this service in their
	runtime GDP configuration.  This will normally be in /etc/gdp/params/gdp
	or /usr/local/etc/gdp/params/gdp.  There should be a line in that
	file that reads:
	   swarm.gdp.hongdb.host=$fqdn
	Everything else should be automatic.

	We have plans to improve this in the future.
XYZZY
echo ${Reset}
info "Thank you for your attention."
