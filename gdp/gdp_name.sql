-- Schema for the external -> internal log name mapping

--  ----- BEGIN LICENSE BLOCK -----

-- the database is pretty simple....
CREATE DATABASE IF NOT EXISTS gdp_names;
USE gdp_names;
CREATE TABLE IF NOT EXISTS gdp_names (
	xname VARCHAR(255) PRIMARY KEY,
	gname BINARY(32));

-- anonymous user for doing reads
CREATE USER IF NOT EXISTS ''@'%';
GRANT SELECT ON gdp_names TO ''@'%';

-- privileged user for doing updates
-- (should figure out a better way of managing the password)
CREATE USER IF NOT EXISTS 'creation_service'@'%' IDENTIFIED BY 'changeme';
GRANT INSERT ON gdp_names TO 'creation_service'@'%';
