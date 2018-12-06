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
GRANT ALL PRIVILEGES ON blackbox.*
	TO 'gdpr'@'127.0.0.1'
	IDENTIFIED BY 'testblackbox'
	WITH GRANT OPTION;
SELECT user, host FROM mysql.user;

# GRANT ALL PRIVILEGES will automatically create the user if not exists, as if
# CREATE USER 'gdpr'@'127.0.0.1';

# ==============================================================================
