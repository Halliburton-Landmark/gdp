VER=		latest

all: gdp-src-base gdp-dev-c gdp-run-base gdplogd

# image containing source code and prerequisites
gdp-src-base:	.FORCE
	docker build --target gdp-src-base -t gdp-src-base:${VER} .

# image for application development in C
gdp-dev-c:	.FORCE
	docker build --target gdp-dev-c -t gdp-dev-c:${VER} .

# image for application development in Python
gdp-dev-python:	.FORCE
	docker build --target gdp-dev-python -t gdp-dev-python:${VER} .

# image for application execution (minimal OS, base for other images)
gdp-run-base:	.FORCE
	docker build --target gdp-run-base -t gdp-run-base:${VER} .

# image for GDP log server
gdplogd:	.FORCE
	docker build --target gdplogd -t gdplogd:${VER} .

# image for GDP router
gdp_router:	.FORCE
	docker build --target gdp_router -t gdp_router:${VER} .

.FORCE:
