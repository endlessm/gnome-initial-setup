pages = [
   'account',
   'language',
   'live-chooser',
   'keyboard',
   'endless-eula',
   'network',
   'timezone',
   'privacy',
   'goa',
   'password',
   'site',
   'summary',
   'welcome',
]

if libmalcontent_dep.found() and libmalcontent_ui_dep.found()
  pages += 'parental-controls'
endif

foreach page: pages
    subdir (page)
endforeach
