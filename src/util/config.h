/*
 * ira - iRacing Application
 * Configuration Management
 *
 * Copyright (c) 2026 Christopher Griffiths
 */

#ifndef IRA_CONFIG_H
#define IRA_CONFIG_H

#include <stdbool.h>

/* Configuration structure */
typedef struct {
    /* Telemetry settings */
    bool telemetry_logging_enabled;
    int telemetry_log_interval_ms;
    char telemetry_log_path[260];

    /* Display settings */
    bool use_metric_units;
    int refresh_rate_hz;

    /* General settings */
    char data_path[260];
} ira_config;

/*
 * Initialize configuration with defaults
 */
void config_init_defaults(ira_config *cfg);

/*
 * Load configuration from file.
 * Returns true on success, false on error (uses defaults on error).
 */
bool config_load(ira_config *cfg, const char *filename);

/*
 * Load configuration from default location.
 * Default: %APPDATA%/ira/config.json
 */
bool config_load_default(ira_config *cfg);

/*
 * Save configuration to file.
 */
bool config_save(const ira_config *cfg, const char *filename);

/*
 * Save configuration to default location.
 */
bool config_save_default(const ira_config *cfg);

/*
 * Get the default config file path.
 * Returns pointer to static buffer.
 */
const char *config_get_default_path(void);

/*
 * Get the default data directory path.
 * Returns pointer to static buffer.
 */
const char *config_get_data_path(void);

/*
 * Ensure the data directory exists.
 */
bool config_ensure_data_dir(void);

/*
 * Get the default apps configuration file path.
 * Returns pointer to static buffer.
 */
const char *config_get_apps_path(void);

#endif /* IRA_CONFIG_H */
