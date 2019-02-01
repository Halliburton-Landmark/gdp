gdp-nhop:  gdp-directory-client.c ../gdp-directoryd.h
	gcc -Wall -o gdp-nhop gdp-directory-client.c \
		-I../../../../gdp ../../../../gdp/libs/libgdp.a \
		-I../../../../ep ../../../../gdp/libs/libep.a \
		-lmariadb -lpthread -lcrypto

all: gdp-nhop

clean:
	rm gdp-nhop
