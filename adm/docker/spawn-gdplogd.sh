#!/bin/sh

#
#  Spawn a gdplogd container.
#
#	This is run on the host OS, not in the container.
#	It arranges to run the container with appropriate defaults.
#
#	NOTE: This does not work with bash, which "helpfully" tries
#	to escape quotes, but gets it wrong.  Fortunately, the default
#	shell on Debian is dash, which is better, although even dash
#	gets it wrong, which is why we individually export envars
#	and then tell docker to pick them up from the environment.
#

args=`getopt D $*`
test $? != 0 && echo "Usage: $0 [-D]" >&2 && exit 1
set -- $args
debug=false
: ${VER:=latest}
for arg
do
	case "$arg" in
	  -D)
		debug=true
		;;
	  --)
		shift
		break;;
	esac
	shift
done

#  This parses the same files that would be used if the daemon were not
#  running in a container and passes them into the container using
#  environment variables.  It only handles a small set of important
#  variables.
paramfiles=`cat <<-EOF
	/etc/ep_adm_params/gdp
	/etc/ep_adm_params/gdplogd
	/usr/local/etc/ep_adm_params/gdp
	/usr/local/etc/ep_adm_params/gdplogd
	$HOME/.ep_adm_params/gdp
	$HOME/.ep_adm_params/gdplogd
EOF
`

# drop comments, change dots in names to underscores, clean up syntax.
# need to repeat one of the patterns because the semantics of global
# substitute means it won't retry overlapping patterns.
tmpfile=/tmp/$$
sed \
	-e "/^#/d" \
	-e "s/\.\([^.=]*\)=/_\1=/" \
	-e "s/\.\([^.=]*\)=/_\1=/" \
	-e "s/\.\([^.=]*\)=/_\1=/" \
	-e "s/\.\([^.=]*\)=/_\1=/" \
	-e "s/\.\([^.=]*\)=/_\1=/" \
	-e "s/\.\([^.=]*\)=/_\1=/" \
	-e "s/\.\([^.=]*\)=/_\1=/" \
	-e "s/\.\([^.=]*\)=/_\1=/" \
	-e "s/ *= */=\"/" \
	-e "s/$/\"/" \
		$paramfiles > $tmpfile 2>/dev/null
unset paramfiles
. $tmpfile
rm $tmpfile

# fixed arguments: data (log) location
: ${swarm_gdplogd_log_dir:=/var/swarm/gdp/glogs}
args="-v $swarm_gdplogd_log_dir:/var/swarm/gdp/glogs"

# name of log server
if [ -z "$swarm_gdplogd_gdpname" ]; then
	# make up a name based on our host name
	swarm_gdplogd_gdpname=`hostname --fqdn | \
				tr '.' '\n' | \
				tac | \
				tr '\n' '.' | \
				sed 's/\.$//'`
fi
export GDPLOGD_NAME="${swarm_gdplogd_gdpname}"
args="$args -e GDPLOGD_NAME"

# optional argument: routers
export GDP_ROUTER="${swarm_gdp_routers}"
test -z "${swarm_gdp_routers}" || args="$args -e GDP_ROUTER"

# name of Human-Oriented Name to GDPname Directory server (IP address)
export GDP_HONGD_SERVER="$swarm_gdp_namedb_host"
test -z "$swarm_gdp_namedb_host" || args="$args -e GDP_HONGD_SERVER"

# additional arguments to gdplogd itself (passed in environment)
args="$args -e GDPLOGD_ARGS"

if $debug; then
	echo "=== Environment ==="
	env
	echo "=== Command Line Args ==="
	echo "$*"
	echo "=== Command ==="
	echo "exec docker run $args gdplogd:$VER $*"
else
	exec docker run $args gdplogd:$VER $*
fi
