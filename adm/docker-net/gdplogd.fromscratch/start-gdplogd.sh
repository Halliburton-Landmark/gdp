#!/usr/bin/env sh

set -e
/usr/sbin/gdplogd2 -G $GDP_ROUTER -N $GDPLOGD_NAME $@
