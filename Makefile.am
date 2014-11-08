## Process this file with automake to produce Makefile.in

# Copyright 2013 Endless Mobile, Inc.

## -----------
## Makefile.am
## -----------
## Please keep this file well-commented.

# Generated files that 'make clean' should erase
CLEANFILES =
DISTCLEANFILES =

# Extra files to distribute in the tarball
EXTRA_DIST = README.md

# # # INSTALL RULES # # #

bin_PROGRAMS = xapian-bridge

xapian_bridge_SOURCES = \
	src/xb-main.c \
	src/xb-database-cache.h \
	src/xb-database-cache.c \
	src/xb-database-manager.h \
	src/xb-database-manager.c \
	src/xb-error.h \
	src/xb-error.c \
	src/xb-prefix-store.h \
	src/xb-prefix-store.c \
	src/xb-routed-server.h \
	src/xb-routed-server.c \
	src/xb-router.h \
	src/xb-router.c \
	$(NULL)

AM_CPPFLAGS = \
	-DLOCALSTATEDIR=\""$(localstatedir)"\" \
	$(XAPIAN_BRIDGE_CFLAGS) \
	$(NULL)

LDADD = \
	$(XAPIAN_BRIDGE_LIBS) \
	$(NULL)

# # # TESTS # # #

# Run tests when running 'make check'
# TESTS = test
xapian_bridge_js_tests = \
	test/unit/testCache.js \
	test/unit/testPrefixStore.js \
	test/unit/testRouter.js \
	$(NULL)

EXTRA_DIST += \
	$(xapian_bridge_js_tests) \
	$(NULL)

# Run tests when running 'make check'
TESTS = \
	$(xapian_bridge_js_tests) \
	$(NULL)
TEST_EXTENSIONS = .js

# Jasmine support for the tests
JASMINE_SUBMODULE_PATH = $(top_srcdir)/test/jasmine
AM_JS_LOG_FLAGS = \
	--include-path=$(top_srcdir)/src 

include $(JASMINE_SUBMODULE_PATH)/Makefile-jasmine.am.inc
