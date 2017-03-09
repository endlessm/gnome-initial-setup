
noinst_LTLIBRARIES = libgisbrandingwelcome.la

AM_CPPFLAGS = \
	$(INITIAL_SETUP_CFLAGS) \
	-DVENDOR_CONF_FILE="\"$(VENDOR_CONF_FILE)\"" \
	-DCONFIGDIR=\"$(sysconfdir)/$(PACKAGE)\" \
	-DLOCALSTATEDIR="\"$(localstatedir)\"" \
	-DUIDIR="\"$(uidir)\""

BUILT_SOURCES =

resource_files = $(shell glib-compile-resources --sourcedir=$(srcdir) --generate-dependencies $(srcdir)/branding-welcome.gresource.xml)
branding-welcome-resources.c: branding-welcome.gresource.xml $(resource_files)
	$(AM_V_GEN) $(GLIB_COMPILE_RESOURCES) --target=$@ --sourcedir=$(srcdir) --generate-source $<
branding-welcome-resources.h: branding-welcome.gresource.xml $(resource_files)
	$(AM_V_GEN) $(GLIB_COMPILE_RESOURCES) --target=$@ --sourcedir=$(srcdir) --generate-header $<
BUILT_SOURCES += branding-welcome-resources.c branding-welcome-resources.h

libgisbrandingwelcome_la_SOURCES =			\
	gis-branding-welcome-page.c gis-branding-welcome-page.h	\
	$(BUILT_SOURCES)

libgisbrandingwelcome_la_CFLAGS = $(INITIAL_SETUP_CFLAGS) -I "$(srcdir)/../.."
libgisbrandingwelcome_la_LIBADD = $(INITIAL_SETUP_LIBS)
libgisbrandingwelcome_la_LDFLAGS = -export_dynamic -avoid-version -module -no-undefined

EXTRA_DIST =	\
	branding-welcome.gresource.xml	\
	$(resource_files)
