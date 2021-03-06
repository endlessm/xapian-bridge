dnl Copyright 2014-2015 Endless Mobile, Inc.
dnl
dnl Permission is hereby granted, free of charge, to any person obtaining a copy
dnl of this software and associated documentation files (the "Software"), to
dnl deal in the Software without restriction, including without limitation the
dnl rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
dnl sell copies of the Software, and to permit persons to whom the Software is
dnl furnished to do so, subject to the following conditions:
dnl
dnl The above copyright notice and this permission notice shall be included in
dnl all copies or substantial portions of the Software.
dnl
dnl THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
dnl IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
dnl FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
dnl AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
dnl LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
dnl FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
dnl IN THE SOFTWARE.
dnl
dnl For the avoidance of doubt, the above license terms apply to works generated
dnl as a result of using this file.
dnl
dnl Macros to check for code coverage support
dnl
dnl If you wish to use the JavaScript coverage function, Makefile-jasmine.am.inc
dnl should be included before calling EOS_COVERAGE_RULES.
dnl
dnl Add clean-coverage to the clean-local target in your Makefile to get the
dnl clean rules for the coverage data. Or, if clean-local is not defined in your
dnl Makefile, you can just use EOS_COVERAGE_CLEAN_RULES.
dnl
dnl You should also add run_coverage.coverage to TESTS and add .coverage to
dnl TEST_EXTENSIONS
dnl
dnl Variables that affect the operation of the inserted make rules:
dnl
dnl  - EOS_JS_COVERAGE_FILES: The list of JavaScript files to be included
dnl    in the JavaScript coverage report. This must be set before including
dnl    this Makefile if EOS_ENABLE_JS_COVERAGE was enabled. Absolute, relative
dnl    and resource paths are all fine.
dnl
dnl If EOS_ENABLE_C_COVERAGE was enabled and coverage reporting for the
dnl project has been enabled on the commandline, then these make rules will
dnl add -coverage to CFLAGS and LDFLAGS and also create a "eos-c-coverage"
dnl target which collects line and function hit counter data and places
dnl it in $coverage_directory/output/c/coverage.lcov
dnl
dnl If EOS_ENABLE_JS_COVERAGE was enabled and coverage reporting for this
dnl project has been enabled on the commandline, then these make rules will
dnl add the coverage generating switch to EOS_JS_COVERAGE_LOG_FLAGS for all
dnl files specified in EOS_JS_COVERAGE_FILES. Coverage output from gjs will
dnl go in $coverage_directory/output/js/coverage.lcov

EOS_HAVE_C_COVERAGE=no
EOS_HAVE_JS_COVERAGE=no
EOS_HAVE_COBERTURA=no
EOS_HAVE_GENTHML=no
EOS_HAVE_COVERAGE_REPORT=no

LCOV=notfound
GENHTML=notfound
LCOV_RESULT_MERGER=notfound

AC_DEFUN_ONCE([EOS_COVERAGE_REPORT], [
    # Enable the --enable-coverage switch, although it'll only be effective
    # if we can actually do coverage reports
    AC_ARG_ENABLE([coverage],
        [AS_HELP_STRING([--enable-coverage],
            [Generate code coverage statistics when running tests @<:@default=no@:>@])
    ])

    # This needs to be defined here so that AC_MSG_RESULT sees it
    EOS_COVERAGE_REQUESTED=no
    AC_MSG_CHECKING([whether code coverage support was requested])
    AS_IF([test "x$enable_coverage" = "xyes"], [
        EOS_COVERAGE_REQUESTED=yes
    ])
    AC_MSG_RESULT([$EOS_COVERAGE_REQUESTED])

    AC_MSG_CHECKING([where to put coverage data])
    AC_ARG_VAR([EOS_COVERAGE_DIR],
        [Where to put coverage reports @<:@default=_coverage@:>@])
    EOS_DEFAULT_COVERAGE_DIR='$(abs_top_builddir)/_coverage'
    AC_ARG_WITH([coverage-dir], [
        AS_HELP_STRING([--with-coverage-dir=DIRECTORY],
            [Where to put coverage reports (overrides EOS_COVERAGE_DIR)])
    ], [
        EOS_COVERAGE_DIR="$withval"
    ])
    AS_IF([test -z "$EOS_COVERAGE_DIR"], [
        EOS_COVERAGE_DIR="$EOS_DEFAULT_COVERAGE_DIR"
        AC_MSG_RESULT([in default location (_coverage)])
    ], [
        AC_MSG_RESULT([in $EOS_COVERAGE_DIR])
    ])

    AS_IF([test "x$EOS_COVERAGE_REQUESTED" = "xyes"], [

        # Need LCOV to do coverage report filtering. If we don't have it
        # then we have to disable coverage reporting wholesale.
        AC_PATH_PROG([LCOV], [lcov], [notfound])
        AC_ARG_VAR([LCOV], [Path to lcov])

        AS_IF([test "x$LCOV" != "xnotfound"], [
            m4_foreach_w([opt], [$1], [
                AS_CASE(opt,
                    [c], [
                        # Turn on the coverage generating flags in GCC. This
                        # does not work with clang
                        # (see llvm.org/bugs/show_bug.cgi?id=16568).
                        EOS_COVERAGE_COMPILER_FLAGS="-coverage -g -O0"
                        EOS_C_COVERAGE_LDFLAGS="-lgcov"
                        EOS_COVERAGE_CFLAGS_SUPPORTED=no

                        LAST_CFLAGS="$CFLAGS"
                        CFLAGS="$CFLAGS $EOS_COVERAGE_COMPILER_FLAGS"
                        AC_MSG_CHECKING([if compiler supports $EOS_COVERAGE_COMPILER_FLAGS])
                        AC_TRY_COMPILE([], [], [EOS_COVERAGE_CFLAGS_SUPPORTED=yes])
                        CFLAGS="$LAST_CFLAGS"
                        AC_MSG_RESULT($EOS_COVERAGE_CFLAGS_SUPPORTED)

                        # If this is empty then coverage reporting is not
                        # supported by the compiler.
                        AS_IF([test x"$EOS_COVERAGE_CFLAGS_SUPPORTED" = "xyes"], [
                            EOS_HAVE_C_COVERAGE=yes

                            # These flags aren't mandatory, but they make
                            # coverage reports more accurate
                            EOS_C_COVERAGE_CFLAGS="$EOS_COVERAGE_COMPILER_FLAGS"
                        ])
                    ],
                    [js], [
                        PKG_CHECK_MODULES([GJS_WITH_COVERAGE], [gjs-1.0 >= 1.40.0], [
                            EOS_HAVE_JS_COVERAGE=yes
                        ])
                    ])
            ])

            AS_IF([test "x$EOS_HAVE_C_COVERAGE" = "xyes" || test "x$EOS_HAVE_JS_COVERAGE" = "xyes"], [
                AC_PATH_PROG([GENHTML], [genhtml], [notfound])
                AC_ARG_VAR([GENHTML], [Path to genhtml])
                AS_IF([test "x$GENHTML" = "xnotfound"], [
                ], [
                    EOS_HAVE_GENHTML=yes
                ])

                AC_PATH_PROG([LCOV_RESULT_MERGER], [node-lcov-result-merger], [notfound])
                AC_ARG_VAR([LCOV_RESULT_MERGER], [Path to lcov-result-merger])

                AC_MSG_CHECKING([for lcov_cobertura])
                python -c "import lcov_cobertura" > /dev/null 2>&1
                AS_IF([test "$?" = "0"], [
                    EOS_COVERAGE_HAVE_LCOV_COBERTURA=yes
                ], [
                    EOS_COVERAGE_HAVE_LCOV_COBERTURA=no
                ])
                AC_MSG_RESULT([$EOS_COVERAGE_HAVE_LCOV_COBERTURA])

                AS_IF([test "x$LCOV_RESULT_MERGER" != "xnotfound" && test "x$EOS_COVERAGE_HAVE_LCOV_COBERTURA" = "xyes"], [
                    EOS_HAVE_COBERTURA=yes
                ])

                # We have coverage reporting as long as we have either cobertura or
                # genhtml support
                AS_IF([test "x$EOS_HAVE_COBERTURA" = "xyes" || test "x$EOS_HAVE_GENHTML" = "xyes" ], [
                    EOS_HAVE_COVERAGE_REPORT=yes
                ])
            ])

            # EOS_ENABLE_COVERAGE is set to "no" unless coverage
            # in at least one language is enabled and a coverage reporter
            # is available.
            EOS_ENABLE_COVERAGE=no
            AS_IF([test "x$EOS_HAVE_COVERAGE_REPORT" = "xyes"], [
                AS_IF([test "x$EOS_HAVE_C_COVERAGE" = "xyes"], [
                    dnl Strip leading spaces
                    EOS_C_COVERAGE_CFLAGS=${EOS_C_COVERAGE_CFLAGS#*  }
                    EOS_ENABLE_C_COVERAGE=yes
                ])
                EOS_ENABLE_JS_COVERAGE=$EOS_HAVE_JS_COVERAGE
                EOS_ENABLE_COVERAGE=yes
            ])
            AC_MSG_CHECKING([whether code coverage support can be enabled])
            AC_MSG_RESULT([$EOS_ENABLE_COVERAGE])
        ])
    ])

    # Now that we've figured out if we have coverage reports, build the rules
    EOS_COVERAGE_RULES_HEADER='
# This file was generated by eos-coverage.m4
#
# Internal variable to track the coverage accumulated counter files.
_eos_coverage_outputs =
_eos_collect_coverage_targets =
_eos_clean_coverage_targets =

# Full path to coverage report folder
_eos_coverage_report_path := $(EOS_COVERAGE_DIR)/report

# Full path to coverage tracefile output
_eos_coverage_trace_path := $(EOS_COVERAGE_DIR)/output

'

    EOS_COVERAGE_RULES_TARGETS='
# First check that all the required variables have been set. This includes:
# - LCOV

$(if $(LCOV),,$(error LCov not found, ensure that eos-coverage.m4 was included in configure.ac))

# Internal variable for the genhtml coverage report path
_eos_genhtml_coverage_report_path = $(_eos_coverage_report_path)/genhtml

# Internal variable for the cobertura coverage report path
_eos_cobertura_coverage_report_path = $(_eos_coverage_report_path)/cobertura

# Set up an intermediate eos-collect-coverage target
# which just runs the language specific coverage collection
# targets
#
# We then compile all of the language specific coverage collection
# tracefiles into a single tracefile and apply the filters
# set in EOS_COVERAGE_BLACKLIST_PATTERNS if there are any
_eos_lcov_add_files_opts = $(addprefix -a ,$(_eos_coverage_outputs))
eos-collect-coverage: $(_eos_collect_coverage_targets)
	$(LCOV) -o $(_eos_coverage_trace_path)/coverage.lcov $(_eos_lcov_add_files_opts)
	if test x"$(EOS_COVERAGE_BLACKLIST_PATTERNS)" != "x" ; then echo $(EOS_COVERAGE_BLACKLIST_PATTERNS) | xargs -L1 $(LCOV) -o $(_eos_coverage_trace_path)/coverage.lcov -r $(_eos_coverage_trace_path)/coverage.lcov ; fi

# The clean-coverage target runs the language specific
# clean rules and also cleans the generated html reports
#
# NOTE: We only delete the default coverage dir and not
# any overriden coverage dir. The reason is that the user
# may have specified a coverage dir that is outside
# the source or build directory and wants to keep
# coverage reports for some later use.
clean-coverage: $(_eos_clean_coverage_targets)
	-rm -rf $(EOS_DEFAULT_COVERAGE_DIR)
'

    EOS_COVERAGE_RULES_TARGETS_DISABLED='
# Define the targets just to print an error if coverage reports are not enabled
eos-collect-coverage:
	@echo "--enable-coverage must be passed to ./configure"
	@exit 1

clean-coverage:
	@echo "no coverage data generated, so none to clean"
'

    AS_IF([test "x$EOS_HAVE_GENHTML" = "xyes"], [
        EOS_GENHTML_COVERAGE_RULES='
# Check that required variable GENHTML is set
$(if $(GENHTML),,$(error GenHTML not found, ensure that eos-coverage.m4 was included in configure.ac))

# The "coverage-genhtml" target depends on eos-collect-coverage
# and then runs genhtml on the coverage counters. This is useful
# if you are just looking at coverage data locally.
coverage-genhtml: eos-collect-coverage
	$(GENHTML) --legend -o $(_eos_genhtml_coverage_report_path) $(_eos_coverage_trace_path)/coverage.lcov
'
        EOS_GENHTML_AUTO_COVERAGE_CMD='make coverage-genhtml'
    ], [
        EOS_GENHTML_COVERAGE_RULES='
coverage-genhtml:
	@echo "Cannot generate GenHTML coverage report as genhtml was not found in PATH"
	@exit 1
'
        EOS_GENHTML_AUTO_COVERAGE_CMD='echo "Not generating GenHTML report"'
    ])

    AS_IF([test "x$EOS_HAVE_COBERTURA" = "xyes"], [
        EOS_COBERTURA_COVERAGE_RULES='
# Paths to each stage of the cobertura coverage report
# 1. Merged path
# 2. XML path
_eos_cobertura_merged_path = $(_eos_cobertura_coverage_report_path)/merged.lcov
_eos_cobertura_xml_path = $(_eos_cobertura_coverage_report_path)/cobertura.xml

# The "coverage-cobertura" target depends on eos-collect-coverage
# and then runs lcov_cobertura.py to convert it to a cobertura compatible
# XML file format
coverage-cobertura: eos-collect-coverage
	mkdir -p $(_eos_cobertura_coverage_report_path)
	$(LCOV_RESULT_MERGER) $(_eos_coverage_trace_path)/coverage.lcov $(_eos_cobertura_merged_path)
	python -c "from lcov_cobertura import LcovCobertura; open(\"$(_eos_cobertura_xml_path)\", \"w\").write(LcovCobertura(open(\"$(_eos_cobertura_merged_path)\", \"r\").read()).convert())"
'
        EOS_COBERTURA_AUTO_COVERAGE_CMD='make coverage-cobertura'
    ], [
        EOS_COBERTURA_COVERAGE_RULES='
coverage-cobertura:
	@echo "Cannot generate Cobertura coverage report as lcov-result-merger was not found in PATH or lcov_cobertura was not found in PYTHONPATH"
	@exit 1
'
        EOS_COBERTURA_AUTO_COVERAGE_CMD='echo "Not generating Cobertura report"'
    ])

    AS_IF([test "x$EOS_ENABLE_JS_COVERAGE" = "xyes"], [
        EOS_JS_COVERAGE_RULES='
# First check that all the required variables have been set. This includes:
# - EOS_JS_COVERAGE_FILES

$(if $(EOS_JS_COVERAGE_FILES),,$(warning Need to define EOS_JS_COVERAGE_FILES))

# Internal variables for the coverage data output path and file
_eos_js_coverage_trace_path := $(_eos_coverage_trace_path)/js
_eos_js_coverage_data_output_file := $(_eos_js_coverage_trace_path)/coverage.lcov

# Add _eos_js_coverage_data_output_file to _eos_coverage_outputs
_eos_coverage_outputs += $(_eos_js_coverage_data_output_file)
'

        # This small fragment collects all the paths and add the --coverage-prefix
        # prefix to each one, finally adding --coverage-output. This makes the list
        # of flags we will pass to gjs to enable coverage reports.
        #
        # This line might make maintainers say "gesundheit!" so here is a
        # short explainer.
        #
        # We're not able to define any functions in the makefile here unless
        # the user explicitly sets AM_JS_LOG_FLAGS using := (because this
        # variable will appear at the top of the makefile otherwise). Because
        # that imposes a very subtle implementation detail on users, the
        # complexity is hidden here instead.
        #
        # The pseudocode for this line looks something like this:
        # paths = []
        # foreach (p in EOS_JS_COVERAGE_FILES) {
        #     if (path not in EOS_COVERAGE_BLACKLIST_PATTERNS and
        #         path not in [p.withoutprefix("*/") for p in EOS_BLACKLIST_PATTERNS]) {
        #         if (path.replace(':', ' ').split(' ')[0] == 'resource') {
        #             paths.push(p) # resource:// style path, unmodified
        #         } else {
        #             paths.push(absolute_path_to(p)) # Absolute path
        #         }
        #     }
        # }
        #
        # flags = []
        # foreach (p in paths) {
        #     flags.push('--coverage-prefix=' + p)
        # }
        #
        # $(filter resource,$(firstword $(subst :, ,$(p)))) is a Makefile
        # idiom to implement "strings starts with". It converts the first
        # argument to subst to a space and then splits the string using
        # spaces as a delimiter, fetching the first word. Obviously, it
        # doesn't work very well if your string already has spaces in it
        # but in this case, we are just looking for the very first
        # resource:// and nothing else. Once the substring is fetched, it
        # "filters" it for "resource" (what we are looking for) and if
        # the substring is exactly "resource" then "resource" is returned,
        # else an emtpy string.
        #
        # Note that $(if cond,consequent,alternative) substitutes consequent
        # cond evalutes to a non-empty string. The documentation on this
        # point suggests that conditional operators can be used. This is
        # misleading.
        EOS_JS_COVERAGE_LOG_FLAGS='$(addprefix --coverage-prefix=,$(foreach p,$(filter-out $(subst */,,$(EOS_COVERAGE_BLACKLIST_PATTERNS)),$(filter-out $(subst *,%,$(EOS_COVERAGE_BLACKLIST_PATTERNS)),$(EOS_JS_COVERAGE_FILES))),$(if $(filter resource,$(firstword $(subst :, ,$(p)))),$(p),$(abspath $(abs_top_srcdir)/$(p))))) --coverage-output=$(_eos_js_coverage_trace_path)'
    ], [
        EOS_JS_COVERAGE_RULES=''
    ])

    AS_IF([test "x$EOS_ENABLE_C_COVERAGE" = "xyes"], [
        EOS_C_COVERAGE_RULES='
# Define internal variables to keep the C coverage counters in
_eos_c_coverage_trace_path := $(_eos_coverage_trace_path)/c
_eos_c_coverage_data_output_file := $(_eos_c_coverage_trace_path)/coverage.lcov
_eos_c_coverage_data_output_tmp_file := $(_eos_c_coverage_data_output_file).tmp

# Add final coverage output file to list of coverage data files
_eos_coverage_outputs += $(_eos_c_coverage_data_output_file)

# With out of tree builds, abs_top_srcdir is actually the relative path
# from the absolute top build dir to the top srcdir.
_eos_normalized_top_srcdir = $(abspath $(abs_top_srcdir))

# Define an eos-c-coverage target to generate the coverage counters
eos-c-coverage:
	mkdir -p $(_eos_c_coverage_trace_path)
	$(LCOV) --compat-libtool --capture --directory $(abs_top_builddir) -o $(_eos_c_coverage_data_output_tmp_file)
	$(LCOV) --extract $(_eos_c_coverage_data_output_tmp_file) "$(_eos_normalized_top_srcdir)/*" -o $(_eos_c_coverage_data_output_file)
	-rm -rf $(_eos_c_coverage_data_output_tmp_file)

eos-clean-c-coverage:
	-find $(abs_top_builddir) -name "*.gcda" -delete
	-find $(abs_top_builddir) -name "*.gcno" -delete

_eos_collect_coverage_targets += eos-c-coverage
_eos_clean_coverage_targets += eos-clean-c-coverage
'
    ], [
        EOS_C_COVERAGE_RULES='
eos-c-coverage:
	@echo "C coverage reporting not enabled"
	@exit 1

eos-clean-c-coverage: eos-c-coverage
'
    ])

    EOS_COVERAGE_AUTORUN_GENERATED_TARGET="
run_coverage.coverage:
	@echo '$EOS_GENHTML_AUTO_COVERAGE_CMD && $EOS_COBERTURA_AUTO_COVERAGE_CMD' > run_coverage.coverage
"

    EOS_COVERAGE_CLEAN_RULES='
clean-local: clean-coverage
 '

    EOS_COVERAGE_RULES_FOOTER='
run_coverage.log: $(filter-out run_coverage.log,$(TEST_LOGS))

COVERAGE_LOG_COMPILER = bash
CLEANFILES += run_coverage.coverage
.PHONY: eos-clean-c-coverage eos-c-coverage clean-coverage eos-collect-coverage coverage-cobertura coverage-genhtml
'

    EOS_COVERAGE_RULES="$EOS_COVERAGE_RULES_HEADER $EOS_GENHTML_COVERAGE_RULES $EOS_COBERTURA_COVERAGE_RULES $EOS_C_COVERAGE_RULES $EOS_JS_COVERAGE_RULES $EOS_COVERAGE_RULES_TARGETS $EOS_COVERAGE_AUTORUN_GENERATED_TARGET $EOS_COVERAGE_RULES_FOOTER"
    EOS_COVERAGE_RULES_DISABLED="$EOS_COVERAGE_RULES_TARGETS_DISABLED $EOS_COVERAGE_AUTORUN_GENERATED_TARGET $EOS_COVERAGE_RULES_FOOTER"

    AM_CONDITIONAL([EOS_ENABLE_COVERAGE], [test "x$EOS_ENABLE_COVERAGE" = "xyes"])

    # Substitute at the top first
    AC_SUBST([EOS_COVERAGE_DIR])
    AC_SUBST([EOS_DEFAULT_COVERAGE_DIR])

    AM_COND_IF([EOS_ENABLE_COVERAGE], [AC_SUBST([EOS_COVERAGE_RULES])],
        [AC_SUBST([EOS_COVERAGE_RULES], [$EOS_COVERAGE_RULES_DISABLED])])
    # We only want to define this to use it for full substitution, not as a variable
    AM_SUBST_NOTMAKE([EOS_COVERAGE_RULES])

    AC_SUBST([EOS_COVERAGE_CLEAN_RULES])
    AM_SUBST_NOTMAKE([EOS_COVERAGE_CLEAN_RULES])

    AC_SUBST([EOS_JS_COVERAGE_LOG_FLAGS])
    AC_SUBST(EOS_C_COVERAGE_CFLAGS)
    AC_SUBST(EOS_C_COVERAGE_LDFLAGS)
])
