# NOTE: env CC=clang CC_LD=lld-link CXX=clang++ CXX_LD=lld-link meson build -Dbuildtype=debug

project(
  'cluamsy',
  'cpp',
  version: '0.1',
  default_options: ['warning_level=3', 'default_library=static'],
)

cxx = meson.get_compiler('cpp')

sdl2_dep = dependency('sdl2', version: '>=2.30', required: true)
sdl2_main_dep = dependency('sdl2main', required: true)
opengl_dep = dependency('gl', required: true)

imgui_dep = dependency(
  'imgui',
  version: '>=1.89',
  required: true,
  default_options: ['sdl2_renderer=enabled'],
)

tomlplusplus_includes = include_directories('./external/tomlplusplus')
tomlplusplus_dep = declare_dependency(include_directories: tomlplusplus_includes)

windivert_includes = include_directories('./external/WinDivert-2.2.0-C/include/')
windivert_dep = declare_dependency(
  dependencies: cxx.find_library(
    'windivert',
    dirs: [meson.current_source_dir() / './external/WinDivert-2.2.0-C/x64/'],
  ),
  include_directories: windivert_includes,
)

luajit_dep = dependency('luajit', version: '>=2.1', required: true)

ws32_dep = cxx.find_library('ws2_32', required: true)
winmm_dep = cxx.find_library('winmm', required: true)

configure_file(
  input: './external/WinDivert-2.2.0-C/x64/WinDivert.dll',
  output: 'WinDivert.dll',
  copy: true,
)

configure_file(
  input: './external/WinDivert-2.2.0-C/x64/WinDivert64.sys',
  output: 'WinDivert64.sys',
  copy: true,
)

sources = files(
  'src/bandwidth.cpp',
  'src/config.cpp',
  'src/divert.cpp',
  'src/drop.cpp',
  'src/duplicate.cpp',
  # 'src/elevate.cpp',
  'src/lag.cpp',
  # 'src/utils.cpp',
  'src/lua.cpp',
  'src/lua_util.cpp',
  'src/main.cpp',
  # 'src/main.cpp',
  # 'src/ood.cpp',
  'src/packet.cpp',
  # 'src/reset.cpp',
  'src/tamper.cpp',
  'src/throttle.cpp',
)
exe = executable(
  'cluamsy',
  sources,
  win_subsystem: 'windows',
  dependencies: [
    imgui_dep,
    sdl2_dep,
    sdl2_main_dep,
    opengl_dep,
    tomlplusplus_dep,
    windivert_dep,
    luajit_dep,
    ws32_dep,
    winmm_dep,
  ],
  override_options: ['cpp_std=c++20'],
  cpp_args: ['-DNOMINMAX', '-DTOML_HEADER_ONLY=0'],
)

test('basic', exe)