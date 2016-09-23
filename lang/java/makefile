# Makefile org.terraswarm Global Data Plane sources
#
# @Version: $Id: makefile 72425 2015-05-27 21:01:46Z cxh $
# @Author: Christopher Brooks
#
# @Copyright (c) 2014-2016 The Regents of the University of California.
# All rights reserved.
#
# Permission is hereby granted, without written agreement and without
# license or royalty fees, to use, copy, modify, and distribute this
# software and its documentation for any purpose, provided that the
# above copyright notice and the following two paragraphs appear in all
# copies of this software.
#
# IN NO EVENT SHALL THE UNIVERSITY OF CALIFORNIA BE LIABLE TO ANY PARTY
# FOR DIRECT, INDIRECT, SPECIAL, INCIDENTAL, OR CONSEQUENTIAL DAMAGES
# ARISING OUT OF THE USE OF THIS SOFTWARE AND ITS DOCUMENTATION, EVEN IF
# THE UNIVERSITY OF CALIFORNIA HAS BEEN ADVISED OF THE POSSIBILITY OF
# SUCH DAMAGE.
#
# THE UNIVERSITY OF CALIFORNIA SPECIFICALLY DISCLAIMS ANY WARRANTIES,
# INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
# MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE. THE SOFTWARE
# PROVIDED HEREUNDER IS ON AN "AS IS" BASIS, AND THE UNIVERSITY OF
# CALIFORNIA HAS NO OBLIGATION TO PROVIDE MAINTENANCE, SUPPORT, UPDATES,
# ENHANCEMENTS, OR MODIFICATIONS.
#
# 						PT_COPYRIGHT_VERSION_2
# 						COPYRIGHTENDKEY
##########################################################################

# To update the GDP version, see ../../gdp/gdp_version.h
# If you change the GDP version, then also update $PTII/configure.in and
# https://www.terraswarm.org/swarmos/wiki/Main/GDPJavaInterface
GDPLIBMAJVER=	0
GDPLIBMINVER=	7
GDP_PATCH_VERSION=0

GDP_MAJOR_MINOR_VERSION=$(GDPLIBMAJVER).$(GDPLIBMINVER)

all: jclass

# JNA will look for the GDP shared library in the classpath.

# https://github.com/java-native-access/jna/blob/master/www/GettingStarted.md
# says that JNA will look in os-arch/library, where os-arch is JNA's
# canonical prefix for the platform.  The shared library is
# automatically extracted.


GDP_JAR=gdp-$(GDP_MAJOR_MINOR_VERSION)-$(GDP_PATCH_VERSION).jar
# The format of the gdp library version must be the same on all
# platforms.  See org/terraswarm/gdp/GdpNLibrary.java, where N is the
# version number.
DARWIN_LIB=libgdp.$(GDP_MAJOR_MINOR_VERSION).dylib
LINUX_LIB=libgdp.$(GDP_MAJOR_MINOR_VERSION).so
LINUX_DASH_LIB=libgdp-$(GDP_MAJOR_MINOR_VERSION).so
LINUX_RHEL_LIB=libgdp.$(GDP_MAJOR_MINOR_VERSION)-rhel.so
install: all darwin/$(DARWIN_LIB) linux-amd64/$(LINUX_LIB) linux-x86-64/$(LINUX_LIB)
	jar cf $(GDP_JAR) org/terraswarm/gdp/*.class darwin linux-amd64 linux-x86-64

darwin/$(DARWIN_LIB): 
	if [ ! -d darwin ]; then mkdir darwin; fi
	if [ -f ../../libs/$(DARWIN_LIB) ]; then \
		echo "Copying ../../libs/$(DARWIN_LIB) to $@"; \
		cp ../../libs/$(DARWIN_LIB) $@; \
	else \
		if [ -f $(PTII)/lib/$(DARWIN_LIB) ]; then \
			echo "Copying $(PTII)/lib/$(DARWIN_LIB) to $@"; \
			cp $(PTII)/lib/$(DARWIN_LIB) $@; \
		else \
			echo "Could not find the Darwin GDP shared library, so the jar file will not contain it."; \
		fi \
	fi

linux-amd64/$(LINUX_LIB):
	if [ ! -d linux-amd64 ]; then mkdir linux-amd64; fi
	if [ -f ../../libs/$(LINUX_DASH_LIB) ]; then \
		echo "Copying ../../libs/$(LINUX_DASH_LIB) to $@"; \
		cp ../../libs/$(LINUX_DASH_LIB) $@; \
	else \
		if [ -f $(PTII)/lib/$(LINUX_RHEL_LIB) ]; then \
			echo "Copying $(PTII)/lib/$(LINUX_RHEL_LIB) to $@"; \
			cp $(PTII)/lib/$(LINUX_RHEL_LIB) $@; \
		else \
			echo "Could not find the Linux GDP RHEL shared library, so the jar file will not contain it."; \
		fi \
	fi

linux-x86-64/$(LINUX_LIB):
	if [ ! -d linux-x86-64 ]; then mkdir linux-x86-64; fi
	if [ -f ../../libs/$(LINUX_DASH_LIB) ]; then \
		echo "Copying ../../libs/$(LINUX_DASH_LIB) to $@"; \
		cp ../../libs/$(LINUX_DASH_LIB) $@; \
	else \
		if [ -f $(PTII)/lib/$(LINUX_LIB) ]; then \
			echo "Copying $(PTII)/lib/$(LINUX_LIB) to $@"; \
			cp $(PTII)/lib/$(LINUX_LIB) $@; \
		else \
			echo "Could not find the Linux GDP non-RHEL shared library, so the jar file will not contain it."; \
		fi \
	fi

##############################
JNA_JAR = jna-4.2.2.jar
JNAERATOR_VERSION = 0.12
JNAERATOR_JAR = jnaerator-$(JNAERATOR_VERSION)-shaded.jar
JNAERATOR_RUNTIME_JAR = jnaerator-runtime-$(JNAERATOR_VERSION).jar

### Download required files. If there is an updated version of these, it needs
### to be manually updated.

$(JNA_JAR):
	wget 'https://maven.java.net/content/repositories/releases/net/java/dev/jna/jna/4.2.2/$(JNA_JAR)' -O $@

$(JNAERATOR_JAR):
	wget 'https://search.maven.org/remotecontent?filepath=com/nativelibs4java/jnaerator/$(JNAERATOR_VERSION)/$(JNAERATOR_JAR)' -O $@

$(JNAERATOR_RUNTIME_JAR):
	wget 'https://search.maven.org/remotecontent?filepath=com/nativelibs4java/jnaerator-runtime/$(JNAERATOR_VERSION)/$(JNAERATOR_RUNTIME_JAR)' -O $@

##############################

## Header files that we need to interpret the compiled library
## Apparently -I has no effect on JNA, and it fails to find event2/buffer.h 
##   from the system default paths


ROOT = ../..
GDP_SRC = $(ROOT)
EVENT_HDRS = $(shell if [ -e /usr/include/event2/buffer.h ]; then echo /usr/include; \
		elif [ -e /usr/local/include/event2/buffer.h ]; then echo /usr/local/include; \
		elif [ -e /opt/local/include/event2/buffer.h ]; then echo /opt/local/include; \
		else echo unknown; fi )
PWD = $(ROOT)/lang/java/

GDP_H_FILES = \
	$(GDP_SRC)/gdp/gdp.h \
	$(GDP_SRC)/gdp/gdp_buf.h \
	$(GDP_SRC)/ep/ep.h \
	$(GDP_SRC)/ep/ep_dbg.h \
	$(GDP_SRC)/ep/ep_thr.h

LIBEVENT_H_FILES = \
    $(EVENT_HDRS)/event2/buffer.h


### Auto-generating java files

# We define gdp_datum_t and gdp_datum as void * so that JNAerator does not generate gdp_datum.java, which would refer to pthreads.
jna: $(JNAERATOR_JAR) $(JNAERATOR_RUNTIME_JAR) $(GDP_H_FILES) $(LIBEVENT_H_FILES)
	java -jar $(JNAERATOR_JAR) -runtime JNA -f \
		-Dgdp_datum_t="void *" -Dgdp_datum="void *" \
		-library gdp.$(GDP_MAJOR_MINOR_VERSION) -I ../../gdp -I ../.. \
		$(GDP_H_FILES) $(LIBEVENT_H_FILES) \
		-o . -library event2 -I/usr/include -I/usr/local/include \
                -package org.terraswarm.gdp  -v  -noJar -noComp
	# Fix up NativeSize. We want our included version of NativeSize be used, instead of JNAerator's version:
	# We use sed -i here, which varies between Mac OS X and RHEL.  However calling sed twice like this works.
	for file in org/terraswarm/gdp/*.java org/terraswarm/gdp/*/*.java; do \
		echo "Fixing $$file"; \
		sed -i -e "s@Gdp[0-9]*Library@Gdp${GDPLIBMAJVER}${GDPLIBMINVER}Library@" $$file; \
		sed -i -e 's@com.ochafik.lang.jnaerator.runtime.NativeSize@org.terraswarm.gdp.NativeSize@' $$file; \
	done
	# Empty target. See https://www.gnu.org/software/make/manual/html_node/Empty-Targets.html
	touch jna


### Compiling .java files to .class files

JSRCS_LIB = $(wildcard org/terraswarm/gdp/*.java)
JSRCS_APPS = $(wildcard org/terraswarm/gdp/apps/*.java)
JSRCS = $(JSRCS_LIB) $(JSRCS_APPS)

# Java rules
CLASSPATH = $(JNA_JAR):$(JNAERATOR_RUNTIME_JAR):$(PWD)

JAVAC = javac

.SUFFIXES: .class .java
.java.class:
	CLASSPATH="$(CLASSPATH):$(PWD)" "$(JAVAC)" $(JFLAGS) $<

JCLASS = $(JSRCS:%.java=%.class)

jclass: jna $(JNA_JAR) $(JSRCS) $(JCLASS)


### apps

JAVA = java
LOG_NAME = logjava
ROUTER_NAME = 172.0.0.1:8007


runCreateTest: all
	DYLD_LIBRARY_PATH=$(GDP_SRC)/libs LD_LIBRARY_PATH=$(GDP_SRC)/libs $(JAVA) -Djna.debug_load=true -Djna.dump_memory=true -classpath $(CLASSPATH) org.terraswarm.gdp.apps.createTest 
runNameTest: all
	DYLD_LIBRARY_PATH=$(GDP_SRC)/libs LD_LIBRARY_PATH=$(GDP_SRC)/libs $(JAVA) -Djna.debug_load=true -Djna.dump_memory=true -classpath $(CLASSPATH) org.terraswarm.gdp.apps.nameTest 
runHelloWorld: all
	DYLD_LIBRARY_PATH=$(GDP_SRC)/libs LD_LIBRARY_PATH=$(GDP_SRC)/libs $(JAVA) -Djna.debug_load=true -Djna.dump_memory=true -classpath $(CLASSPATH) org.terraswarm.gdp.apps.HelloWorld 
#
#runReaderTest: org/terraswarm/gdp/apps/ReaderTest.class
#	DYLD_LIBRARY_PATH=$(GDP_SRC)/libs LD_LIBRARY_PATH=$(GDP_SRC)/libs $(JAVA) -Djna.debug_load=true -Djna.dump_memory=true -classpath $(CLASSPATH) org.terraswarm.gdp.apps.ReaderTest -G $(ROUTER_NAME) -D gdp.api=100 $(LOG_NAME)
#
#runReaderTestPtIILib: org/terraswarm/gdp/apps/ReaderTest.class
#	DYLD_LIBRARY_PATH=$(GDP_SRC)/libs LD_LIBRARY_PATH=$(GDP_SRC)/libs $(JAVA) -Djna.debug_load=true -Djna.dump_memory=true -classpath $(CLASSPATH) org.terraswarm.gdp.apps.ReaderTest -G $(ROUTER_NAME) -D gdp.api=100 $(LOG_NAME)
#
#runReaderTestSubscriber: org/terraswarm/gdp/apps/ReaderTest.class
#	DYLD_LIBRARY_PATH=$(GDP_SRC)/libs LD_LIBRARY_PATH=$(GDP_SRC)/libs $(JAVA) -Djna.debug_load=true -Djna.dump_memory=true -classpath $(CLASSPATH) org.terraswarm.gdp.apps.ReaderTest -G $(ROUTER_NAME) -s -D gdp.api=100 $(LOG_NAME)
#
#runWriterTest: org/terraswarm/gdp/apps/WriterTest.class
#	DYLD_LIBRARY_PATH=$(GDP_SRC)/libs LD_LIBRARY_PATH=$(GDP_SRC)/libs $(JAVA) -Djna.debug_load=true -Djna.dump_memory=true -classpath $(CLASSPATH) org.terraswarm.gdp.apps.WriterTest -G $(ROUTER_NAME) -D gdp.api=100 $(LOG_NAME)
#

### 

clean:
	rm -f jna _* org/terraswarm/gdp/*.class org/terraswarm/gdp/EP* org/terraswarm/gdp/e* org/terraswarm/gdp/*Library.java org/terraswarm/gdp/apps/*.class *.jar
	rm -rf darwin linux-amd64 linux-x86-64

distclean: clean
	rm -f *.jar
