gdc-test:
	gcc -Wall -o gdc-test gdp-directory-client.c \
		-I../../../../gdp ../../../../gdp/libs/libgdp.a \
		-I../../../../ep ../../../../gdp/libs/libep.a \
		-lpthread -lcrypto

all: gdc-test

clean:
	rm gdc-test
