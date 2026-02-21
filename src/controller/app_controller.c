/*
 * ira - iRacing Application
 * App Controller — Business logic for app management
 *
 * Copyright (c) 2026 Christopher Griffiths
 */

#include <string.h>
#include "app_controller.h"
#include "../util/config.h"

bool app_controller_add(app_launcher *launcher, const char *name,
                        const char *exe_path, launch_trigger trigger,
                        close_behavior on_close, const char **error_out)
{
    if (!launcher || !name || !exe_path) {
        if (error_out) *error_out = "Invalid arguments.";
        return false;
    }

    if (launcher_get_app(launcher, name)) {
        if (error_out) *error_out = "App already exists.";
        return false;
    }

    app_profile profile;
    memset(&profile, 0, sizeof(profile));
    strncpy(profile.name, name, sizeof(profile.name) - 1);
    strncpy(profile.exe_path, exe_path, sizeof(profile.exe_path) - 1);
    profile.trigger = trigger;
    profile.on_close = on_close;
    profile.enabled = true;

    if (!launcher_add_app(launcher, &profile)) {
        if (error_out) *error_out = "Could not add app.";
        return false;
    }

    if (!launcher_save_config(launcher, config_get_apps_path())) {
        if (error_out) *error_out = "Could not save configuration.";
        return false;
    }

    return true;
}

bool app_controller_remove(app_launcher *launcher, const char *name,
                           const char **error_out)
{
    if (!launcher || !name) {
        if (error_out) *error_out = "Invalid arguments.";
        return false;
    }

    if (!launcher_remove_app(launcher, name)) {
        if (error_out) *error_out = "Could not remove app.";
        return false;
    }

    if (!launcher_save_config(launcher, config_get_apps_path())) {
        if (error_out) *error_out = "Could not save configuration.";
        return false;
    }

    return true;
}

bool app_controller_toggle_enabled(app_launcher *launcher, int index,
                                   const char **error_out)
{
    if (!launcher) {
        if (error_out) *error_out = "Invalid arguments.";
        return false;
    }

    app_profile *app = launcher_get_app_at(launcher, index);
    if (!app) {
        if (error_out) *error_out = "Invalid selection.";
        return false;
    }

    app->enabled = !app->enabled;

    if (!launcher_save_config(launcher, config_get_apps_path())) {
        app->enabled = !app->enabled;  /* rollback on save failure */
        if (error_out) *error_out = "Could not save configuration.";
        return false;
    }

    return true;
}

bool app_controller_launch_stop(app_launcher *launcher, int index,
                                bool *was_running_out)
{
    if (!launcher) return false;

    app_profile *app = launcher_get_app_at(launcher, index);
    if (!app) return false;

    bool was_running = app->is_running;
    if (was_running_out) *was_running_out = was_running;

    if (was_running) {
        return launcher_stop_app(launcher, app->name);
    } else {
        return launcher_start_app(launcher, app->name);
    }
}

int app_controller_launch_manual(app_launcher *launcher)
{
    if (!launcher) return 0;

    int count = launcher_get_app_count(launcher);
    int launched = 0;

    for (int i = 0; i < count; i++) {
        app_profile *app = launcher_get_app_at(launcher, i);
        if (!app || !app->enabled || app->trigger != LAUNCH_MANUAL) {
            continue;
        }
        if (launcher_start_app(launcher, app->name)) {
            launched++;
        }
    }

    return launched;
}
