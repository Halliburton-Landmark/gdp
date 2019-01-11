VER=		latest
BRANCH=		default
DOCKER=		docker
DOCKERFLAGS=

ALL=		 gdp-src-base gdp-dev-c gdp-run-base gdplogd

all: docker-build

docker-build: ${ALL}

#
#  Build individual images for external use.
#	Intermediate images are built automagically.
#

# image containing source code and prerequisites
gdp-src-base:	.FORCE
	${DOCKER} build ${DOCKERFLAGS} --target gdp-src-base \
		--build-arg BRANCH=${BRANCH} \
		--build-arg VER=${VER} \
		-t gdp-src-base:${VER} .

# image for application development in C
gdp-dev-c:	.FORCE
	${DOCKER} build ${DOCKERFLAGS} --target gdp-dev-c \
		-t gdp-dev-c:${VER} .

# image for application development in Python
gdp-dev-python:	.FORCE
	${DOCKER} build ${DOCKERFLAGS} --target gdp-dev-python \
		-t gdp-dev-python:${VER} .

# image for application execution (minimal OS, base for other images)
gdp-run-base:	.FORCE
	${DOCKER} build ${DOCKERFLAGS} --target gdp-run-base \
		-t gdp-run-base:${VER} .

# image for GDP log server
gdplogd:	.FORCE
	${DOCKER} build ${DOCKERFLAGS} --target gdplogd \
		-t gdplogd:${VER} .

# image for GDP router
gdp_router:	.FORCE
	${DOCKER} build ${DOCKERFLAGS} --target gdp_router \
		-t gdp_router:${VER} .

#
#  Push images to our registry
#	Should only push those images appropriate for external use.
#	Assumes the DOCKER_REG_USER is logged in (see docker login --help
#		and ~/.docker/config.json).
#
DOCKER_REG_HOST=	gdp.cs.berkeley.edu:5005
DOCKER_REG_USER=	gdp
DOCKER_REG=		${DOCKER_REG_HOST}/${DOCKER_REG_USER}
UBUNTU_VER=		16.04
ALPINE_VER=		latest

docker-pull:
	${DOCKER} pull ubuntu:${UBUNTU_VER}
	${DOCKER} pull alpine:${ALPINE_VER}

docker-push: ${ALL}
	for image in ${ALL}; do \
		echo "Pushing $${image}"; \
		${DOCKER} tag $${image} ${DOCKER_REG}/$${image}:${VER}; \
		${DOCKER} push ${DOCKER_REG}/$${image}:${VER}; \
	done

.FORCE:
