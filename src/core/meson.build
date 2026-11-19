# 核心库：不依赖 GTK，也不连接音频服务，单元测试直接链接它
core_deps = [glib, gio, pulse, libm]

libvpcore = static_library('vpcore',
  files('vp-config.c', 'vp-convert.c', 'vp-model.c', 'vp-volume.c'),
  dependencies: core_deps,
  include_directories: root_inc,
)

vpcore_dep = declare_dependency(
  link_with: libvpcore,
  include_directories: [root_inc, include_directories('.')],
  dependencies: core_deps,
)
