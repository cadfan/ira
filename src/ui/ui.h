/*
 * ira - iRacing Application
 * UI Backend Interface
 *
 * Defines the abstract UI interface as a function pointer vtable.
 * Backends (console, future GUI) implement this interface.
 *
 * Copyright (c) 2026 Christopher Griffiths
 */

#ifndef IRA_UI_H
#define IRA_UI_H

#include <stdbool.h>
#include "../data/database.h"
#include "../data/models.h"
#include "../launcher/launcher.h"
#include "../util/config.h"

/*
 * Session info (parsed from iRacing YAML)
 */
typedef struct {
    char track_name[128];
    char track_config[64];
    char car_name[64];
    char driver_name[64];
    int driver_car_idx;
    float track_length_km;
    int car_id;
    int track_id;
} ui_session_info;

/*
 * Decoded telemetry data (backend-agnostic, no SDK dependency)
 */
typedef struct {
    float speed_mps;        /* speed in meters per second */
    float rpm;
    int gear;               /* -1=R, 0=N, 1-9=forward */
    float throttle;         /* 0.0 - 1.0 */
    float brake;            /* 0.0 - 1.0 */
    float clutch;           /* 0.0 - 1.0 */
    int lap;
    float lap_dist_pct;     /* 0.0 - 1.0 */
    double session_time;
    float fuel_level;       /* liters */
    bool is_on_track;
    bool use_metric;
} ui_telemetry_data;

/*
 * UI Backend interface — function pointer vtable.
 *
 * Each backend (console, GUI) provides its own implementation.
 * main.c calls through this interface, never renders directly.
 */
typedef struct ui_backend {
    /* --- Display operations --- */

    /* Show the application banner/splash */
    void (*show_banner)(struct ui_backend *ui);

    /* Show command-line usage/help */
    void (*show_usage)(struct ui_backend *ui, const char *program_name);

    /* Show session info header (track, car, driver) */
    void (*show_session_info)(struct ui_backend *ui, const ui_session_info *info);

    /* Update real-time telemetry display (~60Hz) */
    void (*show_telemetry)(struct ui_backend *ui, const ui_telemetry_data *telem);

    /* Show race filter settings and data status */
    void (*show_filter_status)(struct ui_backend *ui, ira_database *db);

    /* Show filtered race schedule */
    void (*show_races)(struct ui_backend *ui, ira_database *db, bool show_all);

    /* Show configured applications list */
    void (*show_apps)(struct ui_backend *ui, app_launcher *launcher);

    /* Show current settings */
    void (*show_settings)(struct ui_backend *ui, const ira_config *cfg);

    /* --- Status messages --- */

    /* Show a status/info message */
    void (*show_status)(struct ui_backend *ui, const char *message);

    /* Show an error message */
    void (*show_error)(struct ui_backend *ui, const char *message);

    /* Show sync progress (operation name + success/failure) */
    void (*show_sync_progress)(struct ui_backend *ui, const char *operation, bool success,
                               const char *detail);

    /* Show a prompt string (no newline, flushed for immediate display) */
    void (*show_prompt)(struct ui_backend *ui, const char *prompt);

    /* --- Interactive operations --- */

    /* Run the interactive menu. Returns when user exits. */
    void (*run_menu)(struct ui_backend *ui, app_launcher *launcher,
                     ira_config *cfg, ira_database **db_ptr);

    /* Launch manual-trigger apps with status output */
    void (*launch_apps)(struct ui_backend *ui, app_launcher *launcher);

    /* Add an app with status output */
    bool (*add_app)(struct ui_backend *ui, app_launcher *launcher,
                    const char *name, const char *exe_path);

    /* --- Console input (may be no-ops for GUI backends) --- */

    /* Check if input is available (non-blocking) */
    bool (*input_available)(struct ui_backend *ui);

    /* Read a single keypress (blocking) */
    int (*read_key)(struct ui_backend *ui);

    /* Read a line of text input. Returns false if empty/cancelled. */
    bool (*read_line)(struct ui_backend *ui, char *buf, int size);

    /* Flush pending input */
    void (*flush_input)(struct ui_backend *ui);

    /* Show a "waiting" dot (for connection polling) */
    void (*show_waiting_dot)(struct ui_backend *ui);

    /* --- Lifecycle --- */

    /* Destroy the backend and free resources */
    void (*destroy)(struct ui_backend *ui);
} ui_backend;

/*
 * Backend factory functions
 */

/* Create a Win32 console backend */
ui_backend *ui_console_create(void);

#endif /* IRA_UI_H */
