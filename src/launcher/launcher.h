/*
 * ira - iRacing Application
 * Background Application Launcher
 *
 * Copyright (c) 2026 Christopher Griffiths
 */

#ifndef IRA_LAUNCHER_H
#define IRA_LAUNCHER_H

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdbool.h>

/* Launch trigger - when should the app be launched */
typedef enum {
    LAUNCH_MANUAL,      /* Only via button/command */
    LAUNCH_ON_CONNECT,  /* When iRacing connects */
    LAUNCH_ON_SESSION   /* When entering a session */
} launch_trigger;

/* Close behavior - when should the app be closed */
typedef enum {
    CLOSE_ON_IRACING_EXIT,  /* When iRacing disconnects */
    CLOSE_ON_IRA_EXIT,      /* When ira exits */
    CLOSE_NEVER             /* Leave running */
} close_behavior;

/* Application profile configuration */
typedef struct {
    char name[64];              /* Display name */
    char exe_path[MAX_PATH];    /* Path to executable */
    char args[256];             /* Command line arguments */
    char working_dir[MAX_PATH]; /* Working directory (optional) */
    launch_trigger trigger;
    close_behavior on_close;
    bool enabled;

    /* Runtime state (not persisted) */
    HANDLE process_handle;
    DWORD process_id;
    bool is_running;
} app_profile;

/* Application launcher manager */
typedef struct {
    app_profile *apps;
    int app_count;
    int app_capacity;
} app_launcher;

/*
 * Lifecycle functions
 */

/* Create a new launcher instance. Returns NULL on failure. */
app_launcher *launcher_create(void);

/* Destroy a launcher and free all resources. */
void launcher_destroy(app_launcher *launcher);

/*
 * Profile management
 */

/* Add an app profile. Returns true on success. */
bool launcher_add_app(app_launcher *launcher, const app_profile *profile);

/* Remove an app by name. Returns true if found and removed. */
bool launcher_remove_app(app_launcher *launcher, const char *name);

/* Get an app profile by name. Returns NULL if not found. */
app_profile *launcher_get_app(app_launcher *launcher, const char *name);

/* Get app profile by index. Returns NULL if out of bounds. */
app_profile *launcher_get_app_at(app_launcher *launcher, int index);

/* Get number of configured apps. */
int launcher_get_app_count(const app_launcher *launcher);

/*
 * Launch/close operations
 */

/* Start a specific app by name. Returns true on success. */
bool launcher_start_app(app_launcher *launcher, const char *name);

/* Stop a specific app by name. Returns true on success. */
bool launcher_stop_app(app_launcher *launcher, const char *name);

/* Start all enabled apps matching the given trigger. */
void launcher_start_all(app_launcher *launcher, launch_trigger trigger);

/* Stop all enabled apps matching the given close behavior. */
void launcher_stop_all(app_launcher *launcher, close_behavior behavior);

/*
 * Status functions
 */

/* Check if a specific app is running. */
bool launcher_is_running(app_launcher *launcher, const char *name);

/* Update the running status of all apps (poll process state). */
void launcher_update_status(app_launcher *launcher);

/*
 * Persistence (JSON config)
 */

/* Load app configuration from file. Returns true on success. */
bool launcher_load_config(app_launcher *launcher, const char *filename);

/* Save app configuration to file. Returns true on success. */
bool launcher_save_config(const app_launcher *launcher, const char *filename);

/*
 * Utility functions
 */

/* Convert launch_trigger enum to string. */
const char *launcher_trigger_to_string(launch_trigger trigger);

/* Convert string to launch_trigger. Returns LAUNCH_MANUAL on invalid input. */
launch_trigger launcher_string_to_trigger(const char *str);

/* Convert close_behavior enum to string. */
const char *launcher_close_to_string(close_behavior behavior);

/* Convert string to close_behavior. Returns CLOSE_ON_IRACING_EXIT on invalid input. */
close_behavior launcher_string_to_close(const char *str);

#endif /* IRA_LAUNCHER_H */
