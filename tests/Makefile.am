if BUILD_TESTS

TESTS = vdc_mainloop

check_PROGRAMS = vdc_mainloop

vdc_mainloop_SOURCES = vdc_mainloop.c

vdc_mainloop_CFLAGS = \
	-I$(top_srcdir)/src \
    $(CHECK_CFLAGS)

vdc_mainloop_LDADD = \
    $(top_builddir)/src/libdsvdc.la \
    $(CHECK_LIBS)

endif
