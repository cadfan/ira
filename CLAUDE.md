# ira - iRacing Application

A comprehensive do-all application for iRacing, written in C.

## Project Overview

ira is designed to be a unified application that handles all aspects of the iRacing experience, from telemetry analysis to race planning and session management.

## Core Features

### Telemetry & SDK Integration
- Direct integration with the iRacing SDK
- Real-time telemetry data capture and analysis
- Data logging and export capabilities

### iRacing API Integration
- OAuth authentication with iRacing web services
- Retrieve series schedules and race data
- Fetch car and track ownership information
- Race schedule management and session planning

### Inventory Management
- Automatically sync owned tracks and cars via iRacing API
- Filter available sessions by owned content
- Purchase tracking and wishlists

### Background Application Management
- Launch helper applications automatically
- Close applications when iRacing exits
- Configurable application profiles

### Race Filtering
- Filter by race duration
- Filter by Safety Rating (SR) potential
- Filter by iRating range
- Filter by series type (Sports, Formula, Dirt Road, Dirt Oval, Oval)
- Custom filter combinations

### Corner Calling
- Optional corner name announcements
- Visual corner display overlay
- Plugin architecture for extensibility

## Future Plans

### Overlay Support
- Desktop overlay for real-time data display
- VR overlay integration for in-headset information

## Technology Stack

- **Language**: C (C11 standard)
- **Build System**: Meson
- **Platform**: Windows (primary)
- **Data Storage**: JSON files
- **SDK**: iRacing SDK
- **Auth**: OAuth (iRacing API)

## Project Structure

```
ira/
├── src/           # Source files
├── include/       # Header files
├── lib/           # Third-party libraries
├── docs/          # Documentation
├── tests/         # Test files
└── tools/         # Build and utility scripts
```

## Building

```bash
meson setup build
meson compile -C build
```

## Development Guidelines

- Follow C11 standard
- Use consistent naming conventions (snake_case for functions and variables)
- Document public APIs in header files
- Write tests for core functionality

## License

**Source Available License** - Copyright (c) 2026 Christopher Griffiths

- Source code freely available for personal, non-commercial use
- Personal users may view, modify, compile, and run for themselves
- Modified source may be shared with attribution to other personal users
- Redistribution of compiled binaries requires a commercial license
- Contributions require a signed CLA

See [LICENSE](LICENSE) for full terms.
