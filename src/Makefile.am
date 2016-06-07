lib_LTLIBRARIES = libdsvdc.la

libdsvdc_la_include_HEADERS = dsvdc.h
libdsvdc_la_includedir = $(includedir)/dsvdc

libdsvdc_la_SOURCES = \
    common.h \
    database.c \
    database.h \
    dsvdc.c \
    dsvdc.h \
    log.h \
    log.c \
    msg_processor.c \
    msg_processor.h \
    properties.h \
    properties.c \
    sockutil.c \
    sockutil.h \
    util.c \
    util.h

if BUILD_DISCOVERY

libdsvdc_la_SOURCES += \
    discovery.h \
    discovery.c

endif

libdsvdc_la_CFLAGS = \
	-I$(top_builddir)/messages \
    $(PROTOBUFC_CFLAGS) \
    $(AVAHI_CFLAGS) \
    $(PTHREAD_CFLAGS)

libdsvdc_la_LDFLAGS = \
    -version-info $(LIBDSVDC_LT_VERSION)

libdsvdc_la_LIBADD = \
    $(top_builddir)/messages/libprotomessages.la

libdsvdc_la_LIBS = \
    $(PROTOBUFC_LIBS) \
    $(AVAHI_LIBS) \
    $(PTHREAD_LIBS)

