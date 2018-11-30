VER=		latest
BRANCH=		master
DOCKER=		docker
DOCKERFLAGS=

ALL=		 gdp-src-base gdp-dev-c gdp-run-base gdplogd

all: docker-build

docker-build: ${ALL}

#
#  Build individual images for external use.
#	Intermediate images are build automagically.
#

# image containing source code and prerequisites
gdp-src-base:	.FORCE
	${DOCKER} ${DOCKERFLAGS} build --target gdp-src-base \
		--build-arg BRANCH=${BRANCH} \
		-t gdp-src-base:${VER} .

# image for application development in C
gdp-dev-c:	.FORCE
	${DOCKER} ${DOCKERFLAGS} build --target gdp-dev-c \
		-t gdp-dev-c:${VER} .

# image for application development in Python
gdp-dev-python:	.FORCE
	${DOCKER} ${DOCKERFLAGS} build --target gdp-dev-python \
		-t gdp-dev-python:${VER} .

# image for application execution (minimal OS, base for other images)
gdp-run-base:	.FORCE
	${DOCKER} ${DOCKERFLAGS} build --target gdp-run-base \
		-t gdp-run-base:${VER} .

# image for GDP log server
gdplogd:	.FORCE
	${DOCKER} ${DOCKERFLAGS} build --target gdplogd \
		-t gdplogd:${VER} .

# image for GDP router
gdp_router:	.FORCE
	${DOCKER} ${DOCKERFLAGS} build --target gdp_router \
		-t gdp_router:${VER} .

#
#  Push images to our registry
#	Should only push those images appropriate for external use.
#	Assumes the REGISTRY_USER is logged in (see docker login --help
#		and ~/.docker/config.json).
#
REGISTRY_HOST=	gdp.cs.berkeley.edu:5005
REGISTRY_USER=	eric
REGISTRY=	${REGISTRY_HOST}/${REGISTRY_USER}

docker-pull:
	${DOCKER} pull ubuntu:16.04
	${DOCKER} pull alpine:latest

docker-push: ${ALL}
	for image in ${ALL}; do \
		echo "Pushing $${image}"; \
		${DOCKER} tag $${image} ${REGISTRY}/$${image}:${VER}; \
		${DOCKER} push ${REGISTRY}/$${image}:${VER}; \
	done

.FORCE:
