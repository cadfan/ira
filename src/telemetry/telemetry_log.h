/*
 * ira - iRacing Application
 * Telemetry CSV Logger
 *
 * Copyright (c) 2026 Christopher Griffiths
 */

#ifndef IRA_TELEMETRY_LOG_H
#define IRA_TELEMETRY_LOG_H

#include <stdbool.h>
#include "../irsdk/irsdk.h"

/* Maximum number of variables to log */
#define TELEM_LOG_MAX_VARS 64

/* Telemetry logger state */
typedef struct telem_logger telem_logger;

/*
 * Create a new telemetry logger.
 *
 * Parameters:
 *   output_dir - Directory to write log files
 *   session_name - Name for this logging session (used in filename)
 *
 * Returns: Logger handle, or NULL on error.
 */
telem_logger *telem_log_create(const char *output_dir, const char *session_name);

/*
 * Destroy a telemetry logger and close all files.
 */
void telem_log_destroy(telem_logger *logger);

/*
 * Add a variable to be logged.
 * Must be called before telem_log_start().
 *
 * Parameters:
 *   var_name - Name of the iRacing variable (e.g., "Speed", "RPM")
 *
 * Returns: true if added successfully.
 */
bool telem_log_add_var(telem_logger *logger, const char *var_name);

/*
 * Add default variables for logging.
 * Adds common variables: Speed, RPM, Gear, Throttle, Brake, etc.
 */
bool telem_log_add_defaults(telem_logger *logger);

/*
 * Start logging. Opens the output file and writes headers.
 *
 * Returns: true if started successfully.
 */
bool telem_log_start(telem_logger *logger);

/*
 * Stop logging and close the output file.
 */
void telem_log_stop(telem_logger *logger);

/*
 * Check if logger is currently active.
 */
bool telem_log_is_active(telem_logger *logger);

/*
 * Log a single data sample.
 * Call this with each telemetry update.
 *
 * Parameters:
 *   data - Telemetry data buffer from irsdk_wait_for_data()
 *
 * Returns: true if logged successfully.
 */
bool telem_log_sample(telem_logger *logger, const char *data);

/*
 * Get the current log file path.
 */
const char *telem_log_get_filepath(const telem_logger *logger);

/*
 * Get the number of samples logged.
 */
int telem_log_get_sample_count(const telem_logger *logger);

#endif /* IRA_TELEMETRY_LOG_H */
