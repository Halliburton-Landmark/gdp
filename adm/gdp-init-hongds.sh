#!/bin/sh

#
#  Set up Human-Oriented Name to GDPname Directory Service (HONGDS)
#
#	We're assuming MariaDB here, although MySQL can work.  The issue
#	(as of this writing) is about licenses, not functionality.  That
#	may (probably will) change in the future, since it appears that
#	recent versions of MariaDB have better support for replication.
#

debug=false
install_mariadb=false
args=`getopt Di $*`
if [ $? != 0 ]; then
	echo "Usage: $0 [-D] [-i]" >&2
	exit 64
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

set -e
cd `dirname $0`/..
root=`pwd`
. $root/adm/common-support.sh

info "Installing Human-Oriented Name to GDPname Directory Service (HONGD)."

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
	info "Installing MariaDB packages"
	case "$OS" in
	   "ubuntu" | "debian" | "raspbian")
		sudo apt-get update
		sudo apt-get clean
		package mariadb-server
		;;

	   "darwin")
		sudo port selfupdate
		: ${GDP_MARIADB_VERSION:="10.2"}
		package mariadb-${GDP_MARIADB_VERSION}-server
		sudo port select mysql mariadb-$GDP_MARIADB_VEFRSION
		sudo port load mariadb-${GDP_MARIADB_VERSION}-server
		;;

	   "freebsd")
		sudo pkg update
		: ${GDP_MARIADB_VERSION:="102"}
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
	info "Creating and populating HONGD database"

	# determine if mariadb or mysql are already up and running
	if ps -alx | grep mysqld | grep -vq grep
	then
		# it looks like a server is running
		warn "It appears MySQL or MariaDB is already running; I'll use that."
	else
		# apparently nothing running
		info "Starting up MariaDB/MySQL"
		control_service start mysql
	fi

	info "Setting up Human-Oriented Name to GDPname Directory database."
#	info "This will ask for a password for the Log Creation Service."
#	local pw
#	while ! read_new_password pw
#	do
#		error "Try again"
#	done
	creation_service_name="gdp_creation_service"
	creation_service_password=`dd if=/dev/random bs=1 count=9 | base64`
	gdp_user_name="gdp_user"
	gdp_user_password="gdp_user"
	hongd_sql=$root/adm/gdp-hongd.sql.template
	if sed \
		-e "s@CREATION_SERVICE_NAME@$creation_service_name" \
		-e "s@CREATION_SERVICE_PASSWORD@$creation_service_password" \
		-e "s@GDP_USER_NAME@$gdp_user_name" \
		-e "s@GDP_USER_PASSWORD@$gdp_user_password" \
		$hongd_sql | sudo mysql
	then
		pwfile=creation_service_pw.txt
		(umask 0137 && echo $creation_service_password > $pwfile)
		sudo chown gdp:gdp $pwfile
		action "Copy $pwfile to /etc/gdp/$pwfile"
		action "  on the system running the log creation service."
		action "  It should be owned by gdp:gdp, mode 640."
	else
		error "Unable to initialize HONGD database."
	fi
}


#
#  Now is the time to make work actually happen.
#

set_fqdn
$debug && echo fqdn = $fqdn
$skip_install && install_mariadb_packages
create_hongd_db

info "Please read the following instructions."

echo $echo_n "${Bla}${On_Whi}$echo_c"
cat <<- EOF

	All GDP client hosts that want to use Human-Oriented Names (hint: this will
	be almost all of them) need to have a pointer to this service in their
	runtime GDP configuration.  This will normally be in /etc/ep_adm_params/gdp
	or /usr/local/etc/ep_adm_params/gdp.  There should be a line in that
	file that reads:
	   swarm.gdp.hongdb.host=$fqdn
	Everything else should be automatic.

	We have plans to improve this in the future.
EOF
echo ${Reset}
info "Thank you for your attention."


#------------------------#  CUT HERE  #------------------------#
exit 0


#
#  After MariaDB (or MySQL) is installed, the system databases need
#  to be set up, including setting a root password.  If the database
#  has already been set up, this fails in confusing ways, so it's
#  best not to call it unless you know it's needed.
#
#  See also `mysql_secure_installation` (part of the MariaDB and
#  MySQL distributions).
#
initialize_mariadb() {
	warn "Attempting to initialize MariaDB."
	info "You will need to create a new root password for the database."
	info "This is not the same as your system root password."
	while ! read_new_password root_passwd
	do
		echo "Try again -- Enter new root password for database"
	done
	info "Be sure you save this password!!"
	: ${mariadb_user:=mysql}
	mysql_install_db --user=$mariadb_user

	# default root accounts are set up for both localhost and $fqdn
	mysqladmin --user=root --host=localhost password "$root_passwd"
	mysqladmin --user=root --host=$fqdn password "$root_passwd"
}
