dependencies = [
    dependency ('gtk+-3.0', version: '>= 3.11.3'),
    dependency ('gstreamer-1.0', version: '>= 1.10.4'),
]

executable(
    'eos-demo-video',
    ['eos-demo-video.c', 'eos-demo-video.h'],
    include_directories: config_h_dir,
    dependencies: dependencies,
    install: true,
    install_dir: get_option('libexecdir')
)
