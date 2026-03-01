project('ira', 'c',
  version : '2026.2.0',
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

  # HTTP and crypto libraries for iRacing API
  winhttp = meson.get_compiler('c').find_library('winhttp')
  bcrypt = meson.get_compiler('c').find_library('bcrypt')
  crypt32 = meson.get_compiler('c').find_library('crypt32')
  ws2_32 = meson.get_compiler('c').find_library('ws2_32')

  windows_deps = [kernel32, user32, winmm, shell32, winhttp, bcrypt, crypt32, ws2_32]
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
  'src/util/crypto.c',
  'src/util/http.c',
  'src/util/oauth.c',
)

data_sources = files(
  'src/data/models.c',
  'src/data/database.c',
)

filter_sources = files(
  'src/filter/race_filter.c',
)

api_sources = files(
  'src/api/iracing_api.c',
)

launcher_sources = files(
  'src/launcher/launcher.c',
)

controller_sources = files(
  'src/controller/app_controller.c',
  'src/controller/filter_controller.c',
)

telemetry_sources = files(
  'src/telemetry/telemetry_log.c',
)

ui_sources = files(
  'src/ui/console.c',
)

main_sources = files(
  'src/main.c',
)

# Generate version header from template
version_h = configure_file(
  input : 'src/version.h.in',
  output : 'version.h',
  configuration : {'IRA_VERSION': meson.project_version()}
)

# Include directories (build dir for generated version.h)
inc_dirs = include_directories('src')

# Build the executable
ira_exe = executable('ira',
  main_sources + irsdk_sources + util_sources + data_sources + filter_sources + api_sources + launcher_sources + controller_sources + telemetry_sources + ui_sources + [version_h],
  include_directories : inc_dirs,
  dependencies : windows_deps,
  install : true,
)

# Tests
test_filter = executable('test_filter',
  files('tests/test_filter.c') + filter_sources + data_sources + util_sources,
  include_directories : inc_dirs,
  dependencies : windows_deps,
)

test('filter', test_filter)