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

# Common substitution script for generated files
do_subst = sed \
	-e 's|@pkglibdir[@]|$(pkglibdir)|g' \
	-e 's|@pkgdatadir[@]|$(pkgdatadir)|g' \
	-e 's|@localstatedir[@]|$(localstatedir)|g' \
	$(NULL)

xapian-bridge: xapian-bridge.in Makefile
	rm -f $@ $@.tmp && \
	$(do_subst) $< >$@.tmp && \
	chmod +x $@.tmp && \
	mv $@.tmp $@

bin_SCRIPTS = xapian-bridge
CLEANFILES += xapian-bridge
EXTRA_DIST += xapian-bridge.in

# Generate a src/config.js file with installed directory locations
src/config.js: src/config.js.in Makefile
	$(AM_V_GEN)mkdir -p src && \
	rm -f $@ $@.tmp && \
	$(do_subst) $< >$@.tmp && \
	mv $@.tmp $@

CLEANFILES += src/config.js
EXTRA_DIST += src/config.js.in 

js_sources = \
	src/config.js \
	src/databaseCache.js \
	src/databaseManager.js \
	src/router.js \
	src/routedServer.js \
	src/xapianBridge.js \
	$(NULL)

EXTRA_DIST += $(js_sources)
nobase_pkgdata_DATA = $(js_sources)

# # # TESTS # # #

# Run tests when running 'make check'
# TESTS = test
xapian_bridge_js_tests = \
	test/unit/testCache.js \
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
