Please follow ../README.md to start gdp-directoryd with an appropriate db set up. Once installed, this binary can be used (locally or remotely) to test the gdp directory service:

gdp-04[286] ./gdc-test add edu.berkeley.eecs.resource.1 edu.berkeley.eecs.gdpr-02
dguid is [de7429240aa7b4140a4fa1a9105e5b5ae7d0827132f6906334ddddeb9301a0ee]
eguid is [af5195931cc107721ed3406c0341d3bde8e799b8dd392d27a3d6f51ccfe3c69d]
gdp-04[287] ./gdc-test lookup edu.berkeley.eecs.resource.1
dguid is [de7429240aa7b4140a4fa1a9105e5b5ae7d0827132f6906334ddddeb9301a0ee]
eguid is [af5195931cc107721ed3406c0341d3bde8e799b8dd392d27a3d6f51ccfe3c69d]
gdp-04[288] ./gdc-test remove edu.berkeley.eecs.resource.1 edu.berkeley.eecs.gdpr-02
dguid is [de7429240aa7b4140a4fa1a9105e5b5ae7d0827132f6906334ddddeb9301a0ee]
eguid is [af5195931cc107721ed3406c0341d3bde8e799b8dd392d27a3d6f51ccfe3c69d]
gdp-04[289] ./gdc-test lookup edu.berkeley.eecs.resource.1
dguid is [de7429240aa7b4140a4fa1a9105e5b5ae7d0827132f6906334ddddeb9301a0ee]
eguid not found
gdp-04[290] 
