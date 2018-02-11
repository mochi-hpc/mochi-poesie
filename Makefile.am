ACLOCAL_AMFLAGS="-Im4"

SERVER_LIBS=@SERVER_LIBS@
CLIENT_LiBS=@CLIENT_LIBS@

AM_CPPFLAGS = -I${srcdir}/src -I${srcdir}/include

bin_PROGRAMS = bin/poesie-server-daemon \
	       bin/poesie-shutdown

check_PROGRAMS = test/poesie-test

bin_poesie_server_daemon_SOURCES      = src/poesie-server-daemon.c
bin_poesie_server_daemon_DEPENDENCIES = lib/libpoesie-server.la
bin_poesie_server_daemon_LDFLAGS      = -Llib -lpoesie-server
bin_poesie_server_daemon_LDADD        = ${LIBS} -lpoesie-server ${SERVER_LIBS}

bin_poesie_shutdown_SOURCES      = src/poesie-shutdown.c
bin_poesie_shutdown_DEPENDENCIES = lib/libpoesie-client.la
bin_poesie_shutdown_LDFLAGS      = -Llib -lpoesie-client
bin_poesie_shutdown_LDADD        = ${LIBS} -lpoesie-client

test_poesie_test_SOURCES      = test/poesie-test.c
test_poesie_test_DEPENDENCIES = lib/libpoesie-client.la
test_poesie_test_LDFLAGS      = -Llib -lpoesie-client

lib_LTLIBRARIES = lib/libpoesie-client.la \
		  lib/libpoesie-server.la

lib_libpoesie_client_la_SOURCES = src/poesie-client.c

lib_libpoesie_server_la_SOURCES = src/poesie-server.c \
				  src/poesie-vm-impl.c

if BUILD_PYTHON
lib_libpoesie_server_la_SOURCES += src/lang/poesie-python.c
endif

if BUILD_LUA
lib_libpoesie_server_la_SOURCES += src/lang/poesie-lua.c
endif

include_HEADERS = include/poesie-client.h \
		  include/poesie-server.h \
		  include/poesie-common.h

noinst_HEADERS = src/poesie-rpc-types.h \
		 src/poesie-vm.h \
		 src/poesie-vm-impl.h \
		 src/uthash.h \
		 src/lang/poesie-python.h \
		 src/lang/poesie-lua.h

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = maint/poesie-server.pc \
		 maint/poesie-client.pc



