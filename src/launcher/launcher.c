/*
 * ira - iRacing Application
 * Background Application Launcher Implementation
 *
 * Copyright (c) 2026 Christopher Griffiths
 */

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "launcher.h"
#include "../util/json.h"

/* Initial capacity for app list */
#define INITIAL_CAPACITY 8

/*
 * Utility function implementations
 */

const char *launcher_trigger_to_string(launch_trigger trigger)
{
    switch (trigger) {
        case LAUNCH_MANUAL:     return "manual";
        case LAUNCH_ON_CONNECT: return "on_connect";
        case LAUNCH_ON_SESSION: return "on_session";
        default:                return "manual";
    }
}

launch_trigger launcher_string_to_trigger(const char *str)
{
    if (!str) return LAUNCH_MANUAL;

    if (strcmp(str, "on_connect") == 0) return LAUNCH_ON_CONNECT;
    if (strcmp(str, "on_session") == 0) return LAUNCH_ON_SESSION;
    return LAUNCH_MANUAL;
}

const char *launcher_close_to_string(close_behavior behavior)
{
    switch (behavior) {
        case CLOSE_ON_IRACING_EXIT: return "on_iracing_exit";
        case CLOSE_ON_IRA_EXIT:     return "on_ira_exit";
        case CLOSE_NEVER:           return "never";
        default:                    return "on_iracing_exit";
    }
}

close_behavior launcher_string_to_close(const char *str)
{
    if (!str) return CLOSE_ON_IRACING_EXIT;

    if (strcmp(str, "on_ira_exit") == 0) return CLOSE_ON_IRA_EXIT;
    if (strcmp(str, "never") == 0) return CLOSE_NEVER;
    return CLOSE_ON_IRACING_EXIT;
}

/*
 * Lifecycle functions
 */

app_launcher *launcher_create(void)
{
    app_launcher *launcher = (app_launcher *)calloc(1, sizeof(app_launcher));
    if (!launcher) {
        return NULL;
    }

    launcher->apps = (app_profile *)calloc(INITIAL_CAPACITY, sizeof(app_profile));
    if (!launcher->apps) {
        free(launcher);
        return NULL;
    }

    launcher->app_count = 0;
    launcher->app_capacity = INITIAL_CAPACITY;

    return launcher;
}

void launcher_destroy(app_launcher *launcher)
{
    if (!launcher) return;

    /* Stop all running apps that should close on ira exit */
    launcher_stop_all(launcher, CLOSE_ON_IRA_EXIT);

    /* Close any remaining process handles */
    for (int i = 0; i < launcher->app_count; i++) {
        if (launcher->apps[i].process_handle) {
            CloseHandle(launcher->apps[i].process_handle);
            launcher->apps[i].process_handle = NULL;
        }
    }

    free(launcher->apps);
    free(launcher);
}

/*
 * Profile management
 */

bool launcher_add_app(app_launcher *launcher, const app_profile *profile)
{
    if (!launcher || !profile) return false;

    /* Check for duplicate name */
    if (launcher_get_app(launcher, profile->name)) {
        return false;
    }

    /* Expand capacity if needed */
    if (launcher->app_count >= launcher->app_capacity) {
        int new_capacity = launcher->app_capacity * 2;
        app_profile *new_apps = (app_profile *)realloc(
            launcher->apps, new_capacity * sizeof(app_profile));
        if (!new_apps) {
            return false;
        }
        launcher->apps = new_apps;
        launcher->app_capacity = new_capacity;
    }

    /* Copy profile */
    launcher->apps[launcher->app_count] = *profile;

    /* Reset runtime state */
    launcher->apps[launcher->app_count].process_handle = NULL;
    launcher->apps[launcher->app_count].process_id = 0;
    launcher->apps[launcher->app_count].is_running = false;

    launcher->app_count++;
    return true;
}

bool launcher_remove_app(app_launcher *launcher, const char *name)
{
    if (!launcher || !name) return false;

    for (int i = 0; i < launcher->app_count; i++) {
        if (strcmp(launcher->apps[i].name, name) == 0) {
            /* Stop the app if running */
            if (launcher->apps[i].is_running) {
                launcher_stop_app(launcher, name);
            }

            /* Close handle if open */
            if (launcher->apps[i].process_handle) {
                CloseHandle(launcher->apps[i].process_handle);
            }

            /* Shift remaining apps down */
            for (int j = i; j < launcher->app_count - 1; j++) {
                launcher->apps[j] = launcher->apps[j + 1];
            }
            launcher->app_count--;
            return true;
        }
    }

    return false;
}

app_profile *launcher_get_app(app_launcher *launcher, const char *name)
{
    if (!launcher || !name) return NULL;

    for (int i = 0; i < launcher->app_count; i++) {
        if (strcmp(launcher->apps[i].name, name) == 0) {
            return &launcher->apps[i];
        }
    }

    return NULL;
}

app_profile *launcher_get_app_at(app_launcher *launcher, int index)
{
    if (!launcher || index < 0 || index >= launcher->app_count) {
        return NULL;
    }
    return &launcher->apps[index];
}

int launcher_get_app_count(const app_launcher *launcher)
{
    return launcher ? launcher->app_count : 0;
}

/*
 * Launch/close operations
 */

bool launcher_start_app(app_launcher *launcher, const char *name)
{
    app_profile *app = launcher_get_app(launcher, name);
    if (!app) return false;

    /* Don't start if disabled */
    if (!app->enabled) return false;

    /* Check if already running */
    launcher_update_status(launcher);
    if (app->is_running) return true;

    /* Build command line */
    char cmdline[MAX_PATH + 256 + 4];
    if (app->args[0] != '\0') {
        snprintf(cmdline, sizeof(cmdline), "\"%s\" %s", app->exe_path, app->args);
    } else {
        snprintf(cmdline, sizeof(cmdline), "\"%s\"", app->exe_path);
    }

    /* Set up working directory */
    const char *work_dir = NULL;
    if (app->working_dir[0] != '\0') {
        work_dir = app->working_dir;
    }

    /* Create process */
    STARTUPINFOA si;
    PROCESS_INFORMATION pi;

    memset(&si, 0, sizeof(si));
    si.cb = sizeof(si);
    memset(&pi, 0, sizeof(pi));

    BOOL success = CreateProcessA(
        NULL,           /* Application name (use command line) */
        cmdline,        /* Command line */
        NULL,           /* Process security attributes */
        NULL,           /* Thread security attributes */
        FALSE,          /* Inherit handles */
        CREATE_NEW_CONSOLE, /* Creation flags */
        NULL,           /* Environment */
        work_dir,       /* Working directory */
        &si,            /* Startup info */
        &pi             /* Process information */
    );

    if (!success) {
        return false;
    }

    /* Store process info */
    app->process_handle = pi.hProcess;
    app->process_id = pi.dwProcessId;
    app->is_running = true;

    /* Don't need the thread handle */
    CloseHandle(pi.hThread);

    return true;
}

/* Callback for EnumWindows to find main window of a process */
typedef struct {
    DWORD process_id;
    HWND main_window;
} find_window_data;

static BOOL CALLBACK find_main_window_callback(HWND hwnd, LPARAM lparam)
{
    find_window_data *data = (find_window_data *)lparam;
    DWORD window_pid = 0;

    GetWindowThreadProcessId(hwnd, &window_pid);
    if (window_pid == data->process_id) {
        /* Check if this is a main window (visible, no owner) */
        if (IsWindowVisible(hwnd) && GetWindow(hwnd, GW_OWNER) == NULL) {
            data->main_window = hwnd;
            return FALSE; /* Stop enumeration */
        }
    }
    return TRUE; /* Continue */
}

bool launcher_stop_app(app_launcher *launcher, const char *name)
{
    app_profile *app = launcher_get_app(launcher, name);
    if (!app) return false;

    /* Update status first */
    launcher_update_status(launcher);

    if (!app->is_running || !app->process_handle) {
        return true; /* Already stopped */
    }

    /* Try graceful shutdown first: send WM_CLOSE to main window */
    find_window_data data = { app->process_id, NULL };
    EnumWindows(find_main_window_callback, (LPARAM)&data);

    if (data.main_window) {
        PostMessageA(data.main_window, WM_CLOSE, 0, 0);

        /* Wait for graceful exit (up to 3 seconds) */
        if (WaitForSingleObject(app->process_handle, 3000) == WAIT_OBJECT_0) {
            CloseHandle(app->process_handle);
            app->process_handle = NULL;
            app->process_id = 0;
            app->is_running = false;
            return true;
        }
    }

    /* Graceful shutdown failed - force terminate */
    TerminateProcess(app->process_handle, 0);
    WaitForSingleObject(app->process_handle, 1000);

    CloseHandle(app->process_handle);
    app->process_handle = NULL;
    app->process_id = 0;
    app->is_running = false;

    return true;
}

void launcher_start_all(app_launcher *launcher, launch_trigger trigger)
{
    if (!launcher) return;

    for (int i = 0; i < launcher->app_count; i++) {
        if (launcher->apps[i].enabled && launcher->apps[i].trigger == trigger) {
            launcher_start_app(launcher, launcher->apps[i].name);
        }
    }
}

void launcher_stop_all(app_launcher *launcher, close_behavior behavior)
{
    if (!launcher) return;

    for (int i = 0; i < launcher->app_count; i++) {
        if (launcher->apps[i].enabled && launcher->apps[i].on_close == behavior) {
            launcher_stop_app(launcher, launcher->apps[i].name);
        }
    }
}

/*
 * Status functions
 */

bool launcher_is_running(app_launcher *launcher, const char *name)
{
    app_profile *app = launcher_get_app(launcher, name);
    if (!app) return false;

    /* Update status before returning */
    if (app->process_handle) {
        DWORD exit_code;
        if (GetExitCodeProcess(app->process_handle, &exit_code)) {
            app->is_running = (exit_code == STILL_ACTIVE);
        } else {
            app->is_running = false;
        }

        /* Clean up handle if process has exited */
        if (!app->is_running) {
            CloseHandle(app->process_handle);
            app->process_handle = NULL;
            app->process_id = 0;
        }
    }

    return app->is_running;
}

void launcher_update_status(app_launcher *launcher)
{
    if (!launcher) return;

    for (int i = 0; i < launcher->app_count; i++) {
        app_profile *app = &launcher->apps[i];

        if (app->process_handle) {
            DWORD exit_code;
            if (GetExitCodeProcess(app->process_handle, &exit_code)) {
                app->is_running = (exit_code == STILL_ACTIVE);
            } else {
                app->is_running = false;
            }

            /* Clean up handle if process has exited */
            if (!app->is_running) {
                CloseHandle(app->process_handle);
                app->process_handle = NULL;
                app->process_id = 0;
            }
        }
    }
}

/*
 * Persistence functions
 */

bool launcher_load_config(app_launcher *launcher, const char *filename)
{
    if (!launcher || !filename) return false;

    json_value *root = json_parse_file(filename);
    if (!root) return false;

    if (json_get_type(root) != JSON_OBJECT) {
        json_free(root);
        return false;
    }

    json_value *apps_array = json_object_get(root, "apps");
    if (!apps_array || json_get_type(apps_array) != JSON_ARRAY) {
        json_free(root);
        return false;
    }

    int count = json_array_length(apps_array);
    for (int i = 0; i < count; i++) {
        json_value *app_obj = json_array_get(apps_array, i);
        if (!app_obj || json_get_type(app_obj) != JSON_OBJECT) {
            continue;
        }

        app_profile profile;
        memset(&profile, 0, sizeof(profile));

        /* Read required fields */
        json_value *val;

        val = json_object_get(app_obj, "name");
        if (val && json_get_type(val) == JSON_STRING) {
            strncpy(profile.name, json_get_string(val), sizeof(profile.name) - 1);
        } else {
            continue; /* Name is required */
        }

        val = json_object_get(app_obj, "exe_path");
        if (val && json_get_type(val) == JSON_STRING) {
            strncpy(profile.exe_path, json_get_string(val), sizeof(profile.exe_path) - 1);
        } else {
            continue; /* exe_path is required */
        }

        /* Read optional fields */
        val = json_object_get(app_obj, "args");
        if (val && json_get_type(val) == JSON_STRING) {
            strncpy(profile.args, json_get_string(val), sizeof(profile.args) - 1);
        }

        val = json_object_get(app_obj, "working_dir");
        if (val && json_get_type(val) == JSON_STRING) {
            strncpy(profile.working_dir, json_get_string(val), sizeof(profile.working_dir) - 1);
        }

        val = json_object_get(app_obj, "trigger");
        if (val && json_get_type(val) == JSON_STRING) {
            profile.trigger = launcher_string_to_trigger(json_get_string(val));
        } else {
            profile.trigger = LAUNCH_ON_CONNECT;
        }

        val = json_object_get(app_obj, "on_close");
        if (val && json_get_type(val) == JSON_STRING) {
            profile.on_close = launcher_string_to_close(json_get_string(val));
        } else {
            profile.on_close = CLOSE_ON_IRACING_EXIT;
        }

        val = json_object_get(app_obj, "enabled");
        if (val && json_get_type(val) == JSON_BOOL) {
            profile.enabled = json_get_bool(val);
        } else {
            profile.enabled = true;
        }

        launcher_add_app(launcher, &profile);
    }

    json_free(root);
    return true;
}

bool launcher_save_config(const app_launcher *launcher, const char *filename)
{
    if (!launcher || !filename) return false;

    json_value *root = json_new_object();
    if (!root) return false;

    json_value *apps_array = json_new_array();
    if (!apps_array) {
        json_free(root);
        return false;
    }

    for (int i = 0; i < launcher->app_count; i++) {
        const app_profile *app = &launcher->apps[i];

        json_value *app_obj = json_new_object();
        if (!app_obj) continue;

        json_object_set(app_obj, "name", json_new_string(app->name));
        json_object_set(app_obj, "exe_path", json_new_string(app->exe_path));
        json_object_set(app_obj, "args", json_new_string(app->args));
        json_object_set(app_obj, "working_dir", json_new_string(app->working_dir));
        json_object_set(app_obj, "trigger",
                       json_new_string(launcher_trigger_to_string(app->trigger)));
        json_object_set(app_obj, "on_close",
                       json_new_string(launcher_close_to_string(app->on_close)));
        json_object_set(app_obj, "enabled", json_new_bool(app->enabled));

        json_array_push(apps_array, app_obj);
    }

    json_object_set(root, "apps", apps_array);

    bool result = json_write_file(root, filename, true);
    json_free(root);

    return result;
}
