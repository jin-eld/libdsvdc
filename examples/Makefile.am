bin_PROGRAMS = reference

reference_SOURCES = reference.c

reference_CFLAGS = \
    -I$(top_srcdir)/src \
    $(LIBDSUID_CFLAGS) \
    $(PROTOBUFC_CFLAGS) \
    $(AVAHI_CFLAGS)

reference_LDADD = \
    $(top_builddir)/src/libdsvdc.la \
    $(LIBDSUID_LIBS) \
    $(PROTOBUFC_LIBS) \
    $(AVAHI_LIBS)
