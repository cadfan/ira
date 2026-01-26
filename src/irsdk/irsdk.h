/*
 * ira - iRacing Application
 * iRacing SDK Interface (Pure C)
 *
 * Copyright (c) 2026 Christopher Griffiths
 */

#ifndef IRA_IRSDK_H
#define IRA_IRSDK_H

#include <stdbool.h>
#include "irsdk_defines.h"

/*
 * Initialization and shutdown
 */

/* Initialize connection to iRacing. Returns true on success. */
bool irsdk_startup(void);

/* Shutdown and cleanup resources */
void irsdk_shutdown(void);

/*
 * Connection status
 */

/* Check if connected to iRacing */
bool irsdk_is_connected(void);

/*
 * Data access
 */

/*
 * Wait for new telemetry data with timeout.
 * If data is non-NULL, copies the data buffer to it.
 * Returns true if new data is available.
 */
bool irsdk_wait_for_data(int timeout_ms, char *data);

/*
 * Check for new data without waiting.
 * If data is non-NULL, copies the data buffer to it.
 * Returns true if new data is available.
 */
bool irsdk_get_new_data(char *data);

/* Get pointer to the header structure. Returns NULL if not connected. */
const irsdk_Header *irsdk_get_header(void);

/* Get the session info YAML string. Returns NULL if not connected. */
const char *irsdk_get_session_info(void);

/* Get the session info update counter (increments when session info changes) */
int irsdk_get_session_info_update(void);

/*
 * Variable access
 */

/* Get pointer to the variable headers array */
const irsdk_VarHeader *irsdk_get_var_headers(void);

/* Get a specific variable header by index */
const irsdk_VarHeader *irsdk_get_var_header(int index);

/* Find variable index by name. Returns -1 if not found. */
int irsdk_var_name_to_index(const char *name);

/* Find variable offset by name. Returns -1 if not found. */
int irsdk_var_name_to_offset(const char *name);

/*
 * Helper functions for reading variable values from a data buffer
 */

/* Get boolean value from data buffer */
bool irsdk_get_var_bool(const char *data, int var_offset, int entry);

/* Get integer value from data buffer */
int irsdk_get_var_int(const char *data, int var_offset, int entry);

/* Get float value from data buffer */
float irsdk_get_var_float(const char *data, int var_offset, int entry);

/* Get double value from data buffer */
double irsdk_get_var_double(const char *data, int var_offset, int entry);

/*
 * Broadcast messages (remote control)
 */

/* Send a broadcast message to iRacing (3 short parameters) */
void irsdk_broadcast_msg(irsdk_BroadcastMsg msg, int var1, int var2, int var3);

/* Send a broadcast message to iRacing (1 short + 1 int parameter) */
void irsdk_broadcast_msg_int(irsdk_BroadcastMsg msg, int var1, int var2);

/* Send a broadcast message to iRacing (1 short + 1 float parameter) */
void irsdk_broadcast_msg_float(irsdk_BroadcastMsg msg, int var1, float var2);

/* Encode a car number with leading zeros */
int irsdk_pad_car_num(int num, int zeros);

/*
 * Utility
 */

/* Get the size of the data buffer needed for telemetry */
int irsdk_get_buf_len(void);

#endif /* IRA_IRSDK_H */
