noinst_LTLIBRARIES = libprotomessages.la

EXTRA_DIST =    $(top_srcdir)/messages/messages.proto \
                $(top_srcdir)/messages/vdcapi.proto

BUILT_SOURCES = messages.pb-c.h \
                messages.pb-c.c \
                vdcapi.pb-c.h \
                vdcapi.pb-c.c

CLEANFILES = $(BUILT_SOURCES)

libprotomessages_la_SOURCES = \
                $(BUILT_SOURCES)

libprotomessages_la_CPPFLAGS = \
                -I$(top_builddir)/messages

libprotomessages_la_CFLAGS = \
                -fvisibility=hidden

messages.pb-c.h: messages.proto
messages.pb-c.c: messages.proto
	$(PROTOC) -I $(top_srcdir)/messages --c_out=$(top_builddir)/messages $(top_srcdir)/messages/messages.proto

vdcapi.pb-c.h: vdcapi.proto
vdcapi.pb-c.c: vdcapi.proto
	$(PROTOC) -I $(top_srcdir)/messages --c_out=$(top_builddir)/messages $(top_srcdir)/messages/vdcapi.proto

