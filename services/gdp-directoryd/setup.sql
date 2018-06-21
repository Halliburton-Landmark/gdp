# ==============================================================================
# Usage: sudo mysql -v --show-warnings < setup.sql
# ==============================================================================

#
# directory daemon expects this database name
#
create database if not exists blackbox;
show databases;

#
# directory daemon expects this database user, identified by '<password>'
# (directory daemon is co-located, database is only accessible from localhost)
#
# PRODUCTION NOTE: change 'testblackbox' here and in the directory daemon source
# to some suitable secret ahead of build and installation.
#
grant all privileges on blackbox.* TO 'gdpr'@'127.0.0.1' identified by 'testblackbox' with grant option;
select user, host from mysql.user;

# grant all privileges will automatically create the user if not exists, as if
# create user 'gdpr'@'127.0.0.1';

# ==============================================================================
