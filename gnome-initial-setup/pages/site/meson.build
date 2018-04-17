sources += gnome.compile_resources(
	'site-resources',
	files('site.gresource.xml'),
	c_name: 'site'
)

sources += files(
	'gis-site-page.c',
	'gis-site-page.h',
	'gis-site.c',
	'gis-site.h',
	'gis-site-search-entry.c',
	'gis-site-search-entry.h',
)

install_data(
	'eos-write-location',
	install_dir: libexec_dir,
	install_mode: 'rwxr-xr-x'
)
