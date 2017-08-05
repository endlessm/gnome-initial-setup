SUBDIRS = pages

BUILT_SOURCES =

uidir = $(datadir)/gnome-initial-setup

AM_CPPFLAGS = \
	$(INITIAL_SETUP_CFLAGS) \
	-DSECRET_API_SUBJECT_TO_CHANGE \
	-DGCR_API_SUBJECT_TO_CHANGE \
	-DVENDOR_CONF_FILE="\"$(VENDOR_CONF_FILE)\"" \
	-DUIDIR="\"$(uidir)\"" \
	-DGNOMELOCALEDIR=\""$(datadir)/locale"\" \
	-DLIBLOCALEDIR=\""$(prefix)/lib/locale"\" \
	-DCONFIGDIR=\"$(sysconfdir)/$(PACKAGE)\" \
	-DDATADIR=\""$(datadir)"\" \
	-DLOCALSTATEDIR=\""$(localstatedir)"\" \
	-DLIBEXECDIR=\""$(libexecdir)"\"

libexec_PROGRAMS = gnome-initial-setup-exec gnome-initial-setup-copy-worker

assistant_resource_files = $(shell glib-compile-resources --sourcedir=$(srcdir) --generate-dependencies $(srcdir)/gis-assistant.gresource.xml)
gis-assistant-resources.c: gis-assistant.gresource.xml $(assistant_resource_files)
	$(AM_V_GEN) $(GLIB_COMPILE_RESOURCES) --target=$@ --sourcedir=$(srcdir) --generate-source $<
gis-assistant-resources.h: gis-assistant.gresource.xml $(assistant_resource_files)
	$(AM_V_GEN) $(GLIB_COMPILE_RESOURCES) --target=$@ --sourcedir=$(srcdir) --generate-header $<
BUILT_SOURCES += gis-assistant-resources.c gis-assistant-resources.h

page_util_resource_files = $(shell glib-compile-resources --sourcedir=$(srcdir) --generate-dependencies $(srcdir)/gis-page-util.gresource.xml)
gis-page-util-resources.c: gis-page-util.gresource.xml $(page_util_resource_files)
	$(AM_V_GEN) $(GLIB_COMPILE_RESOURCES) --target=$@ --sourcedir=$(srcdir) --generate-source $<
gis-page-util-resources.h: gis-page-util.gresource.xml $(page_util_resource_files)
	$(AM_V_GEN) $(GLIB_COMPILE_RESOURCES) --target=$@ --sourcedir=$(srcdir) --generate-header $<
BUILT_SOURCES += gis-page-util-resources.c gis-page-util-resources.h

gnome_initial_setup_exec_SOURCES = \
	$(BUILT_SOURCES) \
	gnome-initial-setup.c gnome-initial-setup.h \
	gis-assistant.c gis-assistant.h \
	gis-page.c gis-page.h \
	gis-page-util.c gis-page-util.h \
	gis-pkexec.c gis-pkexec.h \
	gis-driver.c gis-driver.h \
	gis-keyring.c gis-keyring.h

gnome_initial_setup_exec_LDADD =	\
	pages/branding-welcome/libgisbrandingwelcome.la \
	pages/language/libgislanguage.la \
	pages/region/libgisregion.la \
	pages/keyboard/libgiskeyboard.la \
	pages/display/libgisdisplay.la \
	pages/endless-eula/libgisendlesseula.la \
	pages/eulas/libgiseulas.la \
	pages/network/libgisnetwork.la \
	pages/timezone/libgistimezone.la \
	pages/privacy/libgisprivacy.la \
	pages/live-chooser/libgislivechooser.la \
	pages/goa/libgisgoa.la \
	pages/account/libgisaccount.la \
	pages/password/libgispassword.la \
	pages/software/libgissoftware.la \
	pages/summary/libgissummary.la \
	$(INITIAL_SETUP_LIBS) \
	-lm

gnome_initial_setup_copy_worker_SOURCES =		\
	gnome-initial-setup-copy-worker.c

gnome_initial_setup_copy_worker_CFLAGS = \
	$(COPY_WORKER_CFLAGS)

gnome_initial_setup_copy_worker_LDADD = \
	$(COPY_WORKER_LIBS)

setlocalewrapperdir = $(SETLOCALE_WRAPPER_DIR)
setlocalewrapper_LTLIBRARIES = libsetlocale-wrapper.la
libsetlocale_wrapper_la_SOURCES = setlocale-wrapper.c
libsetlocale_wrapper_la_LIBADD = -ldl

gnome-initial-setup: gnome-initial-setup.in Makefile
	$(AM_V_GEN) sed -e "s|@setlocale_wrapper_dir[@]|$(SETLOCALE_WRAPPER_DIR)|g;" -e "s|@libexecdir[@]|$(libexecdir)|g" $< > $@

libexec_SCRIPTS = gnome-initial-setup
dist_libexec_SCRIPTS = \
	eos-setup-demo-user \
	eos-note-demo-config \
	$(NULL)

EXTRA_DIST = \
	gis-assistant.gresource.xml \
	gis-page-util.gresource.xml \
	gnome-initial-setup.in \
	$(assistant_resource_files) \
	$(libexec_SCRIPTS)		\
	$(page_util_resource_files)

CLEANFILES = \
	gnome-initial-setup \
	$(NULL)
