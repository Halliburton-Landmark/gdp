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

MariaDB [blackbox]> CREATE TABLE gdpd ( dguid BINARY(32) NOT NULL, eguid BINARY(32) NOT NULL, ts TIMESTAMP );
Query OK, 0 rows affected (0.20 sec)

#
# Sample dump of blackbox.gdpd table after adding one entry
#

MariaDB [blackbox]> select hex(dguid), hex(eguid), ts from blackbox.gdpd;
Empty set (0.00 sec)

MariaDB [blackbox]> select hex(dguid), hex(eguid), ts from blackbox.gdpd;
+------------------------------------------------------------------+------------------------------------------------------------------+---------------------+
| hex(dguid)                                                       | hex(eguid)                                                       | ts                  |
+------------------------------------------------------------------+------------------------------------------------------------------+---------------------+
| D65A93F45F8C7FA3AD37E88D565DAD3214151CA8F8491734ECA0662CA44A6C74 | DDC128C2064AC6335EF450C31E7B9A1794329CA53007469A67C1B801EBC96C2E | 2018-05-09 13:40:51 |
+------------------------------------------------------------------+------------------------------------------------------------------+---------------------+
1 row in set (0.00 sec)

#
# delete rows after test runs
#
MariaDB [blackbox]> delete from blackbox.gdpd;
Query OK, 1 row affected (0.09 sec)
