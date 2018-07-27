autostart_files = [
    'gnome-initial-setup-copy-worker.desktop',
    'gnome-initial-setup-first-login.desktop',
    'gnome-welcome-tour.desktop',
    'eos-demo-video.desktop'
]

conf = configuration_data()
conf.set('LIBEXECDIR', join_paths(prefix, get_option('libexecdir')))

foreach desktop_file: autostart_files
    i18n.merge_file(
        'desktop',
        input: configure_file(
            input: files(desktop_file + '.in.in'),
            output: desktop_file + '.in',
            configuration: conf
        ),
        output: desktop_file,
        install_dir: join_paths(get_option('sysconfdir'), 'xdg', 'autostart'),
        po_dir: po_dir,
        install: true,
        type: 'desktop'
    )
endforeach

i18n.merge_file(
    'desktop',
    input: files('setup-shell.desktop'),
    output: 'setup-shell.desktop',
    install_dir: join_paths(data_dir, 'gdm', 'greeter', 'applications'),
    po_dir: po_dir,
    install: true,
    type: 'desktop'
)

i18n.merge_file(
    'desktop',
    input: configure_file(
        input: files('gnome-initial-setup.desktop.in.in'),
        output: 'gnome-initial-setup.desktop.in',
        configuration: conf
    ),
    output: 'gnome-initial-setup.desktop',
    install_dir: join_paths(data_dir, 'gdm', 'greeter', 'applications'),
    po_dir: po_dir,
    install: true,
    type: 'desktop'
)

rules_dir = join_paths(data_dir, 'polkit-1', 'rules.d')
install_data('20-gnome-initial-setup.rules', install_dir: rules_dir)
install_data('com.endlessm.DemoMode.rules', install_dir: rules_dir)

session_dir = join_paths(data_dir, 'gnome-session', 'sessions')
install_data('gnome-initial-setup.session', install_dir: session_dir)

mode_dir = join_paths(data_dir, 'gnome-shell', 'modes')
install_data('initial-setup.json', install_dir: mode_dir)

install_data('gnome-welcome-tour', install_dir: get_option('libexecdir'))

eos_demo_mode_data_dir = join_paths(get_option('datadir'), 'eos-demo-mode')
install_data('eos-demo-video-C.webm', install_dir: eos_demo_mode_data_dir)
install_data('eos-demo-video-id.webm', install_dir: eos_demo_mode_data_dir)

policykit_dep = dependency ('polkit-gobject-1', version: '>= 0.103')
policykit_actions_dir = policykit_dep.get_pkgconfig_variable('policydir')

configure_file(
    input: 'com.endlessm.TestMode.policy.in',
    output: 'com.endlessm.TestMode.policy',
    install: true,
    install_dir: policykit_actions_dir,
    configuration: conf
)

configure_file(
    input: 'com.endlessm.DemoMode.policy.in',
    output: 'com.endlessm.DemoMode.policy',
    install: true,
    install_dir: policykit_actions_dir,
    configuration: conf
)

systemd_system_unit_dir = get_option('systemd-system-unit-dir')
systemd_dep = dependency ('systemd')
if systemd_system_unit_dir == ''
    systemd_system_unit_dir = systemd_dep.get_pkgconfig_variable('systemdsystemunitdir')
endif

configure_file(
    input: 'eos-transient-setup.service.in',
    output: 'eos-transient-setup.service',
    install: true,
    install_dir: systemd_system_unit_dir,
    configuration: conf
)

install_data(
    'product_serial.conf',
    install_dir: systemd_dep.get_pkgconfig_variable('tmpfilesdir')
)
