dnl Process this file with autoconf to produce configure

# Copyright 2013 Endless Mobile, Inc.

dnl ------------
dnl configure.ac
dnl ------------
dnl Please keep this file well-commented. Autotools involve a lot of magical
dnl incantations, and it is easy to mess things up if you don't know what you
dnl are doing.

# Version numbers
# ---------------
# API name and version for pkg-config; increment when breaking API/ABI.
# This version number is also the major version number.
m4_define([_XAPIAN_BRIDGE_API_VERSION_MACRO], [0])

# Minor and micro versions: increment micro version when making a release. Minor
# version is even for a stable release and odd for a development release.
# When making any release, if the API changes, set the interface age to 0.
m4_define([_XAPIAN_BRIDGE_MINOR_VERSION_MACRO], [0])
m4_define([_XAPIAN_BRIDGE_MICRO_VERSION_MACRO], [0])
m4_define([_XAPIAN_BRIDGE_INTERFACE_AGE_MACRO], [0])

# Full version, for use in AC_INIT
m4_define([_XAPIAN_BRIDGE_VERSION_MACRO],
    [_XAPIAN_BRIDGE_API_VERSION_MACRO._XAPIAN_BRIDGE_MINOR_VERSION_MACRO._XAPIAN_BRIDGE_MICRO_VERSION_MACRO])

# Variables to define
# -------------------
# Version numbers and macros
AC_SUBST([XAPIAN_BRIDGE_API_VERSION], _XAPIAN_BRIDGE_API_VERSION_MACRO)
AC_DEFINE([XAPIAN_BRIDGE_MAJOR_VERSION], [_XAPIAN_BRIDGE_API_VERSION_MACRO], [Major (API) version])
AC_SUBST([XAPIAN_BRIDGE_API_NAME], ["xapianbridge-$XAPIAN_BRIDGE_API_VERSION"])
XAPIAN_BRIDGE_MINOR_VERSION=_XAPIAN_BRIDGE_MINOR_VERSION_MACRO
XAPIAN_BRIDGE_MICRO_VERSION=_XAPIAN_BRIDGE_MICRO_VERSION_MACRO
XAPIAN_BRIDGE_INTERFACE_AGE=_XAPIAN_BRIDGE_INTERFACE_AGE_MACRO
AC_DEFINE([XAPIAN_BRIDGE_MINOR_VERSION], [_XAPIAN_BRIDGE_MINOR_VERSION_MACRO], [Minor version])
AC_DEFINE([XAPIAN_BRIDGE_MICRO_VERSION], [_XAPIAN_BRIDGE_MICRO_VERSION_MACRO], [Micro version])
AC_DEFINE([XAPIAN_BRIDGE_VERSION], [_XAPIAN_BRIDGE_VERSION_MACRO], [Full version number])

# Initialization
# --------------
# Initialize Autoconf: package name, version, bug report address, tarball name,
# website
AC_PREREQ(2.63)
AC_INIT([Xapian Bridge], [_XAPIAN_BRIDGE_API_VERSION_MACRO],
    [], [xapian-bridge], [http://endlessm.com])
# Initialize Automake: enable all warnings and do not insist on GNU standards
# no-portability suppresses warnings about syntax specific to GNU make
AM_INIT_AUTOMAKE([-Wall -Wno-portability foreign dist-xz no-dist-gzip 1.9 tar-ustar parallel-tests])
# Avoid spewing garbage over the terminal ('make V=1' to see the garbage)
AM_SILENT_RULES([yes])
# Keep Autotools macros local to this source tree
AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_HEADERS([config.h])
m4_ifdef([AM_SILENT_RULES],[AM_SILENT_RULES([yes])])

AC_CACHE_SAVE

# Required build tools
# --------------------
# Make sure we can create directory hierarchies
AC_PROG_MKDIR_P
AC_PROG_LIBTOOL
PKG_PROG_PKG_CONFIG

AC_CACHE_SAVE

# Required libraries
# ------------------
m4_define(glib_minver, 2.40.0)
PKG_CHECK_MODULES(XAPIAN_BRIDGE, [gio-2.0 >= glib_minver
                                  json-glib-1.0
                                  libsoup-2.4
                                  xapian-glib-1.0])

# systemd units
#--------------
AC_ARG_WITH([systemdsystemunitdir],
            AS_HELP_STRING([--with-systemdsystemunitdir=DIR], [Directory for systemd service files]),
            [],
            [with_systemdsystemunitdir=$($PKG_CONFIG --variable=systemdsystemunitdir systemd)])
AC_SUBST([systemdsystemunitdir], [$with_systemdsystemunitdir])

AC_CACHE_SAVE

# Output
# ------
# List files here that the configure script should output
AC_CONFIG_FILES([
    Makefile
])
# Do the output
AC_OUTPUT
