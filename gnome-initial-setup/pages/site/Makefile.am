
NULL =

noinst_LTLIBRARIES = libgissite.la

AM_CPPFLAGS = \
	-DLOCALSTATEDIR=\""$(localstatedir)"\"				\
	-DLIBEXECDIR=\""$(libexecdir)"\"				\
	$(NULL)

BUILT_SOURCES =

resource_files = 							\
	$(shell glib-compile-resources --sourcedir=$(srcdir)		\
	--generate-dependencies $(srcdir)/site.gresource.xml)		\
	$(NULL)

site-resources.c: site.gresource.xml $(resource_files)
	$(AM_V_GEN)							\
	$(GLIB_COMPILE_RESOURCES) 					\
	--target=$@							\
	--sourcedir=$(srcdir)						\
	--generate-source $<						\
	$(NULL)

site-resources.h: site.gresource.xml $(resource_files)
	$(AM_V_GEN)							\
	$(GLIB_COMPILE_RESOURCES)					\
	--target=$@							\
	--sourcedir=$(srcdir)						\
	--generate-header $<						\
	$(NULL)

BUILT_SOURCES += site-resources.c site-resources.h

libgissite_la_SOURCES =							\
	$(BUILT_SOURCES)						\
	gis-site-page.c gis-site-page.h					\
	gis-site.c gis-site.h						\
	gis-site-search-entry.c gis-site-search-entry.h			\
	$(NULL)

libgissite_la_CFLAGS = $(INITIAL_SETUP_CFLAGS) -I "$(srcdir)/../.."
libgissite_la_LIBADD = $(INITIAL_SETUP_LIBS)
libgissite_la_LDFLAGS = -export_dynamic -avoid-version -module -no-undefined

EXTRA_DIST =								\
	site.gresource.xml						\
	$(resource_files)						\
	$(NULL)

dist_libexec_SCRIPTS = eos-write-location
