project('ira', 'c',
  version : '0.1.0',
  license : 'Proprietary',
  default_options : [
    'c_std=c11',
    'warning_level=2',
  ]
)

# Windows-specific dependencies
if host_machine.system() == 'windows'
  # Core Windows libraries
  kernel32 = meson.get_compiler('c').find_library('kernel32')
  user32 = meson.get_compiler('c').find_library('user32')
  winmm = meson.get_compiler('c').find_library('winmm')
  shell32 = meson.get_compiler('c').find_library('shell32')

  windows_deps = [kernel32, user32, winmm, shell32]
else
  error('ira currently only supports Windows')
endif

# Source files
irsdk_sources = files(
  'src/irsdk/irsdk.c',
  'src/irsdk/yaml_parser.c',
)

util_sources = files(
  'src/util/json.c',
  'src/util/config.c',
)

launcher_sources = files(
  'src/launcher/launcher.c',
)

telemetry_sources = files(
  'src/telemetry/telemetry_log.c',
)

main_sources = files(
  'src/main.c',
)

# Include directories
inc_dirs = include_directories('src')

# Build the executable
ira_exe = executable('ira',
  main_sources + irsdk_sources + util_sources + launcher_sources + telemetry_sources,
  include_directories : inc_dirs,
  dependencies : windows_deps,
  install : true,
)
