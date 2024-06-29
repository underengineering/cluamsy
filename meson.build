project('cluamsy', 'cpp',
  version : '0.1',
  default_options : ['warning_level=3'])

cxx = meson.get_compiler('cpp')

sdl2_dep = dependency('sdl2', version : '>=2.30', required : true)
sdl2_main_dep = dependency('sdl2main')
opengl_dep = dependency('gl', required : true)

imgui_dep = dependency('imgui', version : '>=1.89', required : true, default_options : ['sdl_renderer=true'])

windivert_includes = include_directories('./external/WinDivert-2.2.0-C/include/')
windivert_dep = declare_dependency(
  dependencies : cxx.find_library('windivert', dirs : [meson.current_source_dir() / './external/WinDivert-2.2.0-C/x64/']),
  include_directories : windivert_includes)

luajit_dep = dependency('luajit', version : '>=2.1', required : true)

ws32_dep = cxx.find_library('ws2_32', required : true)
winmm_dep = cxx.find_library('winmm', required : true)

sources = files(
  'src/main.cpp',
  # 'src/bandwidth.cpp',
  'src/divert.cpp',
  'src/drop.cpp',
  # 'src/duplicate.cpp',
  # 'src/elevate.cpp',
  # 'src/lag.cpp',
  # 'src/main.cpp',
  # 'src/ood.cpp',
  'src/packet.cpp',
  'src/module.cpp',
  # 'src/reset.cpp',
  # 'src/tamper.cpp',
  'src/throttle.cpp'
  # 'src/utils.cpp',
  # 'src/lua.cpp',
)
exe = executable(
  'cluamsy',
  sources,
  win_subsystem : 'windows',
  dependencies : [
    imgui_dep, sdl2_dep, sdl2_main_dep, opengl_dep, windivert_dep, luajit_dep, ws32_dep, winmm_dep
  ],
  override_options : ['cpp_std=c++20'],
  cpp_args : ['-DNOMINMAX'])

test('basic', exe)
