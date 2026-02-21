/*
 * ira - iRacing Application
 * Console UI Backend
 *
 * Implements the ui_backend interface using printf-based console output
 * and Win32 Console API for input handling.
 *
 * Copyright (c) 2026 Christopher Griffiths
 */

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <time.h>

#include "ui.h"
#include "../filter/race_filter.h"
#include "../controller/app_controller.h"

#include "version.h"

/* External shutdown flag from main.c */
extern volatile bool g_running;

/* Console backend state (extends ui_backend) */
typedef struct {
    ui_backend base;    /* must be first for casting */
    HANDLE hOutput;
    WORD default_attrs;
} console_state;

/* Console color constants */
#define COL_DEFAULT     (FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE)
#define COL_BRIGHT      (COL_DEFAULT | FOREGROUND_INTENSITY)
#define COL_RED         (FOREGROUND_RED | FOREGROUND_INTENSITY)
#define COL_GREEN       (FOREGROUND_GREEN | FOREGROUND_INTENSITY)
#define COL_YELLOW      (FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_INTENSITY)
#define COL_CYAN        (FOREGROUND_BLUE | FOREGROUND_GREEN | FOREGROUND_INTENSITY)
#define COL_GRAY        (FOREGROUND_INTENSITY)
#define COL_DIM_RED     (FOREGROUND_RED)
#define COL_DIM_GREEN   (FOREGROUND_GREEN)
#define COL_DIM_CYAN    (FOREGROUND_BLUE | FOREGROUND_GREEN)

static console_state *get_state(ui_backend *ui) {
    return (console_state *)ui;
}

static void set_color(ui_backend *ui, WORD attrs) {
    SetConsoleTextAttribute(get_state(ui)->hOutput, attrs);
}

static void reset_color(ui_backend *ui) {
    console_state *s = get_state(ui);
    SetConsoleTextAttribute(s->hOutput, s->default_attrs);
}

/* --- Unit conversion helpers --- */

static float mps_to_kph(float mps)
{
    return mps * 3.6f;
}

static float mps_to_mph(float mps)
{
    return mps * 2.23694f;
}

/* --- Input helper --- */

static bool read_line(char *buf, int size)
{
    if (!fgets(buf, size, stdin)) return false;
    char *nl = strchr(buf, '\n');
    if (nl) *nl = '\0';
    return buf[0] != '\0';
}

/* Forward declarations */
static void console_show_error(ui_backend *ui, const char *message);

/* --- Display operations --- */

static void console_show_banner(ui_backend *ui)
{
    set_color(ui, COL_DIM_CYAN);
    printf("========================================\n");
    set_color(ui, COL_CYAN);
    printf("  ira - iRacing Application v%s\n", IRA_VERSION);
    set_color(ui, COL_GRAY);
    printf("  Copyright (c) 2026 Christopher Griffiths\n");
    set_color(ui, COL_DIM_CYAN);
    printf("========================================\n\n");
    reset_color(ui);
}

static void console_show_usage(ui_backend *ui, const char *program_name)
{
    (void)ui;
    printf("Usage: %s [options]\n\n", program_name);
    printf("Options:\n");
    printf("  -h, --help              Show this help message\n");
    printf("  -l, --log               Enable telemetry logging to CSV\n");
    printf("  -m, --metric            Use metric units (default)\n");
    printf("  -i, --imperial          Use imperial units\n");
    printf("  --log-dir <path>        Set telemetry log directory\n");
    printf("  --menu                  Open interactive configuration menu\n");
    printf("\n");
    printf("App Launcher:\n");
    printf("  --launch-apps           Launch all manual-trigger apps and exit\n");
    printf("  --list-apps             List configured apps and status\n");
    printf("  --add-app <name> <path> Add a new app to launch on iRacing connect\n");
    printf("\n");
    printf("Race Filter:\n");
    printf("  --races                 Show filtered races for current week\n");
    printf("  --races-all             Show all races (ignore filters)\n");
    printf("  --filter-status         Show current filter settings\n");
    printf("  --sync                  Sync data from iRacing API (requires auth)\n");
    printf("\n");
}

static void console_show_session_info(ui_backend *ui, const ui_session_info *info)
{
    set_color(ui, COL_DIM_CYAN);
    printf("----------------------------------------\n");
    reset_color(ui);

    set_color(ui, COL_CYAN);
    printf("Track: ");
    set_color(ui, COL_BRIGHT);
    printf("%s", info->track_name);
    if (info->track_config[0] != '\0') {
        set_color(ui, COL_GRAY);
        printf(" (%s)", info->track_config);
    }
    if (info->track_length_km > 0) {
        set_color(ui, COL_GRAY);
        printf(" - %.2f km", info->track_length_km);
    }
    printf("\n");

    if (info->car_name[0] != '\0') {
        set_color(ui, COL_CYAN);
        printf("Car:   ");
        set_color(ui, COL_BRIGHT);
        printf("%s\n", info->car_name);
    }
    if (info->driver_name[0] != '\0') {
        set_color(ui, COL_CYAN);
        printf("Driver: ");
        set_color(ui, COL_BRIGHT);
        printf("%s\n", info->driver_name);
    }

    set_color(ui, COL_DIM_CYAN);
    printf("----------------------------------------\n\n");
    reset_color(ui);
}

static void console_show_telemetry(ui_backend *ui, const ui_telemetry_data *telem)
{
    float speed_display;
    const char *speed_unit;
    if (telem->use_metric) {
        speed_display = mps_to_kph(telem->speed_mps);
        speed_unit = "kph";
    } else {
        speed_display = mps_to_mph(telem->speed_mps);
        speed_unit = "mph";
    }

    char gear_str[4];
    if (telem->gear == -1) {
        snprintf(gear_str, sizeof(gear_str), "R");
    } else if (telem->gear == 0) {
        snprintf(gear_str, sizeof(gear_str), "N");
    } else {
        snprintf(gear_str, sizeof(gear_str), "%d", telem->gear);
    }

    printf("\r");

    /* Speed */
    set_color(ui, COL_GRAY);
    printf("Speed:");
    set_color(ui, COL_BRIGHT);
    printf(" %6.1f %s", speed_display, speed_unit);

    set_color(ui, COL_DIM_CYAN);
    printf(" | ");

    /* RPM - color-coded by range */
    set_color(ui, COL_GRAY);
    printf("RPM:");
    if (telem->rpm > 8000.0f) {
        set_color(ui, COL_RED);
    } else if (telem->rpm > 6000.0f) {
        set_color(ui, COL_YELLOW);
    } else {
        set_color(ui, COL_GREEN);
    }
    printf(" %6.0f", telem->rpm);

    set_color(ui, COL_DIM_CYAN);
    printf(" | ");

    /* Gear */
    set_color(ui, COL_GRAY);
    printf("Gear:");
    set_color(ui, COL_BRIGHT);
    printf(" %s", gear_str);

    set_color(ui, COL_DIM_CYAN);
    printf(" | ");

    /* Throttle */
    set_color(ui, COL_GRAY);
    printf("Thr:");
    set_color(ui, COL_GREEN);
    printf(" %3.0f%%", telem->throttle * 100.0f);

    set_color(ui, COL_DIM_CYAN);
    printf(" | ");

    /* Brake */
    set_color(ui, COL_GRAY);
    printf("Brk:");
    set_color(ui, telem->brake > 0.0f ? COL_RED : COL_GRAY);
    printf(" %3.0f%%", telem->brake * 100.0f);

    set_color(ui, COL_DIM_CYAN);
    printf(" | ");

    /* Lap */
    set_color(ui, COL_GRAY);
    printf("Lap:");
    set_color(ui, COL_BRIGHT);
    printf(" %d", telem->lap);
    set_color(ui, COL_GRAY);
    printf(" (%.1f%%)", telem->lap_dist_pct * 100.0f);

    set_color(ui, COL_DIM_CYAN);
    printf(" | ");

    /* Fuel - warn when low, gray when unavailable */
    set_color(ui, COL_GRAY);
    printf("Fuel:");
    if (telem->fuel_level < 0.0f) {
        set_color(ui, COL_GRAY);
        printf(" --.-L");
    } else {
        if (telem->fuel_level < 2.0f) {
            set_color(ui, COL_RED);
        } else if (telem->fuel_level < 5.0f) {
            set_color(ui, COL_YELLOW);
        } else {
            set_color(ui, COL_GREEN);
        }
        printf(" %.1fL", telem->fuel_level);
    }

    reset_color(ui);
    printf("   ");
    fflush(stdout);
}

static void console_show_filter_status(ui_backend *ui, ira_database *db)
{
    if (!db) {
        console_show_error(ui, "Database not initialized");
        return;
    }

    ira_filter *f = &db->filter;

    set_color(ui, COL_CYAN);
    printf("Race Filter Settings\n");
    set_color(ui, COL_DIM_CYAN);
    printf("========================================\n");
    reset_color(ui);
    printf("Owned content only: %s\n", f->owned_content_only ? "yes" : "no");
    printf("License range:      %s - %s\n",
           license_to_string(f->min_license),
           license_to_string(f->max_license));

    printf("Categories:         ");
    if (f->category_count == 0) {
        printf("all");
    } else {
        for (int i = 0; i < f->category_count; i++) {
            if (i > 0) printf(", ");
            printf("%s", category_to_string(f->categories[i]));
        }
    }
    printf("\n");

    printf("Setup type:         ");
    if (f->fixed_setup_only) printf("fixed only");
    else if (f->open_setup_only) printf("open only");
    else printf("any");
    printf("\n");

    printf("Official only:      %s\n", f->official_only ? "yes" : "no");

    printf("Race duration:      ");
    if (f->min_race_mins > 0 || f->max_race_mins > 0) {
        if (f->min_race_mins > 0) printf("%d min", f->min_race_mins);
        else printf("any");
        printf(" - ");
        if (f->max_race_mins > 0) printf("%d min", f->max_race_mins);
        else printf("any");
    } else {
        printf("any");
    }
    printf("\n");

    printf("Excluded series:    %d\n", f->excluded_series_count);
    printf("Excluded tracks:    %d\n", f->excluded_track_count);
    set_color(ui, COL_DIM_CYAN);
    printf("========================================\n\n");
    reset_color(ui);

    set_color(ui, COL_CYAN);
    printf("Data Status:\n");
    reset_color(ui);
    printf("  Tracks:  %d loaded", db->track_count);
    if (db->tracks_updated > 0) {
        printf(" (updated: %s", ctime(&db->tracks_updated));
        /* ctime adds newline, remove it */
        printf("\b)");
    }
    printf("\n");

    printf("  Cars:    %d loaded", db->car_count);
    if (db->cars_updated > 0) {
        printf(" (updated: %s", ctime(&db->cars_updated));
        printf("\b)");
    }
    printf("\n");

    printf("  Seasons: %d loaded", db->season_count);
    if (db->seasons_updated > 0) {
        printf(" (updated: %s", ctime(&db->seasons_updated));
        printf("\b)");
    }
    printf("\n");

    printf("  Owned cars:   %d\n", db->owned.owned_car_count);
    printf("  Owned tracks: %d\n", db->owned.owned_track_count);
    printf("\n");

    printf("Config file: %s\n", database_get_filter_path());
}

/* Get color for a race category */
static WORD category_color(race_category cat)
{
    switch (cat) {
        case CATEGORY_OVAL:      return FOREGROUND_BLUE | FOREGROUND_GREEN | FOREGROUND_INTENSITY;
        case CATEGORY_ROAD:      return FOREGROUND_GREEN | FOREGROUND_INTENSITY;
        case CATEGORY_DIRT_OVAL: return FOREGROUND_RED | FOREGROUND_GREEN;
        case CATEGORY_DIRT_ROAD: return FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_INTENSITY;
        case CATEGORY_SPORTS_CAR: return FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_INTENSITY;
        case CATEGORY_FORMULA:    return FOREGROUND_RED | FOREGROUND_BLUE | FOREGROUND_INTENSITY;
        default:                 return FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE;
    }
}

static void console_show_races(ui_backend *ui, ira_database *db, bool show_all)
{
    if (!db) {
        console_show_error(ui, "Database not initialized");
        return;
    }

    if (db->season_count == 0) {
        printf("No season data loaded.\n");
        printf("Use --sync to fetch data from iRacing API (requires authentication).\n");
        printf("Or manually place data files in: %s\n", database_get_seasons_path());
        return;
    }

    /* Create filter results */
    filter_results *results = filter_results_create();
    if (!results) {
        printf("Error: Could not create filter results\n");
        return;
    }

    /* Apply filter (show_all bypasses all filter criteria) */
    if (show_all) {
        filter_apply_show_all(db, results);
    } else {
        filter_apply(db, results);
    }

    /* Sort by series name */
    filter_results_sort(results, SORT_BY_CATEGORY, true);

    /* Display results */
    set_color(ui, COL_CYAN);
    printf("Races for Current Week\n");
    set_color(ui, COL_DIM_CYAN);
    printf("========================================\n");
    reset_color(ui);

    if (results->race_count == 0) {
        printf("No races found.\n");
    } else {
        race_category last_cat = CATEGORY_UNKNOWN;

        for (int i = 0; i < results->race_count; i++) {
            filtered_race *race = &results->races[i];

            /* Skip failed matches unless showing all */
            if (!show_all && race->match != MATCH_OK) {
                continue;
            }

            /* Print category header */
            race_category cat = race->series ? race->series->category : CATEGORY_UNKNOWN;
            if (cat != last_cat) {
                set_color(ui, category_color(cat));
                printf("\n--- %s ---\n", category_to_string(cat));
                reset_color(ui);
                last_cat = cat;
            }

            /* Series name */
            const char *series_name = race->series ? race->series->series_name : "Unknown Series";
            set_color(ui, COL_BRIGHT);
            printf("\n%s\n", series_name);
            reset_color(ui);

            /* Track info */
            if (race->track) {
                printf("  Track:    %s", race->track->track_name);
                if (race->track->config_name[0]) {
                    printf(" (%s)", race->track->config_name);
                }
                printf("\n");
            } else if (race->week) {
                printf("  Track:    %s", race->week->track_name);
                if (race->week->config_name[0]) {
                    printf(" (%s)", race->week->config_name);
                }
                printf("\n");
            }

            /* Duration */
            char duration[32];
            if (race->week) {
                filter_format_duration(race->week, duration, sizeof(duration));
                printf("  Duration: %s\n", duration);
            }

            /* License */
            if (race->series) {
                printf("  License:  %s\n", license_to_string(race->series->min_license));
            }

            /* Setup type */
            if (race->season) {
                printf("  Setup:    %s\n", race->season->fixed_setup ? "Fixed" : "Open");
            }

            /* Ownership status - color-coded */
            printf("  Owned:    Car: ");
            set_color(ui, race->owns_car ? COL_GREEN : COL_RED);
            printf("%s", race->owns_car ? "yes" : "NO");
            reset_color(ui);
            printf(", Track: ");
            set_color(ui, race->owns_track ? COL_GREEN : COL_RED);
            printf("%s", race->owns_track ? "yes" : "NO");
            reset_color(ui);
            printf("\n");

            /* Filter status (if showing all) */
            if (show_all && race->match != MATCH_OK) {
                set_color(ui, COL_YELLOW);
                printf("  Filter:   %s\n", filter_match_to_string(race->match));
                reset_color(ui);
            }
        }
    }

    set_color(ui, COL_DIM_CYAN);
    printf("\n========================================\n");
    reset_color(ui);
    printf("Total: %d checked, ", results->total_checked);
    set_color(ui, COL_GREEN);
    printf("%d passed filter\n", results->passed_count);
    reset_color(ui);

    set_color(ui, COL_GRAY);
    if (results->failed_ownership > 0) {
        printf("  %d failed: missing content\n", results->failed_ownership);
    }
    if (results->failed_category > 0) {
        printf("  %d failed: wrong category\n", results->failed_category);
    }
    if (results->failed_license > 0) {
        printf("  %d failed: license mismatch\n", results->failed_license);
    }
    if (results->failed_other > 0) {
        printf("  %d failed: other reasons\n", results->failed_other);
    }
    reset_color(ui);

    filter_results_destroy(results);
}

static void console_show_apps(ui_backend *ui, app_launcher *launcher)
{
    int count = launcher_get_app_count(launcher);
    if (count == 0) {
        printf("No apps configured.\n");
        set_color(ui, COL_GRAY);
        printf("Add apps to: %s\n", config_get_apps_path());
        reset_color(ui);
        return;
    }

    set_color(ui, COL_CYAN);
    printf("Configured apps (%d):\n", count);
    set_color(ui, COL_DIM_CYAN);
    printf("----------------------------------------\n");
    reset_color(ui);

    for (int i = 0; i < count; i++) {
        app_profile *app = launcher_get_app_at(launcher, i);
        if (!app) continue;

        launcher_update_status(launcher);

        set_color(ui, COL_BRIGHT);
        printf("%d. %s\n", i + 1, app->name);
        reset_color(ui);
        set_color(ui, COL_GRAY);
        printf("   Path:    %s\n", app->exe_path);
        printf("   Trigger: %s\n", launcher_trigger_to_string(app->trigger));
        printf("   Close:   %s\n", launcher_close_to_string(app->on_close));
        reset_color(ui);
        printf("   Enabled: ");
        set_color(ui, app->enabled ? COL_GREEN : COL_GRAY);
        printf("%s\n", app->enabled ? "yes" : "no");
        reset_color(ui);
        printf("   Status:  ");
        set_color(ui, app->is_running ? COL_GREEN : COL_GRAY);
        printf("%s\n", app->is_running ? "RUNNING" : "stopped");
        reset_color(ui);
        printf("\n");
    }
}

static void console_show_settings(ui_backend *ui, const ira_config *cfg)
{
    (void)ui;
    printf("\n--- Current Settings ---\n");
    printf("Units:                %s\n", cfg->use_metric_units ? "metric" : "imperial");
    printf("Telemetry logging:    %s\n", cfg->telemetry_logging_enabled ? "enabled" : "disabled");
    printf("Log path:             %s\n", cfg->telemetry_log_path);

    const char *switch_str;
    switch (cfg->car_switch_behavior) {
        case CAR_SWITCH_AUTO:     switch_str = "auto"; break;
        case CAR_SWITCH_PROMPT:   switch_str = "prompt"; break;
        case CAR_SWITCH_DISABLED: switch_str = "disabled"; break;
        default:                  switch_str = "unknown"; break;
    }
    printf("Car switch behavior:  %s\n", switch_str);

    printf("\nConfig file: %s\n", config_get_default_path());
    printf("Apps file:   %s\n", config_get_apps_path());
    printf("Data dir:    %s\n", config_get_data_path());
}

/* --- Status messages --- */

static void console_show_status(ui_backend *ui, const char *message)
{
    (void)ui;
    printf("%s\n", message);
}

static void console_show_error(ui_backend *ui, const char *message)
{
    set_color(ui, COL_RED);
    printf("Error: ");
    reset_color(ui);
    printf("%s\n", message);
}

static void console_show_sync_progress(ui_backend *ui, const char *operation, bool success,
                                        const char *detail)
{
    printf("%s...\n", operation);
    set_color(ui, success ? COL_GREEN : COL_RED);
    printf("  %s\n", success ? "OK" : (detail ? detail : "FAILED"));
    reset_color(ui);
}

static void console_show_prompt(ui_backend *ui, const char *prompt)
{
    (void)ui;
    printf("%s", prompt);
    fflush(stdout);
}

/* --- Interactive menu helpers --- */

static void show_menu_prompt(ui_backend *ui)
{
    set_color(ui, COL_DIM_CYAN);
    printf("\n========================================\n");
    set_color(ui, COL_CYAN);
    printf("  ira - Configuration Menu\n");
    set_color(ui, COL_DIM_CYAN);
    printf("========================================\n");
    reset_color(ui);
    set_color(ui, COL_BRIGHT);
    printf("  [1]");
    reset_color(ui);
    printf(" List apps\n");
    set_color(ui, COL_BRIGHT);
    printf("  [2]");
    reset_color(ui);
    printf(" Add app\n");
    set_color(ui, COL_BRIGHT);
    printf("  [3]");
    reset_color(ui);
    printf(" Remove app\n");
    set_color(ui, COL_BRIGHT);
    printf("  [4]");
    reset_color(ui);
    printf(" Toggle app enabled/disabled\n");
    set_color(ui, COL_BRIGHT);
    printf("  [5]");
    reset_color(ui);
    printf(" Launch/stop app manually\n");
    set_color(ui, COL_BRIGHT);
    printf("  [6]");
    reset_color(ui);
    printf(" View settings\n");
    set_color(ui, COL_BRIGHT);
    printf("  [7]");
    reset_color(ui);
    printf(" Show filter status\n");
    set_color(ui, COL_BRIGHT);
    printf("  [8]");
    reset_color(ui);
    printf(" Show races\n");
    set_color(ui, COL_BRIGHT);
    printf("  [q]");
    reset_color(ui);
    printf(" Exit menu\n");
    set_color(ui, COL_DIM_CYAN);
    printf("----------------------------------------\n");
    reset_color(ui);
    printf("Select option: ");
    fflush(stdout);
}

static void menu_add_app_interactive(ui_backend *ui, app_launcher *launcher)
{
    char name[64];
    char path[MAX_PATH];
    char trigger_str[32];
    char close_str[32];
    char msg[128];

    ui->show_status(ui, "\n--- Add New App ---");

    ui->show_prompt(ui, "App name: ");
    if (!ui->read_line(ui, name, sizeof(name))) {
        ui->show_status(ui, "Cancelled.");
        return;
    }

    ui->show_prompt(ui, "Executable path: ");
    if (!ui->read_line(ui, path, sizeof(path))) {
        ui->show_status(ui, "Cancelled.");
        return;
    }

    ui->show_prompt(ui, "Trigger (on_connect/on_session/manual) [on_connect]: ");
    if (!ui->read_line(ui, trigger_str, sizeof(trigger_str))) {
        strcpy(trigger_str, "on_connect");
    }

    ui->show_prompt(ui, "Close behavior (on_iracing_exit/on_ira_exit/never) [on_iracing_exit]: ");
    if (!ui->read_line(ui, close_str, sizeof(close_str))) {
        strcpy(close_str, "on_iracing_exit");
    }

    const char *error = NULL;
    if (!app_controller_add(launcher, name, path,
                            launcher_string_to_trigger(trigger_str),
                            launcher_string_to_close(close_str), &error)) {
        ui->show_error(ui, error);
        return;
    }

    snprintf(msg, sizeof(msg), "Added '%s' successfully.", name);
    ui->show_status(ui, msg);
}

static void menu_remove_app_interactive(ui_backend *ui, app_launcher *launcher)
{
    char msg[128];
    int count = launcher_get_app_count(launcher);
    if (count == 0) {
        ui->show_status(ui, "\nNo apps configured.");
        return;
    }

    ui->show_status(ui, "\n--- Remove App ---");
    for (int i = 0; i < count; i++) {
        app_profile *app = launcher_get_app_at(launcher, i);
        if (app) {
            snprintf(msg, sizeof(msg), "  [%d] %s", i + 1, app->name);
            ui->show_status(ui, msg);
        }
    }
    ui->show_status(ui, "  [0] Cancel");

    ui->show_prompt(ui, "Select app to remove: ");

    char input[16];
    if (!ui->read_line(ui, input, sizeof(input))) {
        ui->show_status(ui, "Cancelled.");
        return;
    }

    int choice = atoi(input);
    if (choice == 0 || choice > count) {
        ui->show_status(ui, "Cancelled.");
        return;
    }

    app_profile *app = launcher_get_app_at(launcher, choice - 1);
    if (!app) {
        ui->show_error(ui, "Invalid selection.");
        return;
    }

    char name[64];
    strncpy(name, app->name, sizeof(name) - 1);
    name[sizeof(name) - 1] = '\0';

    const char *error = NULL;
    if (!app_controller_remove(launcher, name, &error)) {
        ui->show_error(ui, error);
        return;
    }

    snprintf(msg, sizeof(msg), "Removed '%s' successfully.", name);
    ui->show_status(ui, msg);
}

static void menu_toggle_app_interactive(ui_backend *ui, app_launcher *launcher)
{
    char msg[128];
    int count = launcher_get_app_count(launcher);
    if (count == 0) {
        ui->show_status(ui, "\nNo apps configured.");
        return;
    }

    ui->show_status(ui, "\n--- Toggle App Enabled/Disabled ---");
    for (int i = 0; i < count; i++) {
        app_profile *app = launcher_get_app_at(launcher, i);
        if (app) {
            snprintf(msg, sizeof(msg), "  [%d] %s (%s)", i + 1, app->name,
                     app->enabled ? "enabled" : "disabled");
            ui->show_status(ui, msg);
        }
    }
    ui->show_status(ui, "  [0] Cancel");

    ui->show_prompt(ui, "Select app to toggle: ");

    char input[16];
    if (!ui->read_line(ui, input, sizeof(input))) {
        ui->show_status(ui, "Cancelled.");
        return;
    }

    int choice = atoi(input);
    if (choice == 0 || choice > count) {
        ui->show_status(ui, "Cancelled.");
        return;
    }

    const char *error = NULL;
    if (!app_controller_toggle_enabled(launcher, choice - 1, &error)) {
        ui->show_error(ui, error);
        return;
    }

    app_profile *app = launcher_get_app_at(launcher, choice - 1);
    if (app) {
        snprintf(msg, sizeof(msg), "'%s' is now %s.", app->name,
                 app->enabled ? "enabled" : "disabled");
        ui->show_status(ui, msg);
    }
}

static void menu_launch_stop_app_interactive(ui_backend *ui, app_launcher *launcher)
{
    char msg[128];
    int count = launcher_get_app_count(launcher);
    if (count == 0) {
        ui->show_status(ui, "\nNo apps configured.");
        return;
    }

    launcher_update_status(launcher);

    ui->show_status(ui, "\n--- Launch/Stop App ---");
    for (int i = 0; i < count; i++) {
        app_profile *app = launcher_get_app_at(launcher, i);
        if (app) {
            snprintf(msg, sizeof(msg), "  [%d] %s (%s)", i + 1, app->name,
                     app->is_running ? "RUNNING" : "stopped");
            ui->show_status(ui, msg);
        }
    }
    ui->show_status(ui, "  [0] Cancel");

    ui->show_prompt(ui, "Select app: ");

    char input[16];
    if (!ui->read_line(ui, input, sizeof(input))) {
        ui->show_status(ui, "Cancelled.");
        return;
    }

    int choice = atoi(input);
    if (choice == 0 || choice > count) {
        ui->show_status(ui, "Cancelled.");
        return;
    }

    app_profile *app = launcher_get_app_at(launcher, choice - 1);
    if (!app) {
        ui->show_error(ui, "Invalid selection.");
        return;
    }

    bool was_running = false;
    const char *action = app->is_running ? "Stopping" : "Starting";
    snprintf(msg, sizeof(msg), "%s %s", action, app->name);
    bool ok = app_controller_launch_stop(launcher, choice - 1, &was_running);
    ui->show_sync_progress(ui, msg, ok, NULL);
}

/* --- Interactive menu --- */

static void console_run_menu(ui_backend *ui, app_launcher *launcher,
                              ira_config *cfg, ira_database **db_ptr)
{
    char echo[4];
    bool in_menu = true;

    /* Flush any pending input before showing menu */
    ui->flush_input(ui);

    while (in_menu && g_running) {
        show_menu_prompt(ui);
        int choice = ui->read_key(ui);
        snprintf(echo, sizeof(echo), "%c", choice);
        ui->show_status(ui, echo);

        switch (choice) {
            case '1':
                ui->show_status(ui, "");
                ui->show_apps(ui, launcher);
                break;
            case '2':
                menu_add_app_interactive(ui, launcher);
                break;
            case '3':
                menu_remove_app_interactive(ui, launcher);
                break;
            case '4':
                menu_toggle_app_interactive(ui, launcher);
                break;
            case '5':
                menu_launch_stop_app_interactive(ui, launcher);
                break;
            case '6':
                ui->show_settings(ui, cfg);
                break;
            case '7':
                if (!*db_ptr) {
                    ui->show_status(ui, "\nLoading database...");
                    *db_ptr = database_create();
                    if (*db_ptr) {
                        database_load_all(*db_ptr);
                    }
                }
                if (!*db_ptr) {
                    ui->show_error(ui, "Could not load database.");
                } else {
                    ui->show_status(ui, "");
                    ui->show_filter_status(ui, *db_ptr);
                }
                break;
            case '8':
                if (!*db_ptr) {
                    ui->show_status(ui, "\nLoading database...");
                    *db_ptr = database_create();
                    if (*db_ptr) {
                        database_load_all(*db_ptr);
                    }
                }
                if (!*db_ptr) {
                    ui->show_error(ui, "Could not load database.");
                } else {
                    ui->show_status(ui, "");
                    ui->show_races(ui, *db_ptr, false);
                }
                break;
            case 'q':
            case 'Q':
                in_menu = false;
                break;
            default:
                ui->show_status(ui, "Invalid option.");
                break;
        }

        if (in_menu && g_running) {
            ui->show_prompt(ui, "\nPress any key to continue...");
            ui->read_key(ui);
        }
    }
}

/* --- App launcher operations --- */

static void console_launch_apps(ui_backend *ui, app_launcher *launcher)
{
    char msg[128];
    int count = launcher_get_app_count(launcher);
    int launched = 0;

    for (int i = 0; i < count; i++) {
        app_profile *app = launcher_get_app_at(launcher, i);
        if (!app || !app->enabled || app->trigger != LAUNCH_MANUAL) {
            continue;
        }

        snprintf(msg, sizeof(msg), "Launching %s", app->name);
        bool ok = launcher_start_app(launcher, app->name);
        ui->show_sync_progress(ui, msg, ok, NULL);
        if (ok) launched++;
    }

    if (launched == 0) {
        ui->show_status(ui, "No manual-trigger apps to launch.");
    } else {
        snprintf(msg, sizeof(msg), "\nLaunched %d app(s).", launched);
        ui->show_status(ui, msg);
    }
}

static bool console_add_app(ui_backend *ui, app_launcher *launcher,
                             const char *name, const char *exe_path)
{
    char msg[MAX_PATH + 128];
    if (!launcher || !name || !exe_path) return false;

    const char *error = NULL;
    if (!app_controller_add(launcher, name, exe_path,
                            LAUNCH_ON_CONNECT, CLOSE_ON_IRACING_EXIT, &error)) {
        ui->show_error(ui, error);
        return false;
    }

    snprintf(msg, sizeof(msg), "Added '%s' -> %s", name, exe_path);
    ui->show_status(ui, msg);
    ui->show_status(ui, "Trigger: on_connect, Close: on_iracing_exit");
    snprintf(msg, sizeof(msg), "Config: %s", config_get_apps_path());
    ui->show_status(ui, msg);
    return true;
}

/* --- Console input --- */

static bool console_input_available(ui_backend *ui)
{
    (void)ui;
    HANDLE hInput = GetStdHandle(STD_INPUT_HANDLE);
    if (hInput == INVALID_HANDLE_VALUE) return false;

    /* Wait with 0 timeout - returns immediately */
    return WaitForSingleObject(hInput, 0) == WAIT_OBJECT_0;
}

static int console_read_key(ui_backend *ui)
{
    (void)ui;
    HANDLE hInput = GetStdHandle(STD_INPUT_HANDLE);
    if (hInput == INVALID_HANDLE_VALUE) return 0;

    /* Set console mode for reading */
    DWORD oldMode;
    GetConsoleMode(hInput, &oldMode);
    SetConsoleMode(hInput, ENABLE_PROCESSED_INPUT);

    INPUT_RECORD record;
    DWORD read;

    while (ReadConsoleInput(hInput, &record, 1, &read)) {
        if (record.EventType == KEY_EVENT &&
            record.Event.KeyEvent.bKeyDown &&
            record.Event.KeyEvent.uChar.AsciiChar != 0) {
            SetConsoleMode(hInput, oldMode);
            return record.Event.KeyEvent.uChar.AsciiChar;
        }
    }

    SetConsoleMode(hInput, oldMode);
    return 0;
}

static void console_flush_input(ui_backend *ui)
{
    (void)ui;
    HANDLE hInput = GetStdHandle(STD_INPUT_HANDLE);
    if (hInput != INVALID_HANDLE_VALUE) {
        FlushConsoleInputBuffer(hInput);
    }
    /* Also clear GetAsyncKeyState history */
    for (int vk = 0x08; vk <= 0x5A; vk++) {
        GetAsyncKeyState(vk);
    }
}

static bool console_read_line(ui_backend *ui, char *buf, int size)
{
    (void)ui;
    return read_line(buf, size);
}

static void console_show_waiting_dot(ui_backend *ui)
{
    (void)ui;
    printf(".");
    fflush(stdout);
}

/* --- Lifecycle --- */

static void console_destroy(ui_backend *ui)
{
    reset_color(ui);
    free(ui);
}

/* --- Factory --- */

ui_backend *ui_console_create(void)
{
    console_state *state = calloc(1, sizeof(console_state));
    if (!state) return NULL;

    /* Capture console handle and default attributes */
    state->hOutput = GetStdHandle(STD_OUTPUT_HANDLE);
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    if (GetConsoleScreenBufferInfo(state->hOutput, &csbi)) {
        state->default_attrs = csbi.wAttributes;
    } else {
        state->default_attrs = COL_DEFAULT;
    }

    ui_backend *ui = &state->base;

    /* Display operations */
    ui->show_banner = console_show_banner;
    ui->show_usage = console_show_usage;
    ui->show_session_info = console_show_session_info;
    ui->show_telemetry = console_show_telemetry;
    ui->show_filter_status = console_show_filter_status;
    ui->show_races = console_show_races;
    ui->show_apps = console_show_apps;
    ui->show_settings = console_show_settings;

    /* Status messages */
    ui->show_status = console_show_status;
    ui->show_error = console_show_error;
    ui->show_sync_progress = console_show_sync_progress;
    ui->show_prompt = console_show_prompt;

    /* Interactive operations */
    ui->run_menu = console_run_menu;
    ui->launch_apps = console_launch_apps;
    ui->add_app = console_add_app;

    /* Console input */
    ui->input_available = console_input_available;
    ui->read_key = console_read_key;
    ui->read_line = console_read_line;
    ui->flush_input = console_flush_input;
    ui->show_waiting_dot = console_show_waiting_dot;

    /* Lifecycle */
    ui->destroy = console_destroy;

    return ui;
}
