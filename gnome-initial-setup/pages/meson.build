pages = [
   'account',
   'display',
   'language',
   'live-chooser',
   'keyboard',
   'endless-eula',
   'eulas',
   'network',
   'timezone',
   'privacy',
   'goa',
   'password',
   'site',
   'summary',
   'software'
]

if get_option('region-page')
   pages += ['region']
endif

foreach page: pages
    subdir (page)
endforeach
