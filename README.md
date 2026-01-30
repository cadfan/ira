# ira

A comprehensive companion application for iRacing, written in C.

ira runs alongside iRacing to provide real-time telemetry display, automatic application management, race filtering, and session planning tools.

## Features

### Real-Time Telemetry Display

Connect directly to iRacing's shared memory for live telemetry data:

- **Speed** - Current vehicle speed (metric or imperial)
- **RPM & Gear** - Engine state and transmission position
- **Throttle, Brake, Clutch** - Pedal inputs as percentages
- **Lap Information** - Current lap number and lap times
- **Fuel Level** - Remaining fuel quantity

Telemetry updates at 60Hz for smooth, responsive readings.

### Telemetry Logging

Record telemetry data to CSV files for later analysis:

- Automatic session detection names files by track
- Configurable sample rate
- Logs all core variables: Speed, RPM, Gear, Throttle, Brake, Clutch, Lap data, Fuel
- Compatible with any spreadsheet or analysis tool

Enable logging with `ira -l` or `ira --log`.

### Background Application Launcher

Automatically manage helper applications based on your iRacing session:

**Launch Triggers:**
- **Manual** - Launch only when you request it
- **On Connect** - Launch when iRacing starts
- **On Session** - Launch when you enter a session

**Close Behaviors:**
- **On iRacing Exit** - Close when iRacing closes
- **On ira Exit** - Close when ira closes
- **Never** - Leave running

**Conditional Launching:**

Configure apps to only launch for specific cars or tracks:

```json
{
  "name": "Dirt Setup Tool",
  "exe_path": "C:\\Tools\\DirtSetup.exe",
  "trigger": "on_session",
  "on_close": "on_iracing_exit",
  "enabled": true,
  "car_filter": {
    "mode": "include",
    "ids": [67, 89, 102]
  }
}
```

### Race Filtering

Find races that match your preferences and owned content:

**Filter Criteria:**
- **Owned Content Only** - Only show races where you own the car and track
- **License Range** - Filter by required license (Rookie through Pro)
- **Categories** - Oval, Road, Dirt Oval, Dirt Road, Sports Car, Formula
- **Setup Type** - Fixed setup, open setup, or both
- **Official Only** - Exclude unofficial series
- **Duration** - Set minimum and maximum race lengths in minutes
- **Exclusions** - Block specific series or tracks from results

### Interactive Configuration Menu

Press any key while waiting for iRacing to open the configuration menu:

1. List configured apps
2. Add new app
3. Remove app
4. Toggle app enabled/disabled
5. Launch/stop app manually
6. View settings
7. Show filter status
8. Show filtered races

### iRacing API Integration

Sync data directly from iRacing's web services:

- Cars with specifications (HP, weight, price)
- Tracks with configurations and features
- Series definitions and schedules
- Your owned content inventory
- Current season race schedules

**Note:** API access requires OAuth2 credentials from iRacing.

## Installation

### Requirements

- Windows 10 or later
- iRacing installed and configured

### Building from Source

```bash
python -m pip install meson ninja
git clone https://github.com/cadfan/ira.git
cd ira
meson setup build
meson compile -C build
```

The executable will be at `build/ira.exe`.

## Usage

```
ira [options]

General:
  -h, --help              Show help message
  -m, --metric            Use metric units [default]
  -i, --imperial          Use imperial units
  -l, --log               Enable telemetry logging
  --menu                  Open configuration menu

App Launcher:
  --list-apps             List configured apps
  --add-app <name> <path> Add app to configuration

Race Filtering:
  --races                 Show filtered races
  --filter-status         Show filter settings
  --sync                  Sync from iRacing API
```

## License

**Source Available License** - See [LICENSE](LICENSE) for full terms.