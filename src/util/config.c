/*
 * ira - iRacing Application
 * Configuration Management
 *
 * Copyright (c) 2026 Christopher Griffiths
 */

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shlobj.h>
#include <stdio.h>
#include <string.h>
#include <direct.h>

#include "config.h"
#include "json.h"

#pragma comment(lib, "shell32")

/* Static buffers for paths */
static char g_config_path[MAX_PATH] = {0};
static char g_data_path[MAX_PATH] = {0};

/*
 * Initialize paths
 */
static void init_paths(void)
{
    if (g_data_path[0] != '\0') {
        return; /* Already initialized */
    }

    /* Get AppData folder */
    char appdata[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPathA(NULL, CSIDL_APPDATA, NULL, 0, appdata))) {
        snprintf(g_data_path, MAX_PATH, "%s\\ira", appdata);
        snprintf(g_config_path, MAX_PATH, "%s\\ira\\config.json", appdata);
    } else {
        /* Fallback to current directory */
        strcpy(g_data_path, ".");
        strcpy(g_config_path, ".\\config.json");
    }
}

/*
 * Initialize configuration with defaults
 */
void config_init_defaults(ira_config *cfg)
{
    if (!cfg) return;

    init_paths();

    memset(cfg, 0, sizeof(ira_config));

    cfg->telemetry_logging_enabled = false;
    cfg->telemetry_log_interval_ms = 100; /* 10 Hz */
    strncpy(cfg->telemetry_log_path, g_data_path, sizeof(cfg->telemetry_log_path) - 1);

    cfg->use_metric_units = true;
    cfg->refresh_rate_hz = 60;

    strncpy(cfg->data_path, g_data_path, sizeof(cfg->data_path) - 1);
}

/*
 * Get default config file path
 */
const char *config_get_default_path(void)
{
    init_paths();
    return g_config_path;
}

/*
 * Get default data directory path
 */
const char *config_get_data_path(void)
{
    init_paths();
    return g_data_path;
}

/*
 * Ensure data directory exists
 */
bool config_ensure_data_dir(void)
{
    init_paths();

    /* Check if directory exists */
    DWORD attrs = GetFileAttributesA(g_data_path);
    if (attrs != INVALID_FILE_ATTRIBUTES && (attrs & FILE_ATTRIBUTE_DIRECTORY)) {
        return true;
    }

    /* Create directory */
    return _mkdir(g_data_path) == 0 || errno == EEXIST;
}

/*
 * Load configuration from file
 */
bool config_load(ira_config *cfg, const char *filename)
{
    if (!cfg || !filename) {
        return false;
    }

    /* Initialize with defaults first */
    config_init_defaults(cfg);

    /* Parse JSON file */
    json_value *root = json_parse_file(filename);
    if (!root) {
        return false;
    }

    if (json_get_type(root) != JSON_OBJECT) {
        json_free(root);
        return false;
    }

    /* Read telemetry settings */
    json_value *telemetry = json_object_get(root, "telemetry");
    if (telemetry && json_get_type(telemetry) == JSON_OBJECT) {
        json_value *val;

        val = json_object_get(telemetry, "logging_enabled");
        if (val && json_get_type(val) == JSON_BOOL) {
            cfg->telemetry_logging_enabled = json_get_bool(val);
        }

        val = json_object_get(telemetry, "log_interval_ms");
        if (val && json_get_type(val) == JSON_NUMBER) {
            cfg->telemetry_log_interval_ms = json_get_int(val);
        }

        val = json_object_get(telemetry, "log_path");
        if (val && json_get_type(val) == JSON_STRING) {
            strncpy(cfg->telemetry_log_path, json_get_string(val),
                    sizeof(cfg->telemetry_log_path) - 1);
        }
    }

    /* Read display settings */
    json_value *display = json_object_get(root, "display");
    if (display && json_get_type(display) == JSON_OBJECT) {
        json_value *val;

        val = json_object_get(display, "use_metric_units");
        if (val && json_get_type(val) == JSON_BOOL) {
            cfg->use_metric_units = json_get_bool(val);
        }

        val = json_object_get(display, "refresh_rate_hz");
        if (val && json_get_type(val) == JSON_NUMBER) {
            cfg->refresh_rate_hz = json_get_int(val);
        }
    }

    /* Read general settings */
    json_value *general = json_object_get(root, "general");
    if (general && json_get_type(general) == JSON_OBJECT) {
        json_value *val;

        val = json_object_get(general, "data_path");
        if (val && json_get_type(val) == JSON_STRING) {
            strncpy(cfg->data_path, json_get_string(val),
                    sizeof(cfg->data_path) - 1);
        }
    }

    json_free(root);
    return true;
}

/*
 * Load configuration from default location
 */
bool config_load_default(ira_config *cfg)
{
    return config_load(cfg, config_get_default_path());
}

/*
 * Save configuration to file
 */
bool config_save(const ira_config *cfg, const char *filename)
{
    if (!cfg || !filename) {
        return false;
    }

    /* Ensure directory exists */
    config_ensure_data_dir();

    /* Build JSON structure */
    json_value *root = json_new_object();
    if (!root) return false;

    /* Telemetry settings */
    json_value *telemetry = json_new_object();
    if (telemetry) {
        json_object_set(telemetry, "logging_enabled",
                       json_new_bool(cfg->telemetry_logging_enabled));
        json_object_set(telemetry, "log_interval_ms",
                       json_new_number(cfg->telemetry_log_interval_ms));
        json_object_set(telemetry, "log_path",
                       json_new_string(cfg->telemetry_log_path));
        json_object_set(root, "telemetry", telemetry);
    }

    /* Display settings */
    json_value *display = json_new_object();
    if (display) {
        json_object_set(display, "use_metric_units",
                       json_new_bool(cfg->use_metric_units));
        json_object_set(display, "refresh_rate_hz",
                       json_new_number(cfg->refresh_rate_hz));
        json_object_set(root, "display", display);
    }

    /* General settings */
    json_value *general = json_new_object();
    if (general) {
        json_object_set(general, "data_path",
                       json_new_string(cfg->data_path));
        json_object_set(root, "general", general);
    }

    /* Write to file */
    bool result = json_write_file(root, filename, true);
    json_free(root);

    return result;
}

/*
 * Save configuration to default location
 */
bool config_save_default(const ira_config *cfg)
{
    return config_save(cfg, config_get_default_path());
}
