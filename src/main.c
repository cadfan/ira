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
#include "api/iracing_api.h"
#include "ui/ui.h"

/* Global flag for clean shutdown (extern'd by console backend) */
volatile bool g_running = true;

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

/* Parse session info from YAML */
static bool parse_session_info(ui_session_info *info)
{
    const char *yaml = irsdk_get_session_info();
    if (!yaml) {
        return false;
    }

    memset(info, 0, sizeof(ui_session_info));

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

/* SDK telemetry variable offsets (cached for performance, internal to main.c) */
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
} telemetry_offsets;

/* Initialize telemetry offsets */
static bool init_telemetry_offsets(telemetry_offsets *offsets)
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

/* Decode raw SDK data buffer into backend-agnostic telemetry struct */
static void decode_telemetry(const char *data, const telemetry_offsets *offsets,
                              bool use_metric, ui_telemetry_data *out)
{
    out->speed_mps = irsdk_get_var_float(data, offsets->speed, 0);
    out->rpm = irsdk_get_var_float(data, offsets->rpm, 0);
    out->gear = irsdk_get_var_int(data, offsets->gear, 0);
    out->throttle = irsdk_get_var_float(data, offsets->throttle, 0);
    out->brake = irsdk_get_var_float(data, offsets->brake, 0);
    out->clutch = irsdk_get_var_float(data, offsets->clutch, 0);
    out->lap = irsdk_get_var_int(data, offsets->lap, 0);
    out->lap_dist_pct = irsdk_get_var_float(data, offsets->lap_dist_pct, 0);
    out->session_time = irsdk_get_var_double(data, offsets->session_time, 0);
    out->fuel_level = (offsets->fuel_level >= 0)
                    ? irsdk_get_var_float(data, offsets->fuel_level, 0)
                    : -1.0f;  /* -1 = no data (offset unavailable) */
    out->is_on_track = irsdk_get_var_int(data, offsets->is_on_track, 0) != 0;
    out->use_metric = use_metric;
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

/* Sync data from iRacing API */
static void sync_data(ui_backend *ui, ira_database *db)
{
    if (!db) {
        ui->show_error(ui, "Database not initialized");
        return;
    }

    ui->show_status(ui, "Syncing data from iRacing API...");

    iracing_api *api = api_create();
    if (!api) {
        ui->show_error(ui, "Could not create API client");
        return;
    }

    /* Load OAuth credentials from config file */
    if (!api_load_oauth_config(api, "oauth_config.json")) {
        ui->show_error(ui, "Could not load OAuth credentials.");
        ui->show_status(ui, "Create an oauth_config.json file with:");
        ui->show_status(ui, "  {");
        ui->show_status(ui, "    \"client_id\": \"YOUR_CLIENT_ID\",");
        ui->show_status(ui, "    \"client_secret\": \"YOUR_CLIENT_SECRET\"");
        ui->show_status(ui, "  }");
        api_destroy(api);
        return;
    }

    /* Authenticate */
    api_error err = api_authenticate(api);
    if (err != API_OK) {
        char msg[256];
        snprintf(msg, sizeof(msg), "Authentication failed: %s", api_get_last_error(api));
        ui->show_error(ui, msg);
        api_destroy(api);
        return;
    }

    /* Fetch data */
    err = api_fetch_cars(api, db);
    ui->show_sync_progress(ui, "Fetching cars", err == API_OK,
                            err != API_OK ? api_error_string(err) : NULL);

    err = api_fetch_tracks(api, db);
    ui->show_sync_progress(ui, "Fetching tracks", err == API_OK,
                            err != API_OK ? api_error_string(err) : NULL);

    err = api_fetch_series(api, db);
    ui->show_sync_progress(ui, "Fetching series", err == API_OK,
                            err != API_OK ? api_error_string(err) : NULL);

    err = api_fetch_car_classes(api, db);
    ui->show_sync_progress(ui, "Fetching car classes", err == API_OK,
                            err != API_OK ? api_error_string(err) : NULL);

    time_t now = time(NULL);
    struct tm *tm = localtime(&now);
    int year = tm->tm_year + 1900;
    int quarter = (tm->tm_mon / 3) + 1;
    err = api_fetch_seasons(api, db, year, quarter);
    ui->show_sync_progress(ui, "Fetching seasons", err == API_OK,
                            err != API_OK ? api_error_string(err) : NULL);

    /* Resolve car_ids in schedule weeks from car classes */
    if (db->season_count > 0 && db->car_class_count > 0) {
        for (int i = 0; i < db->season_count; i++) {
            ira_season *season = &db->seasons[i];
            /* Collect car_ids from all car classes in this season */
            int ids[16];
            int id_count = 0;
            for (int c = 0; c < season->car_class_count && id_count < 16; c++) {
                ira_car_class *cc = database_get_car_class(db, season->car_class_ids[c]);
                if (!cc) continue;
                for (int k = 0; k < cc->car_count && id_count < 16; k++) {
                    ids[id_count++] = cc->car_ids[k];
                }
            }
            /* Apply to all schedule weeks */
            for (int j = 0; j < season->schedule_count; j++) {
                memcpy(season->schedule[j].car_ids, ids, id_count * sizeof(int));
                season->schedule[j].car_count = id_count;
            }
        }
    }

    err = api_fetch_owned_content(api, db);
    ui->show_sync_progress(ui, "Fetching owned content", err == API_OK,
                            err != API_OK ? api_error_string(err) : NULL);

    /* Save data */
    ui->show_status(ui, "\nSaving data...");
    database_save_all(db);

    api_destroy(api);
    ui->show_status(ui, "Sync complete.");
}

int main(int argc, char *argv[])
{
    /* Create UI backend */
    ui_backend *ui = ui_console_create();
    if (!ui) {
        fprintf(stderr, "Error: Could not create UI backend\n");
        return 1;
    }

    ui->show_banner(ui);

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
    bool do_menu = false;
    const char *add_app_name = NULL;
    const char *add_app_path = NULL;
    char log_dir[260];
    strncpy(log_dir, cfg.telemetry_log_path, sizeof(log_dir) - 1);

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            ui->show_usage(ui, argv[0]);
            ui->destroy(ui);
            return 0;
        } else if (strcmp(argv[i], "-l") == 0 || strcmp(argv[i], "--log") == 0) {
            enable_logging = true;
        } else if (strcmp(argv[i], "-m") == 0 || strcmp(argv[i], "--metric") == 0) {
            cfg.use_metric_units = true;
        } else if (strcmp(argv[i], "-i") == 0 || strcmp(argv[i], "--imperial") == 0) {
            cfg.use_metric_units = false;
        } else if (strcmp(argv[i], "--log-dir") == 0 && i + 1 < argc) {
            strncpy(log_dir, argv[++i], sizeof(log_dir) - 1);
        } else if (strcmp(argv[i], "--menu") == 0) {
            do_menu = true;
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

    /* Handle --menu command */
    if (do_menu) {
        if (launcher) {
            ira_database *menu_db = NULL;
            g_running = true;  /* Ensure menu loop can run */
            ui->run_menu(ui, launcher, &cfg, &menu_db);
            if (menu_db) {
                database_destroy(menu_db);
            }
            launcher_destroy(launcher);
        } else {
            ui->show_error(ui, "Could not create launcher");
        }
        ui->destroy(ui);
        return 0;
    }

    /* Handle --add-app command */
    if (do_add_app) {
        if (launcher) {
            ui->add_app(ui, launcher, add_app_name, add_app_path);
            launcher_destroy(launcher);
        } else {
            ui->show_error(ui, "Could not create launcher");
        }
        ui->destroy(ui);
        return 0;
    }

    /* Handle --list-apps command */
    if (do_list_apps) {
        if (launcher) {
            ui->show_apps(ui, launcher);
            launcher_destroy(launcher);
        } else {
            ui->show_error(ui, "Could not create launcher");
        }
        ui->destroy(ui);
        return 0;
    }

    /* Handle --launch-apps command */
    if (do_launch_apps) {
        if (launcher) {
            ui->launch_apps(ui, launcher);
            launcher_destroy(launcher);
        } else {
            ui->show_error(ui, "Could not create launcher");
        }
        ui->destroy(ui);
        return 0;
    }

    /* Handle race filter commands */
    if (do_show_races || do_show_races_all || do_filter_status || do_sync) {
        /* Create and load database */
        ira_database *db = database_create();
        if (!db) {
            ui->show_error(ui, "Could not create database");
            launcher_destroy(launcher);
            ui->destroy(ui);
            return 1;
        }

        /* Load cached data */
        database_load_all(db);

        if (do_filter_status) {
            ui->show_filter_status(ui, db);
        } else if (do_sync) {
            sync_data(ui, db);
        } else if (do_show_races || do_show_races_all) {
            ui->show_races(ui, db, do_show_races_all);
        }

        database_destroy(db);
        launcher_destroy(launcher);
        ui->destroy(ui);
        return 0;
    }

    /* Set up signal handler */
    signal(SIGINT, signal_handler);

    {
        char msg[512];
        snprintf(msg, sizeof(msg), "Config: %s", config_get_default_path());
        ui->show_status(ui, msg);
        snprintf(msg, sizeof(msg), "Data:   %s", config_get_data_path());
        ui->show_status(ui, msg);
        snprintf(msg, sizeof(msg), "Apps:   %s", config_get_apps_path());
        ui->show_status(ui, msg);
    }

    /* Initialize state tracking for launcher */
    ira_state current_state = STATE_WAITING;

    /* Database pointer for lazy loading in menu */
    ira_database *menu_db = NULL;

    ui->show_status(ui, "\nWaiting for iRacing... (press any key for menu)");

    /* Clear any pending input */
    ui->flush_input(ui);

    /* Wait for connection, with interactive menu support */
    while (g_running) {
        /* Try to connect and check if actually connected */
        if (irsdk_startup() && irsdk_is_connected()) {
            break;  /* iRacing is running and connected */
        }

        /* Check for keyboard input to open menu */
        if (ui->input_available(ui)) {
            ui->read_key(ui);  /* consume the input */
            ui->run_menu(ui, launcher, &cfg, &menu_db);
            ui->flush_input(ui);  /* Clear input after menu */
            ui->show_status(ui, "\nWaiting for iRacing... (press any key for menu)");
        }

        Sleep(200);
        ui->show_waiting_dot(ui);
    }

    /* Clean up lazy-loaded database if used */
    if (menu_db) {
        database_destroy(menu_db);
        menu_db = NULL;
    }

    if (!g_running) {
        launcher_destroy(launcher);
        ui->destroy(ui);
        return 0;
    }

    ui->show_status(ui, "\nConnected to iRacing!");

    /* State transition: WAITING -> CONNECTED */
    current_state = STATE_CONNECTED;
    if (launcher) {
        launcher_start_all(launcher, LAUNCH_ON_CONNECT);
    }

    ui->show_status(ui, "Waiting for session data (enter a session with a car)...");

    /* Wait for data to be available - need actual telemetry data */
    char *data = NULL;
    int buf_len = 0;
    telemetry_offsets offsets = {0};

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
                ui->show_error(ui, "Could not allocate data buffer");
                irsdk_shutdown();
                ui->destroy(ui);
                return 1;
            }
        }

        /* Try to initialize telemetry offsets */
        if (init_telemetry_offsets(&offsets)) {
            break; /* Success! Variables are available */
        }

        /* Variables not available yet - keep waiting */
        ui->show_waiting_dot(ui);
    }

    if (!g_running) {
        free(data);
        irsdk_shutdown();
        launcher_destroy(launcher);
        ui->destroy(ui);
        return 0;
    }

    ui->show_status(ui, "\nSession data available!");

    /* State transition: CONNECTED -> IN_SESSION */
    current_state = STATE_IN_SESSION;
    if (launcher) {
        launcher_start_all(launcher, LAUNCH_ON_SESSION);
    }

    /* Parse and display session info */
    ui_session_info session_info = {0};
    int last_session_update = -1;
    int current_session_update = irsdk_get_session_info_update();
    int last_car_id = -1;
    int last_track_id = -1;

    if (current_session_update != last_session_update) {
        if (parse_session_info(&session_info)) {
            ui->show_session_info(ui, &session_info);
            last_car_id = session_info.car_id;
            last_track_id = session_info.track_id;

            /* Initial filter evaluation for session apps */
            if (launcher && session_info.car_id > 0) {
                int changes = launcher_update_for_session(launcher, session_info.car_id, session_info.track_id);
                if (changes > 0) {
                    char msg[128];
                    snprintf(msg, sizeof(msg), "Launched/stopped %d app(s) based on car/track filters.\n", changes);
                    ui->show_status(ui, msg);
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
                char msg[512];
                snprintf(msg, sizeof(msg), "Logging telemetry to: %s\n", telem_log_get_filepath(logger));
                ui->show_status(ui, msg);
            } else {
                ui->show_status(ui, "Warning: Could not start telemetry logging\n");
                telem_log_destroy(logger);
                logger = NULL;
            }
        }
    }

    ui->show_status(ui, "Receiving telemetry data (Ctrl+C to exit):\n");

    /* Main loop */
    while (g_running) {
        /* Wait for new data (16ms = ~60Hz) */
        if (irsdk_wait_for_data(16, data)) {
            ui_telemetry_data telem;
            decode_telemetry(data, &offsets, cfg.use_metric_units, &telem);
            ui->show_telemetry(ui, &telem);

            /* Log telemetry if enabled */
            if (logger) {
                telem_log_sample(logger, data);
            }

            /* Check for session info updates */
            current_session_update = irsdk_get_session_info_update();
            if (current_session_update != last_session_update) {
                ui->show_status(ui, "\n\nSession info updated!");
                if (parse_session_info(&session_info)) {
                    ui->show_session_info(ui, &session_info);

                    /* Check for car/track changes */
                    bool car_changed = (session_info.car_id != last_car_id && session_info.car_id > 0);
                    bool track_changed = (session_info.track_id != last_track_id && session_info.track_id > 0);

                    if ((car_changed || track_changed) && launcher) {
                        if (cfg.car_switch_behavior == CAR_SWITCH_AUTO) {
                            int changes = launcher_update_for_session(launcher, session_info.car_id, session_info.track_id);
                            if (changes > 0) {
                                char msg[128];
                                snprintf(msg, sizeof(msg), "Switched %d app(s) for new car/track.", changes);
                                ui->show_status(ui, msg);
                            }
                        } else if (cfg.car_switch_behavior == CAR_SWITCH_PROMPT) {
                            ui->show_status(ui, "Car/track changed. Press Enter to update apps, or continue driving...");
                            int changes = launcher_update_for_session(launcher, session_info.car_id, session_info.track_id);
                            if (changes > 0) {
                                char msg[128];
                                snprintf(msg, sizeof(msg), "Switched %d app(s) for new car/track.", changes);
                                ui->show_status(ui, msg);
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
            ui->show_status(ui, "\n\nDisconnected from iRacing. Waiting to reconnect...");

            /* State transition: IN_SESSION/CONNECTED -> WAITING */
            current_state = STATE_WAITING;
            if (launcher) {
                launcher_stop_all(launcher, CLOSE_ON_IRACING_EXIT);
            }

            /* Stop logging during disconnect */
            if (logger) {
                telem_log_stop(logger);
                {
                    char msg[512];
                    snprintf(msg, sizeof(msg), "Logged %d samples to: %s",
                             telem_log_get_sample_count(logger),
                             telem_log_get_filepath(logger));
                    ui->show_status(ui, msg);
                }
                telem_log_destroy(logger);
                logger = NULL;
            }

            /* Wait for reconnection */
            while (g_running && !irsdk_is_connected()) {
                if (irsdk_wait_for_data(1000, NULL)) {
                    ui->show_status(ui, "Reconnected!\n");

                    /* State transition: WAITING -> CONNECTED */
                    current_state = STATE_CONNECTED;
                    if (launcher) {
                        launcher_start_all(launcher, LAUNCH_ON_CONNECT);
                    }

                    /* Reinitialize offsets in case variables changed */
                    if (!init_telemetry_offsets(&offsets)) {
                        ui->show_error(ui, "Could not reinitialize telemetry offsets");
                        break;
                    }

                    /* State transition: CONNECTED -> IN_SESSION */
                    current_state = STATE_IN_SESSION;

                    /* Re-parse session info */
                    if (parse_session_info(&session_info)) {
                        ui->show_session_info(ui, &session_info);
                        last_car_id = session_info.car_id;
                        last_track_id = session_info.track_id;

                        /* Start session apps with filter evaluation */
                        if (launcher && session_info.car_id > 0) {
                            int changes = launcher_update_for_session(launcher, session_info.car_id, session_info.track_id);
                            if (changes > 0) {
                                char msg[128];
                                snprintf(msg, sizeof(msg), "Launched %d app(s) based on car/track filters.", changes);
                                ui->show_status(ui, msg);
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
                                char msg[512];
                                snprintf(msg, sizeof(msg), "Logging telemetry to: %s\n",
                                         telem_log_get_filepath(logger));
                                ui->show_status(ui, msg);
                            }
                        }
                    }
                }
            }
        }
    }

    /* Cleanup */
    ui->show_status(ui, "\n\nCleaning up...");

    if (logger) {
        telem_log_stop(logger);
        {
            char msg[512];
            snprintf(msg, sizeof(msg), "Logged %d samples to: %s",
                     telem_log_get_sample_count(logger),
                     telem_log_get_filepath(logger));
            ui->show_status(ui, msg);
        }
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
    ui->show_status(ui, "Goodbye!");
    ui->destroy(ui);

    return 0;
}
