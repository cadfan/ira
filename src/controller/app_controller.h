/*
 * ira - iRacing Application
 * App Controller — Business logic for app management
 *
 * Separates app CRUD + persistence from UI display.
 * UI backends call these functions, then render the result.
 *
 * Copyright (c) 2026 Christopher Griffiths
 */

#ifndef IRA_APP_CONTROLLER_H
#define IRA_APP_CONTROLLER_H

#include <stdbool.h>
#include "../launcher/launcher.h"

/*
 * Add a new app to the launcher with the given profile settings.
 * Validates the name is unique, adds to launcher, saves config.
 * Returns true on success. Sets error_out (if non-NULL) on failure.
 */
bool app_controller_add(app_launcher *launcher, const char *name,
                        const char *exe_path, launch_trigger trigger,
                        close_behavior on_close, const char **error_out);

/*
 * Remove an app by name.
 * Removes from launcher and saves config.
 * Returns true on success. Sets error_out on failure.
 */
bool app_controller_remove(app_launcher *launcher, const char *name,
                           const char **error_out);

/*
 * Toggle an app's enabled/disabled state by index.
 * Saves config after toggling.
 * Returns true on success. Sets error_out on failure.
 */
bool app_controller_toggle_enabled(app_launcher *launcher, int index,
                                   const char **error_out);

/*
 * Launch or stop an app by index (based on current running state).
 * Returns true if the operation succeeded.
 */
bool app_controller_launch_stop(app_launcher *launcher, int index,
                                bool *was_running_out);

/*
 * Launch all enabled manual-trigger apps.
 * Returns the number of apps successfully launched.
 */
int app_controller_launch_manual(app_launcher *launcher);

#endif /* IRA_APP_CONTROLLER_H */
