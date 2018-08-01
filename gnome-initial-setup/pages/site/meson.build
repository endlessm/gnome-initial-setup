sources += gnome.compile_resources(
    'site-resources',
    files('site.gresource.xml'),
    c_name: 'site'
)

sources += files(
    'gis-site.c',
    'gis-site-page.c',
    'gis-site-search-entry.c',
)

install_data(
    sources: 'eos-write-location',
    install_dir: libexec_dir
)