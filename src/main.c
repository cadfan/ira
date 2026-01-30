/*
 * ira - iRacing Application
 * Main Entry Point
 *
 * Copyright (c) 2026 Christopher Griffiths
 */

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <stdbool.h>
#include <string.h>

#include "irsdk/irsdk.h"
#include "irsdk/yaml_parser.h"
#include "util/config.h"
#include "telemetry/telemetry_log.h"
#include "launcher/launcher.h"
#include "data/database.h"
#include "data/models.h"
#include "filter/race_filter.h"
#include "api/iracing_api.h"

/* Version info */
#define IRA_VERSION_MAJOR   0
#define IRA_VERSION_MINOR   2
#define IRA_VERSION_PATCH   0

/* Global flag for clean shutdown */
static volatile bool g_running = true;

/* Signal handler for Ctrl+C */
static void signal_handler(int sig)
{
    (void)sig;
    g_running = false;
    printf("\nShutting down...\n");
}

/* Application state for launcher integration */
typedef enum {
    STATE_WAITING,      /* Waiting for iRacing */
    STATE_CONNECTED,    /* iRacing running, not in session */
    STATE_IN_SESSION    /* In car/session with telemetry */
} ira_state;

/* Convert meters per second to km/h */
static float mps_to_kph(float mps)
{
    return mps * 3.6f;
}

/* Convert meters per second to mph */
static float mps_to_mph(float mps)
{
    return mps * 2.23694f;
}

/* Print application banner */
static void print_banner(void)
{
    printf("========================================\n");
    printf("  ira - iRacing Application v%d.%d.%d\n",
           IRA_VERSION_MAJOR, IRA_VERSION_MINOR, IRA_VERSION_PATCH);
    printf("  Copyright (c) 2026 Christopher Griffiths\n");
    printf("========================================\n\n");
}

/* Session info structure */
typedef struct {
    char track_name[128];
    char track_config[64];
    char car_name[64];
    char driver_name[64];
    int driver_car_idx;
    float track_length_km;
    int car_id;
    int track_id;
} SessionInfo;

/* Telemetry variable offsets (cached for performance) */
typedef struct {
    int speed;
    int rpm;
    int gear;
    int throttle;
    int brake;
    int clutch;
    int lap;
    int lap_dist_pct;
    int session_time;
    int fuel_level;
    int is_on_track;
} TelemetryOffsets;

/* Parse session info from YAML */
static bool parse_session_info(SessionInfo *info)
{
    const char *yaml = irsdk_get_session_info();
    if (!yaml) {
        return false;
    }

    memset(info, 0, sizeof(SessionInfo));

    /* Parse track info */
    yaml_parse_string(yaml, "WeekendInfo:TrackDisplayName", info->track_name, sizeof(info->track_name));
    if (info->track_name[0] == '\0') {
        yaml_parse_string(yaml, "WeekendInfo:TrackName", info->track_name, sizeof(info->track_name));
    }
    yaml_parse_string(yaml, "WeekendInfo:TrackConfigName", info->track_config, sizeof(info->track_config));

    /* Parse track length */
    char track_len_str[32];
    if (yaml_parse_string(yaml, "WeekendInfo:TrackLength", track_len_str, sizeof(track_len_str))) {
        /* Format is usually "X.XX km" */
        info->track_length_km = (float)atof(track_len_str);
    }

    /* Parse driver info */
    yaml_parse_int(yaml, "DriverInfo:DriverCarIdx", &info->driver_car_idx);

    /* Build path for driver-specific info */
    char path[128];
    snprintf(path, sizeof(path), "DriverInfo:Drivers:CarIdx:{%d}UserName", info->driver_car_idx);
    yaml_parse_string(yaml, path, info->driver_name, sizeof(info->driver_name));

    snprintf(path, sizeof(path), "DriverInfo:Drivers:CarIdx:{%d}CarScreenName", info->driver_car_idx);
    yaml_parse_string(yaml, path, info->car_name, sizeof(info->car_name));
    if (info->car_name[0] == '\0') {
        snprintf(path, sizeof(path), "DriverInfo:Drivers:CarIdx:{%d}CarPath", info->driver_car_idx);
        yaml_parse_string(yaml, path, info->car_name, sizeof(info->car_name));
    }

    /* Parse car ID */
    snprintf(path, sizeof(path), "DriverInfo:Drivers:CarIdx:{%d}CarID", info->driver_car_idx);
    yaml_parse_int(yaml, path, &info->car_id);

    /* Parse track ID */
    yaml_parse_int(yaml, "WeekendInfo:TrackID", &info->track_id);

    return info->track_name[0] != '\0';
}

/* Display session info */
static void display_session_info(const SessionInfo *info)
{
    printf("----------------------------------------\n");
    printf("Track: %s", info->track_name);
    if (info->track_config[0] != '\0') {
        printf(" (%s)", info->track_config);
    }
    if (info->track_length_km > 0) {
        printf(" - %.2f km", info->track_length_km);
    }
    printf("\n");

    if (info->car_name[0] != '\0') {
        printf("Car:   %s\n", info->car_name);
    }
    if (info->driver_name[0] != '\0') {
        printf("Driver: %s\n", info->driver_name);
    }
    printf("----------------------------------------\n\n");
}

/* Initialize telemetry offsets */
static bool init_telemetry_offsets(TelemetryOffsets *offsets)
{
    offsets->speed = irsdk_var_name_to_offset("Speed");
    offsets->rpm = irsdk_var_name_to_offset("RPM");
    offsets->gear = irsdk_var_name_to_offset("Gear");
    offsets->throttle = irsdk_var_name_to_offset("Throttle");
    offsets->brake = irsdk_var_name_to_offset("Brake");
    offsets->clutch = irsdk_var_name_to_offset("Clutch");
    offsets->lap = irsdk_var_name_to_offset("Lap");
    offsets->lap_dist_pct = irsdk_var_name_to_offset("LapDistPct");
    offsets->session_time = irsdk_var_name_to_offset("SessionTime");
    offsets->fuel_level = irsdk_var_name_to_offset("FuelLevel");
    offsets->is_on_track = irsdk_var_name_to_offset("IsOnTrack");

    /* Check that essential variables were found */
    if (offsets->speed < 0 || offsets->rpm < 0 || offsets->gear < 0) {
        return false;
    }

    return true;
}

/* Display telemetry data */
static void display_telemetry(const char *data, const TelemetryOffsets *offsets, bool use_metric)
{
    float speed_mps = irsdk_get_var_float(data, offsets->speed, 0);
    float rpm = irsdk_get_var_float(data, offsets->rpm, 0);
    int gear = irsdk_get_var_int(data, offsets->gear, 0);
    float throttle = irsdk_get_var_float(data, offsets->throttle, 0);
    float brake = irsdk_get_var_float(data, offsets->brake, 0);
    int lap = irsdk_get_var_int(data, offsets->lap, 0);
    float lap_pct = irsdk_get_var_float(data, offsets->lap_dist_pct, 0);
    float fuel = irsdk_get_var_float(data, offsets->fuel_level, 0);

    /* Convert speed */
    float speed_display;
    const char *speed_unit;
    if (use_metric) {
        speed_display = mps_to_kph(speed_mps);
        speed_unit = "kph";
    } else {
        speed_display = mps_to_mph(speed_mps);
        speed_unit = "mph";
    }

    /* Gear display */
    char gear_str[4];
    if (gear == -1) {
        snprintf(gear_str, sizeof(gear_str), "R");
    } else if (gear == 0) {
        snprintf(gear_str, sizeof(gear_str), "N");
    } else {
        snprintf(gear_str, sizeof(gear_str), "%d", gear);
    }

    /* Clear line and print telemetry */
    printf("\r");
    printf("Speed: %6.1f %s | RPM: %6.0f | Gear: %s | ",
           speed_display, speed_unit, rpm, gear_str);
    printf("Throttle: %3.0f%% | Brake: %3.0f%% | ",
           throttle * 100.0f, brake * 100.0f);
    printf("Lap: %d (%.1f%%) | Fuel: %.1fL   ",
           lap, lap_pct * 100.0f, fuel);
    fflush(stdout);
}

/* Print usage information */
static void print_usage(const char *program_name)
{
    printf("Usage: %s [options]\n\n", program_name);
    printf("Options:\n");
    printf("  -h, --help              Show this help message\n");
    printf("  -l, --log               Enable telemetry logging to CSV\n");
    printf("  -m, --metric            Use metric units (default)\n");
    printf("  -i, --imperial          Use imperial units\n");
    printf("  --log-dir <path>        Set telemetry log directory\n");
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

/* List all configured apps and their status */
static void list_apps(app_launcher *launcher)
{
    int count = launcher_get_app_count(launcher);
    if (count == 0) {
        printf("No apps configured.\n");
        printf("Add apps to: %s\n", config_get_apps_path());
        return;
    }

    printf("Configured apps (%d):\n", count);
    printf("----------------------------------------\n");

    for (int i = 0; i < count; i++) {
        app_profile *app = launcher_get_app_at(launcher, i);
        if (!app) continue;

        launcher_update_status(launcher);

        printf("%d. %s\n", i + 1, app->name);
        printf("   Path:    %s\n", app->exe_path);
        printf("   Trigger: %s\n", launcher_trigger_to_string(app->trigger));
        printf("   Close:   %s\n", launcher_close_to_string(app->on_close));
        printf("   Enabled: %s\n", app->enabled ? "yes" : "no");
        printf("   Status:  %s\n", app->is_running ? "RUNNING" : "stopped");
        printf("\n");
    }
}

/* Launch all manual-trigger apps */
static void launch_manual_apps(app_launcher *launcher)
{
    int count = launcher_get_app_count(launcher);
    int launched = 0;

    for (int i = 0; i < count; i++) {
        app_profile *app = launcher_get_app_at(launcher, i);
        if (!app || !app->enabled || app->trigger != LAUNCH_MANUAL) {
            continue;
        }

        printf("Launching %s...", app->name);
        if (launcher_start_app(launcher, app->name)) {
            printf(" OK\n");
            launched++;
        } else {
            printf(" FAILED\n");
        }
    }

    if (launched == 0) {
        printf("No manual-trigger apps to launch.\n");
    } else {
        printf("\nLaunched %d app(s).\n", launched);
    }
}

/* Add a new app to the configuration */
static bool add_app(app_launcher *launcher, const char *name, const char *exe_path)
{
    if (!launcher || !name || !exe_path) return false;

    /* Check if already exists */
    if (launcher_get_app(launcher, name)) {
        printf("Error: App '%s' already exists.\n", name);
        return false;
    }

    /* Create profile with defaults */
    app_profile profile;
    memset(&profile, 0, sizeof(profile));

    strncpy(profile.name, name, sizeof(profile.name) - 1);
    strncpy(profile.exe_path, exe_path, sizeof(profile.exe_path) - 1);
    profile.trigger = LAUNCH_ON_CONNECT;
    profile.on_close = CLOSE_ON_IRACING_EXIT;
    profile.enabled = true;

    if (!launcher_add_app(launcher, &profile)) {
        printf("Error: Could not add app.\n");
        return false;
    }

    /* Save configuration */
    if (!launcher_save_config(launcher, config_get_apps_path())) {
        printf("Error: Could not save configuration.\n");
        return false;
    }

    printf("Added '%s' -> %s\n", name, exe_path);
    printf("Trigger: on_connect, Close: on_iracing_exit\n");
    printf("Config: %s\n", config_get_apps_path());
    return true;
}

/* Create default apps.json with example configuration */
static void create_default_apps_config(void)
{
    const char *apps_path = config_get_apps_path();

    /* Check if file already exists */
    FILE *f = fopen(apps_path, "r");
    if (f) {
        fclose(f);
        return; /* Already exists */
    }

    /* Create example configuration */
    f = fopen(apps_path, "w");
    if (!f) return;

    fprintf(f, "{\n");
    fprintf(f, "  \"apps\": [\n");
    fprintf(f, "    {\n");
    fprintf(f, "      \"name\": \"Example App\",\n");
    fprintf(f, "      \"exe_path\": \"C:\\\\Path\\\\To\\\\App.exe\",\n");
    fprintf(f, "      \"args\": \"\",\n");
    fprintf(f, "      \"working_dir\": \"\",\n");
    fprintf(f, "      \"trigger\": \"on_connect\",\n");
    fprintf(f, "      \"on_close\": \"on_iracing_exit\",\n");
    fprintf(f, "      \"enabled\": false\n");
    fprintf(f, "    }\n");
    fprintf(f, "  ]\n");
    fprintf(f, "}\n");
    fclose(f);
}

/* Display filter status/settings */
static void show_filter_status(ira_database *db)
{
    if (!db) {
        printf("Error: Database not initialized\n");
        return;
    }

    ira_filter *f = &db->filter;

    printf("Race Filter Settings\n");
    printf("========================================\n");
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
    printf("========================================\n\n");

    printf("Data Status:\n");
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

/* Display filtered races */
static void show_races(ira_database *db, bool show_all)
{
    if (!db) {
        printf("Error: Database not initialized\n");
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

    /* Temporarily disable filter if showing all */
    ira_filter saved_filter;
    if (show_all) {
        saved_filter = db->filter;
        db->filter.owned_content_only = false;
        db->filter.category_count = 0;
        db->filter.min_license = LICENSE_ROOKIE;
        db->filter.max_license = LICENSE_PRO_WC;
        db->filter.fixed_setup_only = false;
        db->filter.open_setup_only = false;
        db->filter.official_only = false;
        db->filter.min_race_mins = 0;
        db->filter.max_race_mins = 0;
    }

    /* Apply filter */
    filter_apply(db, results);

    /* Restore filter if we modified it */
    if (show_all) {
        db->filter = saved_filter;
    }

    /* Sort by series name */
    filter_results_sort(results, SORT_BY_CATEGORY, true);

    /* Display results */
    printf("Races for Current Week\n");
    printf("========================================\n");

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
                printf("\n--- %s ---\n", category_to_string(cat));
                last_cat = cat;
            }

            /* Series name */
            const char *series_name = race->series ? race->series->series_name : "Unknown Series";
            printf("\n%s\n", series_name);

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

            /* Ownership status */
            printf("  Owned:    Car: %s, Track: %s\n",
                   race->owns_car ? "yes" : "NO",
                   race->owns_track ? "yes" : "NO");

            /* Filter status (if showing all) */
            if (show_all && race->match != MATCH_OK) {
                printf("  Filter:   %s\n", filter_match_to_string(race->match));
            }
        }
    }

    printf("\n========================================\n");
    printf("Total: %d checked, %d passed filter\n",
           results->total_checked, results->passed_count);

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

    filter_results_destroy(results);
}

/* Sync data from iRacing API */
static void sync_data(ira_database *db)
{
    if (!db) {
        printf("Error: Database not initialized\n");
        return;
    }

    printf("Syncing data from iRacing API...\n\n");

    iracing_api *api = api_create();
    if (!api) {
        printf("Error: Could not create API client\n");
        return;
    }

    /* Try to load saved tokens */
    /* TODO: token file path */

    /* Authenticate */
    api_error err = api_authenticate(api);
    if (err != API_OK) {
        printf("Authentication: %s\n", api_get_last_error(api));
        printf("\nNote: iRacing API access requires OAuth approval.\n");
        printf("Once approved, credentials can be set via config file.\n");
        api_destroy(api);
        return;
    }

    /* Fetch data */
    printf("Fetching cars...\n");
    err = api_fetch_cars(api, db);
    printf("  %s\n", err == API_OK ? "OK" : api_error_string(err));

    printf("Fetching tracks...\n");
    err = api_fetch_tracks(api, db);
    printf("  %s\n", err == API_OK ? "OK" : api_error_string(err));

    printf("Fetching series...\n");
    err = api_fetch_series(api, db);
    printf("  %s\n", err == API_OK ? "OK" : api_error_string(err));

    printf("Fetching seasons...\n");
    time_t now = time(NULL);
    struct tm *tm = localtime(&now);
    int year = tm->tm_year + 1900;
    int quarter = (tm->tm_mon / 3) + 1;
    err = api_fetch_seasons(api, db, year, quarter);
    printf("  %s\n", err == API_OK ? "OK" : api_error_string(err));

    printf("Fetching owned content...\n");
    err = api_fetch_owned_content(api, db);
    printf("  %s\n", err == API_OK ? "OK" : api_error_string(err));

    /* Save data */
    printf("\nSaving data...\n");
    database_save_all(db);

    api_destroy(api);
    printf("\nSync complete.\n");
}

int main(int argc, char *argv[])
{
    print_banner();

    /* Load configuration */
    ira_config cfg;
    config_init_defaults(&cfg);
    config_load_default(&cfg);

    /* Parse command line arguments */
    bool enable_logging = cfg.telemetry_logging_enabled;
    bool do_launch_apps = false;
    bool do_list_apps = false;
    bool do_add_app = false;
    bool do_show_races = false;
    bool do_show_races_all = false;
    bool do_filter_status = false;
    bool do_sync = false;
    const char *add_app_name = NULL;
    const char *add_app_path = NULL;
    char log_dir[260];
    strncpy(log_dir, cfg.telemetry_log_path, sizeof(log_dir) - 1);

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            print_usage(argv[0]);
            return 0;
        } else if (strcmp(argv[i], "-l") == 0 || strcmp(argv[i], "--log") == 0) {
            enable_logging = true;
        } else if (strcmp(argv[i], "-m") == 0 || strcmp(argv[i], "--metric") == 0) {
            cfg.use_metric_units = true;
        } else if (strcmp(argv[i], "-i") == 0 || strcmp(argv[i], "--imperial") == 0) {
            cfg.use_metric_units = false;
        } else if (strcmp(argv[i], "--log-dir") == 0 && i + 1 < argc) {
            strncpy(log_dir, argv[++i], sizeof(log_dir) - 1);
        } else if (strcmp(argv[i], "--launch-apps") == 0) {
            do_launch_apps = true;
        } else if (strcmp(argv[i], "--list-apps") == 0) {
            do_list_apps = true;
        } else if (strcmp(argv[i], "--add-app") == 0 && i + 2 < argc) {
            do_add_app = true;
            add_app_name = argv[++i];
            add_app_path = argv[++i];
        } else if (strcmp(argv[i], "--races") == 0) {
            do_show_races = true;
        } else if (strcmp(argv[i], "--races-all") == 0) {
            do_show_races_all = true;
        } else if (strcmp(argv[i], "--filter-status") == 0) {
            do_filter_status = true;
        } else if (strcmp(argv[i], "--sync") == 0) {
            do_sync = true;
        }
    }

    /* Create default apps config if it doesn't exist */
    create_default_apps_config();

    /* Create and load app launcher */
    app_launcher *launcher = launcher_create();
    if (launcher) {
        launcher_load_config(launcher, config_get_apps_path());
    }

    /* Handle --add-app command */
    if (do_add_app) {
        if (launcher) {
            add_app(launcher, add_app_name, add_app_path);
            launcher_destroy(launcher);
        } else {
            printf("Error: Could not create launcher\n");
        }
        return 0;
    }

    /* Handle --list-apps command */
    if (do_list_apps) {
        if (launcher) {
            list_apps(launcher);
            launcher_destroy(launcher);
        } else {
            printf("Error: Could not create launcher\n");
        }
        return 0;
    }

    /* Handle --launch-apps command */
    if (do_launch_apps) {
        if (launcher) {
            launch_manual_apps(launcher);
            launcher_destroy(launcher);
        } else {
            printf("Error: Could not create launcher\n");
        }
        return 0;
    }

    /* Handle race filter commands */
    if (do_show_races || do_show_races_all || do_filter_status || do_sync) {
        /* Create and load database */
        ira_database *db = database_create();
        if (!db) {
            printf("Error: Could not create database\n");
            launcher_destroy(launcher);
            return 1;
        }

        /* Load cached data */
        database_load_all(db);

        if (do_filter_status) {
            show_filter_status(db);
        } else if (do_sync) {
            sync_data(db);
        } else if (do_show_races || do_show_races_all) {
            show_races(db, do_show_races_all);
        }

        database_destroy(db);
        launcher_destroy(launcher);
        return 0;
    }

    /* Set up signal handler */
    signal(SIGINT, signal_handler);

    printf("Config: %s\n", config_get_default_path());
    printf("Data:   %s\n", config_get_data_path());
    printf("Apps:   %s\n\n", config_get_apps_path());

    /* Initialize state tracking for launcher */
    ira_state current_state = STATE_WAITING;

    printf("Waiting for iRacing...\n");

    /* Wait for connection */
    while (g_running && !irsdk_startup()) {
        Sleep(1000);
        printf(".");
        fflush(stdout);
    }

    if (!g_running) {
        launcher_destroy(launcher);
        return 0;
    }

    printf("\nConnected to iRacing!\n");

    /* State transition: WAITING -> CONNECTED */
    current_state = STATE_CONNECTED;
    if (launcher) {
        launcher_start_all(launcher, LAUNCH_ON_CONNECT);
    }

    printf("Waiting for session data (enter a session with a car)...\n");

    /* Wait for data to be available - need actual telemetry data */
    char *data = NULL;
    int buf_len = 0;
    TelemetryOffsets offsets = {0};

    while (g_running) {
        /* Wait for data */
        irsdk_wait_for_data(1000, NULL);

        if (!irsdk_is_connected()) {
            continue;
        }

        /* Check if buffer is available */
        buf_len = irsdk_get_buf_len();
        if (buf_len <= 0) {
            continue;
        }

        /* Allocate buffer if needed */
        if (!data) {
            data = (char *)malloc(buf_len);
            if (!data) {
                printf("Error: Could not allocate data buffer\n");
                irsdk_shutdown();
                return 1;
            }
        }

        /* Try to initialize telemetry offsets */
        if (init_telemetry_offsets(&offsets)) {
            break; /* Success! Variables are available */
        }

        /* Variables not available yet - keep waiting */
        printf(".");
        fflush(stdout);
    }

    if (!g_running) {
        free(data);
        irsdk_shutdown();
        launcher_destroy(launcher);
        return 0;
    }

    printf("\nSession data available!\n");

    /* State transition: CONNECTED -> IN_SESSION */
    current_state = STATE_IN_SESSION;
    if (launcher) {
        launcher_start_all(launcher, LAUNCH_ON_SESSION);
    }

    /* Parse and display session info */
    SessionInfo session_info = {0};
    int last_session_update = -1;
    int current_session_update = irsdk_get_session_info_update();
    int last_car_id = -1;
    int last_track_id = -1;

    if (current_session_update != last_session_update) {
        if (parse_session_info(&session_info)) {
            display_session_info(&session_info);
            last_car_id = session_info.car_id;
            last_track_id = session_info.track_id;

            /* Initial filter evaluation for session apps */
            if (launcher && session_info.car_id > 0) {
                int changes = launcher_update_for_session(launcher, session_info.car_id, session_info.track_id);
                if (changes > 0) {
                    printf("Launched/stopped %d app(s) based on car/track filters.\n\n", changes);
                }
            }
        }
        last_session_update = current_session_update;
    }

    /* Set up telemetry logger if enabled */
    telem_logger *logger = NULL;
    if (enable_logging) {
        /* Use track name as session name if available */
        const char *session_name = session_info.track_name[0] ? session_info.track_name : "telemetry";
        logger = telem_log_create(log_dir, session_name);

        if (logger) {
            telem_log_add_defaults(logger);
            if (telem_log_start(logger)) {
                printf("Logging telemetry to: %s\n\n", telem_log_get_filepath(logger));
            } else {
                printf("Warning: Could not start telemetry logging\n\n");
                telem_log_destroy(logger);
                logger = NULL;
            }
        }
    }

    printf("Receiving telemetry data (Ctrl+C to exit):\n\n");

    /* Main loop */
    while (g_running) {
        /* Wait for new data (16ms = ~60Hz) */
        if (irsdk_wait_for_data(16, data)) {
            display_telemetry(data, &offsets, cfg.use_metric_units);

            /* Log telemetry if enabled */
            if (logger) {
                telem_log_sample(logger, data);
            }

            /* Check for session info updates */
            current_session_update = irsdk_get_session_info_update();
            if (current_session_update != last_session_update) {
                printf("\n\nSession info updated!\n");
                if (parse_session_info(&session_info)) {
                    display_session_info(&session_info);

                    /* Check for car/track changes */
                    bool car_changed = (session_info.car_id != last_car_id && session_info.car_id > 0);
                    bool track_changed = (session_info.track_id != last_track_id && session_info.track_id > 0);

                    if ((car_changed || track_changed) && launcher) {
                        if (cfg.car_switch_behavior == CAR_SWITCH_AUTO) {
                            int changes = launcher_update_for_session(launcher, session_info.car_id, session_info.track_id);
                            if (changes > 0) {
                                printf("Switched %d app(s) for new car/track.\n", changes);
                            }
                        } else if (cfg.car_switch_behavior == CAR_SWITCH_PROMPT) {
                            printf("Car/track changed. Press Enter to update apps, or continue driving...\n");
                            /* Note: Non-blocking prompt would require more complex handling */
                            /* For now, auto-switch after displaying the message */
                            int changes = launcher_update_for_session(launcher, session_info.car_id, session_info.track_id);
                            if (changes > 0) {
                                printf("Switched %d app(s) for new car/track.\n", changes);
                            }
                        }
                        /* CAR_SWITCH_DISABLED: do nothing */
                    }

                    last_car_id = session_info.car_id;
                    last_track_id = session_info.track_id;
                }
                last_session_update = current_session_update;
            }
        }

        /* Check if still connected */
        if (!irsdk_is_connected()) {
            printf("\n\nDisconnected from iRacing. Waiting to reconnect...\n");

            /* State transition: IN_SESSION/CONNECTED -> WAITING */
            current_state = STATE_WAITING;
            if (launcher) {
                launcher_stop_all(launcher, CLOSE_ON_IRACING_EXIT);
            }

            /* Stop logging during disconnect */
            if (logger) {
                telem_log_stop(logger);
                printf("Logged %d samples to: %s\n",
                       telem_log_get_sample_count(logger),
                       telem_log_get_filepath(logger));
                telem_log_destroy(logger);
                logger = NULL;
            }

            /* Wait for reconnection */
            while (g_running && !irsdk_is_connected()) {
                if (irsdk_wait_for_data(1000, NULL)) {
                    printf("Reconnected!\n\n");

                    /* State transition: WAITING -> CONNECTED */
                    current_state = STATE_CONNECTED;
                    if (launcher) {
                        launcher_start_all(launcher, LAUNCH_ON_CONNECT);
                    }

                    /* Reinitialize offsets in case variables changed */
                    if (!init_telemetry_offsets(&offsets)) {
                        printf("Error: Could not reinitialize telemetry offsets\n");
                        break;
                    }

                    /* State transition: CONNECTED -> IN_SESSION */
                    current_state = STATE_IN_SESSION;

                    /* Re-parse session info */
                    if (parse_session_info(&session_info)) {
                        display_session_info(&session_info);
                        last_car_id = session_info.car_id;
                        last_track_id = session_info.track_id;

                        /* Start session apps with filter evaluation */
                        if (launcher && session_info.car_id > 0) {
                            int changes = launcher_update_for_session(launcher, session_info.car_id, session_info.track_id);
                            if (changes > 0) {
                                printf("Launched %d app(s) based on car/track filters.\n", changes);
                            }
                        }
                    }
                    last_session_update = irsdk_get_session_info_update();

                    /* Restart logging */
                    if (enable_logging) {
                        const char *session_name = session_info.track_name[0] ?
                                                   session_info.track_name : "telemetry";
                        logger = telem_log_create(log_dir, session_name);
                        if (logger) {
                            telem_log_add_defaults(logger);
                            if (telem_log_start(logger)) {
                                printf("Logging telemetry to: %s\n\n",
                                       telem_log_get_filepath(logger));
                            }
                        }
                    }
                }
            }
        }
    }

    /* Cleanup */
    printf("\n\nCleaning up...\n");

    if (logger) {
        telem_log_stop(logger);
        printf("Logged %d samples to: %s\n",
               telem_log_get_sample_count(logger),
               telem_log_get_filepath(logger));
        telem_log_destroy(logger);
    }

    /* Save configuration */
    cfg.telemetry_logging_enabled = enable_logging;
    config_save_default(&cfg);

    /* Stop all apps that should close on ira exit and cleanup launcher */
    if (launcher) {
        launcher_stop_all(launcher, CLOSE_ON_IRA_EXIT);
        launcher_destroy(launcher);
    }

    free(data);
    irsdk_shutdown();
    printf("Goodbye!\n");

    return 0;
}
