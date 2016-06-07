if BUILD_TESTS

TESTS = vdc_mainloop vdc_properties vdc_database

check_PROGRAMS = vdc_mainloop vdc_properties vdc_database

COMMON_CFLAGS = \
    -I$(top_srcdir)/src \
    $(PROTOBUFC_CFLAGS) \
    $(AVAHI_CFLAGS) \
    $(CHECK_CFLAGS)

COMMON_LDFLAGS = \
    $(PROTOBUFC_LIBS) \
    $(AVAHI_LIBS) \
    $(CHECK_LIBS)

vdc_mainloop_SOURCES = vdc_mainloop.c

vdc_mainloop_CFLAGS = \
    $(COMMON_CFLAGS)

vdc_mainloop_LDADD = \
    $(top_builddir)/src/libdsvdc.la \
    $(COMMON_LDFLAGS)

vdc_properties_SOURCES = vdc_properties.c

vdc_properties_CFLAGS = \
    -I$(top_builddir)/messages \
    $(COMMON_CFLAGS)

vdc_properties_LDADD = \
    $(top_builddir)/src/libdsvdc.la \
    $(COMMON_LDFLAGS)

vdc_database_SOURCES = vdc_database.c

vdc_database_CFLAGS = \
	$(COMMON_CFLAGS)

vdc_database_LDADD = \
    $(top_builddir)/src/libdsvdc.la \
    $(COMMON_LDFLAGS)
endif
