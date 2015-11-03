bin_PROGRAMS = vdc-reference

vdc_reference_SOURCES = vdc-reference.c

vdc_reference_CFLAGS = \
    -I$(top_srcdir)/src \
    $(LIBDSUID_CFLAGS) \
    $(PROTOBUFC_CFLAGS) \
    $(AVAHI_CFLAGS)

vdc_reference_LDADD = \
    $(top_builddir)/src/libdsvdc.la \
    $(LIBDSUID_LIBS) \
    $(PROTOBUFC_LIBS) \
    $(AVAHI_LIBS)
