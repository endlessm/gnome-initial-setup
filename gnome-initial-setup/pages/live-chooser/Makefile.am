
noinst_LTLIBRARIES = libgislivechooser.la

AM_CPPFLAGS = \
	-I"$(top_srcdir)" \
	-I"$(top_srcdir)/gnome-initial-setup" \
	-I"$(top_builddir)" \
	-DDATADIR=\""$(datadir)"\" \
	-DGNOMELOCALEDIR=\""$(datadir)/locale"\" \
	-DLOCALSTATEDIR=\""$(localstatedir)"\" \
	-DLIBEXECDIR=\""$(libexecdir)"\"

BUILT_SOURCES =

resource_files = $(shell glib-compile-resources --sourcedir=$(srcdir) --generate-dependencies $(srcdir)/live-chooser.gresource.xml)
live-chooser-resources.c: live-chooser.gresource.xml $(resource_files)
	$(AM_V_GEN) $(GLIB_COMPILE_RESOURCES) --target=$@ --sourcedir=$(srcdir) --generate-source $<
live-chooser-resources.h: live-chooser.gresource.xml $(resource_files)
	$(AM_V_GEN) $(GLIB_COMPILE_RESOURCES) --target=$@ --sourcedir=$(srcdir) --generate-header $<
BUILT_SOURCES += live-chooser-resources.c live-chooser-resources.h

libgislivechooser_la_SOURCES =				\
	gis-live-chooser-page.c	gis-live-chooser-page.h	\
	$(BUILT_SOURCES)

libgislivechooser_la_CFLAGS = $(INITIAL_SETUP_CFLAGS)
libgislivechooser_la_LIBADD = $(INITIAL_SETUP_LIBS)
libgislivechooser_la_LDFLAGS = -export_dynamic -avoid-version -module -no-undefined

EXTRA_DIST = live-chooser.gresource.xml $(resource_files)

#dist_libexec_SCRIPTS = eos-test-mode
