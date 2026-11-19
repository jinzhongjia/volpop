resources = gnome.compile_resources('vp-resources',
  'ui/volpop.gresource.xml',
  source_dir: 'ui',
  c_name: 'vp',
)

# gtk4-layer-shell 需要在 libwayland-client 之前链接，因此排在最前面
executable('volpop',
  files(
    'main.c',
    'backend/vp-pulse.c',
    'ui/vp-app.c',
    'ui/vp-page-main.c',
    'ui/vp-page-settings.c',
    'ui/vp-widgets.c',
  ),
  resources,
  dependencies: [layer, gtk, pulse_glib, vpcore_dep],
  include_directories: include_directories('backend', 'ui'),
  install: true,
)
