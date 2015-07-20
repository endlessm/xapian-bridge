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

generate-dbs: generate-test-db
	$(top_builddir)/generate-test-db
MAINTAINERCLEANFILES = test/testdb

# Specify a non-system dir for systemd units at distcheck
AM_DISTCHECK_CONFIGURE_FLAGS = \
	--with-systemdsystemunitdir=$$dc_install_base/$(systemdsystemunitdir) \
	$(NULL)

data/xapian-bridge.service: data/xapian-bridge.service.in Makefile
	$(MKDIR_P) $(top_builddir)/data
	$(AM_V_GEN) sed -e 's|@bindir[@]|$(bindir)|g' $< > $@
CLEANFILES += data/xapian-bridge.service
EXTRA_DIST += \
	data/xapian-bridge.service.in \
	data/xapian-bridge.socket \
	$(NULL)

systemdservicedir = $(systemdsystemunitdir)
systemdservice_DATA = \
	data/xapian-bridge.service \
	data/xapian-bridge.socket \
	$(NULL)

# # # CODE COVERAGE # # #

EOS_COVERAGE_BLACKLIST_PATTERNS = */test/*

@EOS_COVERAGE_RULES@

# # # INSTALL RULES # # #

bin_PROGRAMS = xapian-bridge

xapian_bridge_SOURCES = \
	src/xb-main.c \
	src/xb-database-manager.h \
	src/xb-database-manager.c \
	src/xb-error.h \
	src/xb-error.c \
	src/xb-routed-server.h \
	src/xb-routed-server.c \
	src/xb-router.h \
	src/xb-router.c \
	$(NULL)

AM_CPPFLAGS = \
	-DLOCALSTATEDIR=\""$(localstatedir)"\" \
	$(XAPIAN_BRIDGE_CFLAGS) \
	@EOS_C_COVERAGE_CFLAGS@ \
	$(NULL)

LDADD = \
	$(XAPIAN_BRIDGE_LIBS) \
	@EOS_C_COVERAGE_LDFLAGS@ \
	$(NULL)

# # # TESTS # # #
TESTS_ENVIRONMENT = \
	export G_TEST_SRCDIR=$(abs_srcdir); \
	export G_TEST_BUILDDIR=$(abs_builddir); \
	$(NULL)

noinst_PROGRAMS = \
	generate-test-db \
	test-daemon \
	test-database-manager \
	test-router \
	$(NULL)

TEST_CPPFLAGS = \
	-I$(top_srcdir)/src \
	$(XAPIAN_BRIDGE_CFLAGS) \
	$(NULL)

TEST_LIBS = \
	$(XAPIAN_BRIDGE_LIBS) \
	$(NULL)

generate_test_db_SOURCES = \
	test/generate-test-db.c \
	$(NULL)
generate_test_db_CPPFLAGS = $(TEST_CPPFLAGS)
generate_test_db_LDADD = $(TEST_LIBS)

test_router_SOURCES = \
	test/test-router.c \
	src/xb-router.h \
	src/xb-router.c \
	$(NULL)
test_router_CPPFLAGS = $(TEST_CPPFLAGS)
test_router_LDADD = $(TEST_LIBS)

test_database_manager_SOURCES = \
	test/test-database-manager.c \
	test/test-util.h \
	test/test-util.c \
	src/xb-database-manager.h \
	src/xb-database-manager.c \
	src/xb-error.h \
	src/xb-error.c \
	$(NULL)
test_database_manager_CPPFLAGS = $(TEST_CPPFLAGS)
test_database_manager_LDADD = $(TEST_LIBS)

test_daemon_SOURCES = \
	test/test-daemon.c \
	test/test-util.h \
	test/test-util.c \
	$(NULL)
test_daemon_CPPFLAGS = $(TEST_CPPFLAGS)
test_daemon_LDADD = $(TEST_LIBS)

# Run tests when running 'make check'
TESTS = \
	test-daemon \
	test-database-manager \
	test-router \
	$(NULL)

EXTRA_DIST += \
	test/testdb \
	$(NULL)

# # # CLEAN RULES # # #
# See http://www.gnu.org/softare/automake/manual/html_node/Clean.html
clean-local: clean-coverage

.PHONY: generate-dbs
