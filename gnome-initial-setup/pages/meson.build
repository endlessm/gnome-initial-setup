pages = [
   'account',
   'display',
   'language',
   'keyboard',
   'endless-eula',
   'eulas',
   'network',
   'timezone',
   'privacy',
   'goa',
   'password',
   'summary',
   'software'
]

if get_option('region-page')
   pages += ['region']
endif

foreach page: pages
    subdir (page)
endforeach
