.PHONY: install-service install-binary install start stop reinstall deinstall

gdp-directoryd:
	gcc -Wall -o gdp-directoryd gdp-directoryd.c \
		-I/usr/include/mariadb -lmariadb \
		-I../../../gdp

install-service:
	sudo cp gdp-directoryd.service /etc/systemd/system/
	sudo systemctl enable gdp-directoryd
	@echo "Installed service files:"
	@ls -lsa /etc/systemd/system/gdp-directoryd.service
	@ls -lsa /etc/systemd/system/multi-user.target.wants/gdp-directoryd.service

install-binary: gdp-directoryd
	sudo cp -p gdp-directoryd /usr/sbin/gdp-directoryd
	sudo chmod 700 /usr/sbin/gdp-directoryd
	sudo chown gdp:gdp /usr/sbin/gdp-directoryd
	@echo "Installed binary:"
	@ls -lsa /usr/sbin/gdp-directoryd

install: install-binary install-service start

all: gdp-directoryd

clean:
	rm gdp-directoryd

status:
	sudo systemctl status gdp-directoryd

start:
	sudo systemctl start gdp-directoryd

stop:
	sudo systemctl stop gdp-directoryd

reinstall: stop install-binary start

deinstall:
	sudo systemctl stop gdp-directoryd
	sudo systemctl disable gdp-directoryd
	sudo rm -f /etc/systemd/system/gdp-directoryd.service
	sudo rm -f /usr/sbin/gdp-directoryd
