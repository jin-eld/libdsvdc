noinst_PROGRAMS = reference

reference_SOURCES = reference.c

reference_CFLAGS = \
    -I$(top_srcdir)/src \
    $(PROTOBUFC_CFLAGS) \
    $(AVAHI_CFLAGS)

reference_LDADD = \
    $(top_builddir)/src/libdsvdc.la \
    $(PROTOBUFC_LIBS) \
    $(AVAHI_LIBS)
