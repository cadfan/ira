/*
 * ira - iRacing Application
 * Telemetry CSV Logger
 *
 * Copyright (c) 2026 Christopher Griffiths
 */

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <direct.h>

#include "telemetry_log.h"

/* Variable info for logging */
typedef struct {
    char name[IRSDK_MAX_STRING];
    int offset;
    int type;
    int count;
} telem_var_info;

/* Logger state */
struct telem_logger {
    char output_dir[MAX_PATH];
    char session_name[128];
    char filepath[MAX_PATH];

    FILE *file;
    bool active;

    telem_var_info vars[TELEM_LOG_MAX_VARS];
    int var_count;

    int sample_count;
    double start_time;
};

/*
 * Create a new telemetry logger
 */
telem_logger *telem_log_create(const char *output_dir, const char *session_name)
{
    telem_logger *logger = (telem_logger *)calloc(1, sizeof(telem_logger));
    if (!logger) {
        return NULL;
    }

    if (output_dir) {
        strncpy(logger->output_dir, output_dir, MAX_PATH - 1);
    } else {
        strcpy(logger->output_dir, ".");
    }

    if (session_name) {
        strncpy(logger->session_name, session_name, sizeof(logger->session_name) - 1);
    } else {
        strcpy(logger->session_name, "telemetry");
    }

    logger->file = NULL;
    logger->active = false;
    logger->var_count = 0;
    logger->sample_count = 0;

    return logger;
}

/*
 * Destroy a telemetry logger
 */
void telem_log_destroy(telem_logger *logger)
{
    if (!logger) return;

    telem_log_stop(logger);
    free(logger);
}

/*
 * Add a variable to be logged
 */
bool telem_log_add_var(telem_logger *logger, const char *var_name)
{
    if (!logger || !var_name || logger->active) {
        return false;
    }

    if (logger->var_count >= TELEM_LOG_MAX_VARS) {
        return false;
    }

    /* Find the variable in iRacing */
    int idx = irsdk_var_name_to_index(var_name);
    if (idx < 0) {
        return false;
    }

    const irsdk_VarHeader *header = irsdk_get_var_header(idx);
    if (!header) {
        return false;
    }

    telem_var_info *var = &logger->vars[logger->var_count];
    strncpy(var->name, var_name, IRSDK_MAX_STRING - 1);
    var->offset = header->offset;
    var->type = header->type;
    var->count = header->count;

    logger->var_count++;
    return true;
}

/*
 * Add default variables for logging
 */
bool telem_log_add_defaults(telem_logger *logger)
{
    if (!logger) return false;

    /* Core driving data */
    telem_log_add_var(logger, "SessionTime");
    telem_log_add_var(logger, "Lap");
    telem_log_add_var(logger, "LapDistPct");
    telem_log_add_var(logger, "Speed");
    telem_log_add_var(logger, "RPM");
    telem_log_add_var(logger, "Gear");
    telem_log_add_var(logger, "Throttle");
    telem_log_add_var(logger, "Brake");
    telem_log_add_var(logger, "Clutch");
    telem_log_add_var(logger, "SteeringWheelAngle");

    /* Position */
    telem_log_add_var(logger, "Lat");
    telem_log_add_var(logger, "Lon");
    telem_log_add_var(logger, "Alt");

    /* G-forces */
    telem_log_add_var(logger, "LatAccel");
    telem_log_add_var(logger, "LongAccel");
    telem_log_add_var(logger, "VertAccel");

    /* Car state */
    telem_log_add_var(logger, "FuelLevel");
    telem_log_add_var(logger, "FuelUsePerHour");
    telem_log_add_var(logger, "OilTemp");
    telem_log_add_var(logger, "WaterTemp");

    return logger->var_count > 0;
}

/*
 * Generate a unique filename with timestamp
 */
static void generate_filename(telem_logger *logger)
{
    time_t now = time(NULL);
    struct tm *t = localtime(&now);

    char timestamp[32];
    strftime(timestamp, sizeof(timestamp), "%Y%m%d_%H%M%S", t);

    snprintf(logger->filepath, MAX_PATH, "%s\\%s_%s.csv",
             logger->output_dir, logger->session_name, timestamp);
}

/*
 * Write CSV header
 */
static bool write_header(telem_logger *logger)
{
    if (!logger || !logger->file) return false;

    /* Write variable names */
    for (int i = 0; i < logger->var_count; i++) {
        telem_var_info *var = &logger->vars[i];

        if (var->count > 1 && var->type != IRSDK_TYPE_CHAR) {
            /* Array variable - write multiple columns */
            for (int j = 0; j < var->count; j++) {
                if (i > 0 || j > 0) fprintf(logger->file, ",");
                fprintf(logger->file, "%s_%d", var->name, j);
            }
        } else {
            if (i > 0) fprintf(logger->file, ",");
            fprintf(logger->file, "%s", var->name);
        }
    }
    fprintf(logger->file, "\n");

    return true;
}

/*
 * Start logging
 */
bool telem_log_start(telem_logger *logger)
{
    if (!logger || logger->active || logger->var_count == 0) {
        return false;
    }

    /* Ensure output directory exists */
    _mkdir(logger->output_dir);

    /* Generate filename */
    generate_filename(logger);

    /* Open file */
    logger->file = fopen(logger->filepath, "w");
    if (!logger->file) {
        return false;
    }

    /* Write header */
    if (!write_header(logger)) {
        fclose(logger->file);
        logger->file = NULL;
        return false;
    }

    logger->active = true;
    logger->sample_count = 0;
    logger->start_time = 0.0;

    return true;
}

/*
 * Stop logging
 */
void telem_log_stop(telem_logger *logger)
{
    if (!logger) return;

    if (logger->file) {
        fflush(logger->file);
        fclose(logger->file);
        logger->file = NULL;
    }

    logger->active = false;
}

/*
 * Check if logger is active
 */
bool telem_log_is_active(telem_logger *logger)
{
    return logger && logger->active;
}

/*
 * Write a value to the CSV
 */
static void write_value(FILE *file, const char *data, telem_var_info *var, int entry)
{
    const char *ptr = data + var->offset;

    switch (var->type) {
    case IRSDK_TYPE_CHAR:
        /* String - write as quoted string */
        fprintf(file, "\"%s\"", ptr);
        break;

    case IRSDK_TYPE_BOOL:
        fprintf(file, "%d", ((const bool *)ptr)[entry] ? 1 : 0);
        break;

    case IRSDK_TYPE_INT:
    case IRSDK_TYPE_BITFIELD:
        fprintf(file, "%d", ((const int *)ptr)[entry]);
        break;

    case IRSDK_TYPE_FLOAT:
        fprintf(file, "%.6f", ((const float *)ptr)[entry]);
        break;

    case IRSDK_TYPE_DOUBLE:
        fprintf(file, "%.9f", ((const double *)ptr)[entry]);
        break;

    default:
        fprintf(file, "0");
        break;
    }
}

/*
 * Log a single data sample
 */
bool telem_log_sample(telem_logger *logger, const char *data)
{
    if (!logger || !logger->active || !logger->file || !data) {
        return false;
    }

    /* Write values */
    bool first = true;
    for (int i = 0; i < logger->var_count; i++) {
        telem_var_info *var = &logger->vars[i];

        if (var->count > 1 && var->type != IRSDK_TYPE_CHAR) {
            /* Array variable */
            for (int j = 0; j < var->count; j++) {
                if (!first) fprintf(logger->file, ",");
                first = false;
                write_value(logger->file, data, var, j);
            }
        } else {
            if (!first) fprintf(logger->file, ",");
            first = false;
            write_value(logger->file, data, var, 0);
        }
    }
    fprintf(logger->file, "\n");

    logger->sample_count++;

    /* Flush periodically */
    if (logger->sample_count % 100 == 0) {
        fflush(logger->file);
    }

    return true;
}

/*
 * Get the current log file path
 */
const char *telem_log_get_filepath(const telem_logger *logger)
{
    return logger ? logger->filepath : NULL;
}

/*
 * Get the number of samples logged
 */
int telem_log_get_sample_count(const telem_logger *logger)
{
    return logger ? logger->sample_count : 0;
}
