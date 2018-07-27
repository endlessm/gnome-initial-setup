sources = []

subdir('pages')
subdir('demo')

resources = gnome.compile_resources(
    'gis-assistant-resources',
    files('gis-assistant.gresource.xml'),
    c_name: 'gis_assistant'
)

sources += [
    resources,
    'gnome-initial-setup.c',
    'gis-assistant.c',
    'gis-page.c',
    'gis-driver.c',
    'gis-keyring.c',
    'gnome-initial-setup.h',
    'gis-assistant.h',
    'gis-page.h',
    'gis-driver.h',
    'gis-keyring.h',
    'gis-page-util.c',
    'gis-pkexec.c',
]

dependencies = [
    dependency ('libnm', version: '>= 1.2'),
    dependency ('libnma', version: '>= 1.0'),
    dependency ('polkit-gobject-1', version: '>= 0.103'),
    dependency ('accountsservice'),
    dependency ('gnome-desktop-3.0', version: '>= 3.7.5'),
    dependency ('eosmetrics-0', version: '>= 0.5.0'),
    dependency ('evince-document-3.0'),
    dependency ('evince-view-3.0'),
    dependency ('fontconfig'),
    dependency ('gweather-3.0'),
    dependency ('goa-1.0'),
    dependency ('goa-backend-1.0'),
    dependency ('gtk+-3.0', version: '>= 3.11.3'),
    dependency ('gio-unix-2.0', version: '>= 2.53.0'),
    dependency ('gdm', version: '>= 3.8.3'),
    dependency ('geocode-glib-1.0'),
    dependency ('gnome-keyring-1'),
    dependency ('libgeoclue-2.0', version: '>= 2.3.1'),
    cc.find_library('m', required: false),
    dependency ('pango', version: '>= 1.32.5'),
    dependency ('rest-0.7'),
    dependency ('json-glib-1.0'),
    dependency ('krb5'),
    dependency ('libsecret-1'),
    dependency ('pwquality'),
    dependency ('webkit2gtk-4.0'),
    cc.find_library('zint'),
    cheese_dep,
    cheese_gtk_dep,
    pkgkit_dep,
    ibus_dep,
]

dl_dep = cc.find_library('dl', required : true)
setlocale_wrapper_dir = get_option('libdir')

shared_library(
    'setlocale-wrapper',
    ['setlocale-wrapper.c'],
    dependencies: [dl_dep],
    install: true,
    install_dir: setlocale_wrapper_dir
)

setlocalwrapper_conf = configuration_data()
setlocalwrapper_conf.set('setlocale_wrapper_dir', setlocale_wrapper_dir)
setlocalwrapper_conf.set('libexecdir', libexec_dir)

configure_file(output: 'gnome-initial-setup',
               configuration: conf)
install_data(
    'gnome-initial-setup',
    install_dir: libexec_dir
)

install_data(
    'eos-demo-mode',
    install_dir: libexec_dir
)

install_data(
    'eos-transient-setup',
    install_dir: libexec_dir
)

executable(
    'gnome-initial-setup-copy-worker',
    ['gnome-initial-setup-copy-worker.c'],
    include_directories: config_h_dir,
    dependencies: dependencies,
    install: true,
    install_dir: get_option('libexecdir')
)

executable(
    'gnome-initial-setup-exec',
    sources,
    include_directories: config_h_dir,
    dependencies: dependencies,
    install: true,
    install_dir: get_option('libexecdir')
)
