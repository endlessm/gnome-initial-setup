autostart_files = [
    'gnome-initial-setup-copy-worker.desktop',
    'gnome-initial-setup-first-login.desktop',
    'gnome-welcome-tour.desktop'
]

desktop_conf = configuration_data()
desktop_conf.set('LIBEXECDIR', libexec_dir)
desktop_conf.set('systemd_hidden', enable_systemd ? 'true' : 'false')

foreach desktop_file: autostart_files
    i18n.merge_file(
        'desktop',
        input: configure_file(
            input: files(desktop_file + '.in.in'),
            output: desktop_file + '.in',
            configuration: desktop_conf
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
    input: configure_file(
        input: files('gnome-initial-setup.desktop.in.in'),
        output: 'gnome-initial-setup.desktop.in',
        configuration: desktop_conf
    ),
    output: 'gnome-initial-setup.desktop',
    install_dir: join_paths(data_dir, 'gdm', 'greeter', 'applications'),
    po_dir: po_dir,
    install: true,
    type: 'desktop'
)

data_conf = configuration_data()
data_conf.set('libexecdir', libexec_dir)

if enable_systemd
    unit_files = {
        'gnome-initial-setup-first-login.service' : [ 'gnome-session.target.wants/' ],
        'gnome-initial-setup-copy-worker.service' : [ 'gnome-session.target.wants/' ],
        'gnome-welcome-tour.service' : [ 'gnome-session.target.wants/' ],
    }

    foreach unit, wants: unit_files
        configure_file(
            input: unit + '.in',
            output: unit,
            configuration: data_conf,
            install_dir: systemd_userunitdir
        )

        foreach target: wants
            meson.add_install_script('meson-add-wants.sh', systemd_userunitdir, target, unit)
        endforeach
    endforeach

    install_data(
        'gnome-initial-setup.session.conf',
        rename: join_paths('gnome-session@gnome-initial-setup.target.d', 'session.conf'),
        install_dir: systemd_userunitdir
  )
endif


rules_dir = join_paths(data_dir, 'polkit-1', 'rules.d')
configure_file(
    input: '20-gnome-initial-setup.rules.in',
    output: '20-gnome-initial-setup.rules',
    install: true,
    install_dir: rules_dir,
    configuration: data_conf,
)

session_dir = join_paths(data_dir, 'gnome-session', 'sessions')
install_data('gnome-initial-setup.session', install_dir: session_dir)

mode_dir = join_paths(data_dir, 'gnome-shell', 'modes')
install_data('initial-setup.json', install_dir: mode_dir)

install_data('gnome-welcome-tour', install_dir: get_option('libexecdir'))

policykit_actions_dir = policykit_dep.get_pkgconfig_variable('policydir')

configure_file(
    input: 'com.endlessm.TestMode.policy.in',
    output: 'com.endlessm.TestMode.policy',
    install: true,
    install_dir: policykit_actions_dir,
    configuration: desktop_conf,
)

systemd_dep = dependency ('systemd')

install_data(
    'product_serial.conf',
    install_dir: systemd_dep.get_pkgconfig_variable('tmpfilesdir')
)
