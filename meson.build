project('volpop', 'c',
  version: '0.1.0',
  license: 'GPL-3.0-or-later',
  license_files: 'LICENSE',
  meson_version: '>= 1.1',
  default_options: [
    'c_std=gnu11',
    'warning_level=2',
    'buildtype=debugoptimized',
  ],
)

app_id = 'dev.volpop.Volpop'
gnome = import('gnome')
cc = meson.get_compiler('c')

add_project_arguments(
  cc.get_supported_arguments([
    '-Wno-unused-parameter',
    '-Wno-missing-field-initializers',
    '-Wno-cast-function-type',
    '-Werror=implicit-function-declaration',
    '-Werror=incompatible-pointer-types',
    '-Werror=return-type',
  ]),
  '-DG_LOG_DOMAIN="volpop"',
  language: 'c',
)

glib  = dependency('glib-2.0', version: '>= 2.76')
gio   = dependency('gio-2.0',  version: '>= 2.76')
pulse = dependency('libpulse', version: '>= 15.0')
pulse_glib = dependency('libpulse-mainloop-glib', version: '>= 15.0')
libm  = cc.find_library('m', required: false)
gtk   = dependency('gtk4', version: '>= 4.12', required: get_option('ui'))
layer = dependency('gtk4-layer-shell-0', required: get_option('layer_shell').disable_if(not gtk.found()))

conf = configuration_data()
conf.set_quoted('VP_APP_ID', app_id)
conf.set_quoted('VP_VERSION', meson.project_version())
conf.set10('HAVE_LAYER_SHELL', layer.found())
configure_file(output: 'config.h', configuration: conf)

root_inc = include_directories('.')

subdir('src/core')
if gtk.found()
  subdir('src')
  subdir('data')
endif
subdir('tests')

summary({
  'ui': gtk.found(),
  'layer-shell': layer.found(),
}, section: 'Features')
