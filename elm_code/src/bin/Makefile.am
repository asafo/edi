MAINTAINERCLEANFILES = Makefile.in

bin_PROGRAMS = elm_code_test

AM_CPPFLAGS = \
-DPACKAGE_BIN_DIR=\"$(bindir)\" \
-DPACKAGE_LIB_DIR=\"$(libdir)\" \
-DPACKAGE_DATA_DIR=\"$(datadir)/$(PACKAGE)\" \
-I$(top_srcdir)/elm_code/src/lib/ \
-DLOCALEDIR=\"$(datadir)/locale\" \
-DEFL_BETA_API_SUPPORT \
@EFL_CFLAGS@

elm_code_test_SOURCES = \
elm_code_test_main.c \
elm_code_test_private.h

elm_code_test_LDADD = @EFL_LIBS@ $(top_builddir)/elm_code/src/lib/libelm_code.la

elm_code_test_LDFLAGS = @EFL_LTLIBRARY_FLAGS@

