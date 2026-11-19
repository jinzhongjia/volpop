test_env = environment()
test_env.set('G_TEST_SRCDIR', meson.current_source_dir())
test_env.set('G_TEST_BUILDDIR', meson.current_build_dir())
test_env.set('G_DEBUG', 'gc-friendly,fatal-criticals')
test_env.set('MALLOC_PERTURB_', '165')
test_env.set('XDG_CONFIG_HOME', meson.current_build_dir() / 'xdg-config')

foreach name : ['volume', 'model', 'convert', 'config']
  exe = executable('test-' + name, 'test-' + name + '.c', dependencies: vpcore_dep)
  test(name, exe, args: ['--tap'], protocol: 'tap', env: test_env, suite: 'core')
endforeach

desktop_validate = find_program('desktop-file-validate', required: false)
if desktop_validate.found() and gtk.found()
  test('desktop-file', desktop_validate,
    args: [meson.project_source_root() / 'data' / app_id + '.desktop'],
    suite: 'data',
  )
endif
