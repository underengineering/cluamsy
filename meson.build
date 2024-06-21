project('cluamsy', 'c',
  version : '0.1',
  default_options : ['warning_level=3'])

cc = meson.get_compiler('c')

iup_includes = include_directories('./external/iup-3.30_Win64_dll16_lib/include/')
iup_dep = declare_dependency(
  dependencies : cc.find_library('iup', dirs: [meson.current_source_dir() / './external/iup-3.30_Win64_dll16_lib/']),
  include_directories : iup_includes)

windivert_includes = include_directories('./external/WinDivert-2.2.0-C/include/')
windivert_dep = declare_dependency(
  dependencies : cc.find_library('windivert', dirs: [meson.current_source_dir() / './external/WinDivert-2.2.0-C/x64/']),
  include_directories : windivert_includes)

luajit_dep = dependency('luajit', version : '>=2.1', required : true)

ws32_dep = cc.find_library('ws2_32', required : true)
winmm_dep = cc.find_library('winmm', required : true)

sources = files(
  'src/bandwidth.c',
  'src/common.h',
  'src/divert.c',
  'src/drop.c',
  'src/duplicate.c',
  'src/elevate.c',
  'src/lag.c',
  'src/main.c',
  'src/ood.c',
  'src/packet.c',
  'src/reset.c',
  'src/tamper.c',
  'src/throttle.c',
  'src/utils.c',
  'src/lua.c',
)
exe = executable('cluamsy', sources, dependencies : [iup_dep, windivert_dep, luajit_dep, ws32_dep, winmm_dep])

test('basic', exe)
