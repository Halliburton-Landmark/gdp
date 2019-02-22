FROM mariadb:10.4

# assigned password will appear in log, but all options are poor. Change after? 
ENV MYSQL_RANDOM_ROOT_PASSWORD true

# this database declaration instigates setup of mariadb automatically
ENV MYSQL_DATABASE gdp_rib

RUN apt-get update && apt-get install -y \
	libmariadb3 mariadb-plugin-oqgraph libssl1.0.0

# front-end binary
COPY ./gdp-ribd /

# .sql file to set up gdpr@127.0.0.1 for gdp-ribd access
COPY ./01-gdp-ribd.sql /docker-entrypoint-initdb.d/

# script to launch gdp-ribd front-end binary
COPY ./02-gdp-ribd-launch.sh /docker-entrypoint-initdb.d/
