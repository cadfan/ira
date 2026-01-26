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
    printf("  -h, --help        Show this help message\n");
    printf("  -l, --log         Enable telemetry logging to CSV\n");
    printf("  -m, --metric      Use metric units (default)\n");
    printf("  -i, --imperial    Use imperial units\n");
    printf("  --log-dir <path>  Set telemetry log directory\n");
    printf("\n");
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
        }
    }

    /* Set up signal handler */
    signal(SIGINT, signal_handler);

    printf("Config: %s\n", config_get_default_path());
    printf("Data:   %s\n\n", config_get_data_path());

    printf("Waiting for iRacing...\n");

    /* Wait for connection */
    while (g_running && !irsdk_startup()) {
        Sleep(1000);
        printf(".");
        fflush(stdout);
    }

    if (!g_running) {
        return 0;
    }

    printf("\nConnected to iRacing!\n");
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
        return 0;
    }

    printf("\nSession data available!\n");

    /* Parse and display session info */
    SessionInfo session_info = {0};
    int last_session_update = -1;
    int current_session_update = irsdk_get_session_info_update();

    if (current_session_update != last_session_update) {
        if (parse_session_info(&session_info)) {
            display_session_info(&session_info);
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
                }
                last_session_update = current_session_update;
            }
        }

        /* Check if still connected */
        if (!irsdk_is_connected()) {
            printf("\n\nDisconnected from iRacing. Waiting to reconnect...\n");

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
                    /* Reinitialize offsets in case variables changed */
                    if (!init_telemetry_offsets(&offsets)) {
                        printf("Error: Could not reinitialize telemetry offsets\n");
                        break;
                    }

                    /* Re-parse session info */
                    if (parse_session_info(&session_info)) {
                        display_session_info(&session_info);
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

    free(data);
    irsdk_shutdown();
    printf("Goodbye!\n");

    return 0;
}
