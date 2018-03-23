# GDP-DIRECTORYD

Setup requires installation of mariadb database and mariadb
Connector/C client library on the same host as gdp-directoryd.

After mariadb is installed on the desired "blackbox" host,
configure the database:

$ mysql
Welcome to the MariaDB monitor.  Commands end with ; or \g.
Your MariaDB connection id is 56
Server version: 10.0.31-MariaDB-0ubuntu0.16.04.2 Ubuntu 16.04

Copyright (c) 2000, 2017, Oracle, MariaDB Corporation Ab and others.

Type 'help;' or '\h' for help. Type '\c' to clear the current input statement.

MariaDB [(none)]> CREATE DATABASE blackbox;
Query OK, 1 row affected (0.00 sec)

MariaDB [(none)]> show databases;
+--------------------+
| Database           |
+--------------------+
| blackbox           |
| information_schema |
| mysql              |
| performance_schema |
+--------------------+
4 rows in set (0.00 sec)

#
# NOTE: database is not made internet accessible to minimize security risk
#
MariaDB [(none)]> CREATE USER 'gdpr'@'127.0.0.1';
Query OK, 0 rows affected (0.00 sec)

MariaDB [(none)]> SELECT User, Host FROM mysql.user;
+------+-----------+
| User | Host      |
+------+-----------+
| gdpr | 127.0.0.1 |
| root | localhost |
+------+-----------+
2 rows in set (0.00 sec)

MariaDB [(none)]> USE blackbox;
Reading table information for completion of table and column names
You can turn off this feature to get a quicker startup with -A

Database changed
MariaDB [blackbox]> GRANT ALL PRIVILEGES ON blackbox.* TO 'gdpr'@'127.0.0.1' IDENTIFIED BY 'testblackbox' WITH GRANT OPTION;
Query OK, 0 rows affected (0.00 sec)

MariaDB [blackbox]> CREATE TABLE gdpd ( dguid BINARY(32) NOT NULL, eguid BINARY(32) NOT NULL );
Query OK, 0 rows affected (0.27 sec)

#
# Sample dump of blackbox.gdpd table after adding one entry
#

MariaDB [blackbox]> select hex(dguid), hex(eguid) from blackbox.gdpd;
+------------------------------------------------------------------+------------------------------------------------------------------+
| hex(dguid)                                                       | hex(eguid)                                                       |
+------------------------------------------------------------------+------------------------------------------------------------------+
| DE7429240AA7B4140A4FA1A9105E5B5AE7D0827132F6906334DDDDEB9301A0EE | AF5195931CC107721ED3406C0341D3BDE8E799B8DD392D27A3D6F51CCFE3C69D |
+------------------------------------------------------------------+------------------------------------------------------------------+
1 row in set (0.00 sec)

MariaDB [blackbox]> 
