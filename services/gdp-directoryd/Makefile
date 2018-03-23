all:
	gcc -Wall -o gdp-directoryd gdp-directoryd.c \
		-I/usr/include/mariadb -lmariadb

# invoke from sudo
install:
	systemctl stop gdp-directoryd
	cp gdp-directoryd /usr/sbin/gdp-directoryd
	chown gdp:gdp /usr/sbin/gdp-directoryd
	@ls -lsa /usr/sbin/gdp-directoryd
	systemctl start gdp-directoryd
	systemctl status gdp-directoryd

