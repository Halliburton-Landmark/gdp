If this service is ever deployed for real, might be wise to test against a
test table rather than the real table, to avoid disturbing the real table.

Please follow ../README.md to start gdp-directoryd with an appropriate
db set up. Once installed, this binary can be used (locally or
remotely) to test the gdp directory service (test cguid is hardwired
0xc1c1c1...):

Start with empty table:

MariaDB [blackbox]> select hex(dguid),hex(eguid) from blackbox.gdpd;
Empty set (0.01 sec)

MariaDB [blackbox]>

rpratt@gdp-04[73] ./gdc-test add edu.berkeley.eecs.resource.1 edu.berkeley.eecs.gdpr-02
-> dguid [de7429240aa7b4140a4fa1a9105e5b5ae7d0827132f6906334ddddeb9301a0ee]
-> eguid [af5195931cc107721ed3406c0341d3bde8e799b8dd392d27a3d6f51ccfe3c69d]
<- dguid [de7429240aa7b4140a4fa1a9105e5b5ae7d0827132f6906334ddddeb9301a0ee]
<- eguid [af5195931cc107721ed3406c0341d3bde8e799b8dd392d27a3d6f51ccfe3c69d] ack
rpratt@gdp-04[74] ./gdc-test find edu.berkeley.eecs.resource.1
-> dguid [de7429240aa7b4140a4fa1a9105e5b5ae7d0827132f6906334ddddeb9301a0ee]
-> cguid [c1c1c1c1c1c1c1c1c1c1c1c1c1c1c1c1c1c1c1c1c1c1c1c1c1c1c1c1c1c1c1c1]
<- dguid [de7429240aa7b4140a4fa1a9105e5b5ae7d0827132f6906334ddddeb9301a0ee]
<- eguid [af5195931cc107721ed3406c0341d3bde8e799b8dd392d27a3d6f51ccfe3c69d] ack
<- cguid [c1c1c1c1c1c1c1c1c1c1c1c1c1c1c1c1c1c1c1c1c1c1c1c1c1c1c1c1c1c1c1c1] ack
rpratt@gdp-04[75] ./gdc-test remove edu.berkeley.eecs.resource.1 edu.berkeley.eecs.gdpr-02
-> dguid [de7429240aa7b4140a4fa1a9105e5b5ae7d0827132f6906334ddddeb9301a0ee]
-> eguid [af5195931cc107721ed3406c0341d3bde8e799b8dd392d27a3d6f51ccfe3c69d]
<- dguid [de7429240aa7b4140a4fa1a9105e5b5ae7d0827132f6906334ddddeb9301a0ee]
<- eguid [af5195931cc107721ed3406c0341d3bde8e799b8dd392d27a3d6f51ccfe3c69d] ack
rpratt@gdp-04[76] ./gdc-test find edu.berkeley.eecs.resource.1
-> dguid [de7429240aa7b4140a4fa1a9105e5b5ae7d0827132f6906334ddddeb9301a0ee]
-> cguid [c1c1c1c1c1c1c1c1c1c1c1c1c1c1c1c1c1c1c1c1c1c1c1c1c1c1c1c1c1c1c1c1]
<- eguid nak
rpratt@gdp-04[77] 

MariaDB [blackbox]> select hex(dguid),hex(eguid) from blackbox.gdpd;
Empty set (0.01 sec)

MariaDB [blackbox]>
