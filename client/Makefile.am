INCLUDES = -I$(srcdir)/../services

bin_PROGRAMS = icecc
pkglibexec_SCRIPTS = icecc-create-env
icecc_SOURCES = main.cpp arg.cpp cpp.cpp local.cpp remote.cpp util.cpp md5.c safeguard.cpp
icecc_LDADD = ../services/libicecc.la $(LIBRSYNC)
noinst_HEADERS = client.h md5.h util.h
AM_CPPFLAGS = -DPLIBDIR=\"$(pkglibexecdir)\"

EXTRA_DIST = icecc-create-env

install-exec-local:
	$(mkinstalldirs) $(DESTDIR)$(bindir)
	for link in g++ gcc c++ cc icerun $(CLANG_SYMLINK_WRAPPERS); do \
	  rm -f $(DESTDIR)$(bindir)/$$link ;\
	  $(LN_S) icecc $(DESTDIR)$(bindir)/$$link ;\
	done

uninstall-local:
	for link in g++ gcc c++ cc icerun $(CLANG_SYMLINK_WRAPPERS); do \
	  rm $(DESTDIR)$(bindir)/$$link ;\
	done
