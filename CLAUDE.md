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
├── src/
│   ├── main.c              # Entry point
│   └── irsdk/              # iRacing SDK integration (pure C)
│       ├── irsdk.c         # SDK implementation
│       ├── irsdk.h         # Public API
│       └── irsdk_defines.h # Constants and data structures
├── include/                # Public headers
├── data/                   # Data files
├── build/                  # Build output (generated)
├── meson.build             # Build configuration
├── LICENSE                 # Source Available License
├── CLA.md                  # Contributor License Agreement
└── CONTRIBUTORS            # Signed contributors
```

## Building

Requires: Meson, Ninja (or Visual Studio), C compiler (MSVC recommended)

```bash
# Install build tools (if needed)
python -m pip install meson ninja

# Setup and build (with Ninja)
meson setup build
meson compile -C build

# Or with Visual Studio backend
meson setup build --backend=vs
meson compile -C build
```

The executable will be at `build/ira.exe`.

## Development Guidelines

- Follow C11 standard
- Use consistent naming conventions (snake_case for functions and variables)
- Document public APIs in header files
- Write tests for core functionality

## iRacing API Authentication (OAuth2)

**Status**: OAuth2 implementation complete, awaiting credentials from iRacing.

Legacy authentication (username/password to `/auth` endpoint) was retired by iRacing on December 9, 2025. All API access now requires OAuth2.

### When OAuth Credentials Are Received

Once iRacing provides the `client_id` (and optionally `client_secret`), complete these steps:

#### 1. Create a configuration file `oauth_config.json` in the project root:

```json
{
  "client_id": "YOUR_CLIENT_ID_HERE",
  "client_secret": "YOUR_CLIENT_SECRET_OR_NULL",
  "redirect_uri": "http://localhost:8080/callback",
  "callback_port": 8080
}
```

#### 2. Update `src/main.c` to use OAuth authentication:

Replace any legacy `api_set_credentials()` calls with:

```c
#include "api/iracing_api.h"

// In your initialization code:
iracing_api *api = api_create();
api_set_oauth(api, "YOUR_CLIENT_ID", NULL);  // NULL if no client_secret

api_error err = api_authenticate(api);
if (err != API_OK) {
    printf("Auth failed: %s\n", api_get_last_error(api));
    return 1;
}

// Now fetch data:
ira_database *db = database_create();
api_fetch_cars(api, db);
api_fetch_tracks(api, db);
api_fetch_seasons(api, db, 2026, 1);

// Save to JSON files:
database_save_cars(db, database_get_cars_path());
database_save_tracks(db, database_get_tracks_path());
database_save_seasons(db, database_get_seasons_path());
```

#### 3. Test the OAuth flow:

```bash
meson compile -C build
./build/ira.exe
```

The first run will:
1. Open your browser to iRacing's login page
2. Wait for authorization on `localhost:8080`
3. Exchange the code for tokens
4. Save tokens to `oauth_tokens.json` (auto-refreshed on future runs)

### OAuth Module Reference

| Function | Purpose |
|----------|---------|
| `api_set_oauth(api, client_id, client_secret)` | Configure OAuth credentials |
| `api_authenticate(api)` | Start OAuth flow (opens browser) |
| `oauth_token_valid(client)` | Check if token is still valid |
| `oauth_refresh(client)` | Refresh expired token |
| `oauth_save_tokens(client, filename)` | Persist tokens to file |
| `oauth_load_tokens(client, filename)` | Load tokens from file |

### Files Involved

- `src/util/oauth.h` / `oauth.c` - OAuth2 client with PKCE
- `src/util/crypto.h` / `crypto.c` - SHA256 and Base64 for PKCE
- `src/util/http.h` / `http.c` - HTTP client with Bearer token support
- `src/api/iracing_api.h` / `iracing_api.c` - API client with OAuth integration

### Troubleshooting

| Issue | Solution |
|-------|----------|
| "No client_id" error | Call `api_set_oauth()` before `api_authenticate()` |
| Browser doesn't open | Manually visit the URL printed to console |
| Port 8080 in use | Change `callback_port` in oauth_config or code |
| Token expired | Tokens auto-refresh; delete `oauth_tokens.json` to re-auth |
| 401 on API calls | Token invalid; delete `oauth_tokens.json` and re-authenticate |

### iRacing OAuth Registration

To obtain credentials, register at: https://oauth.iracing.com/oauth2/book/client_registration.html

Request type: **Password Limited** (for CLI/backend apps) or **Authorization Code** (for user-facing apps)

Processing time: Up to 10 business days

## License

**Source Available License** - Copyright (c) 2026 Christopher Griffiths

- Source code freely available for personal, non-commercial use
- Personal users may view, modify, compile, and run for themselves
- Modified source may be shared with attribution to other personal users
- Redistribution of compiled binaries requires a commercial license
- Contributions require a signed CLA

See [LICENSE](LICENSE) for full terms.
