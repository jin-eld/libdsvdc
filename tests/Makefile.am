if BUILD_TESTS

TESTS = vdc_mainloop vdc_properties

check_PROGRAMS = vdc_mainloop vdc_properties

vdc_mainloop_SOURCES = vdc_mainloop.c

vdc_mainloop_CFLAGS = \
    -I$(top_srcdir)/src \
    $(PROTOBUFC_CFLAGS) \
    $(CHECK_CFLAGS)

vdc_mainloop_LDADD = \
    $(top_builddir)/src/libdsvdc.la \
    $(PROTOBUFC_LIBS) \
    $(CHECK_LIBS)

vdc_properties_SOURCES = vdc_properties.c

vdc_properties_CFLAGS = \
    -I$(top_srcdir)/src \
    -I$(top_srcdir)/messages \
    $(PROTOBUFC_CFLAGS) \
    $(CHECK_CFLAGS)

vdc_properties_LDADD = \
    $(top_builddir)/src/libdsvdc.la \
    $(PROTOBUFC_LIBS) \
    $(CHECK_LIBS)

endif
