/*
 * ira - iRacing Application
 * iRacing SDK Implementation (Pure C)
 *
 * Copyright (c) 2026 Christopher Griffiths
 *
 * Adapted from iRacing SDK (irsdk_utils.cpp)
 * Original Copyright (c) 2013, iRacing.com Motorsport Simulations, LLC.
 */

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>
#include <time.h>

#include "irsdk.h"

/* Link required Windows libraries */
#pragma comment(lib, "winmm")
#pragma comment(lib, "user32")

/* Local state */
static HANDLE g_data_valid_event = NULL;
static HANDLE g_mem_map_file = NULL;
static const char *g_shared_mem = NULL;
static const irsdk_Header *g_header = NULL;

static int g_last_tick_count = INT_MAX;
static bool g_is_initialized = false;

static const double TIMEOUT_SECONDS = 30.0;
static time_t g_last_valid_time = 0;

/*
 * Initialize connection to iRacing
 */
bool irsdk_startup(void)
{
    if (!g_mem_map_file) {
        g_mem_map_file = OpenFileMappingA(FILE_MAP_READ, FALSE, IRSDK_MEMMAPFILENAME);
        g_last_tick_count = INT_MAX;
    }

    if (g_mem_map_file) {
        if (!g_shared_mem) {
            g_shared_mem = (const char *)MapViewOfFile(g_mem_map_file, FILE_MAP_READ, 0, 0, 0);
            g_header = (const irsdk_Header *)g_shared_mem;
            g_last_tick_count = INT_MAX;
        }

        if (g_shared_mem) {
            if (!g_data_valid_event) {
                g_data_valid_event = OpenEventA(SYNCHRONIZE, FALSE, IRSDK_DATAVALIDEVENTNAME);
                g_last_tick_count = INT_MAX;
            }

            if (g_data_valid_event) {
                g_is_initialized = true;
                return true;
            }
        }
    }

    g_is_initialized = false;
    return false;
}

/*
 * Shutdown and cleanup
 */
void irsdk_shutdown(void)
{
    if (g_data_valid_event) {
        CloseHandle(g_data_valid_event);
    }

    if (g_shared_mem) {
        UnmapViewOfFile(g_shared_mem);
    }

    if (g_mem_map_file) {
        CloseHandle(g_mem_map_file);
    }

    g_data_valid_event = NULL;
    g_shared_mem = NULL;
    g_header = NULL;
    g_mem_map_file = NULL;

    g_is_initialized = false;
    g_last_tick_count = INT_MAX;
}

/*
 * Check for new data
 */
bool irsdk_get_new_data(char *data)
{
    if (g_is_initialized || irsdk_startup()) {
        /* If sim is not active, no new data */
        if (!(g_header->status & IRSDK_ST_CONNECTED)) {
            g_last_tick_count = INT_MAX;
            return false;
        }

        /* Find the latest buffer */
        int latest = 0;
        for (int i = 1; i < g_header->num_buf; i++) {
            if (g_header->var_buf[latest].tick_count < g_header->var_buf[i].tick_count) {
                latest = i;
            }
        }

        /* If newer than last received, report new data */
        if (g_last_tick_count < g_header->var_buf[latest].tick_count) {
            if (data) {
                /* Try twice to get consistent data */
                for (int count = 0; count < 2; count++) {
                    int cur_tick = g_header->var_buf[latest].tick_count;
                    memcpy(data, g_shared_mem + g_header->var_buf[latest].buf_offset, g_header->buf_len);

                    /* Verify data didn't change during copy */
                    if (cur_tick == g_header->var_buf[latest].tick_count) {
                        g_last_tick_count = cur_tick;
                        g_last_valid_time = time(NULL);
                        return true;
                    }
                }
                /* Data changed during copy */
                return false;
            } else {
                g_last_tick_count = g_header->var_buf[latest].tick_count;
                g_last_valid_time = time(NULL);
                return true;
            }
        }
        /* If older, reset - probably disconnected */
        else if (g_last_tick_count > g_header->var_buf[latest].tick_count) {
            g_last_tick_count = g_header->var_buf[latest].tick_count;
            return false;
        }
    }

    return false;
}

/*
 * Wait for new data with timeout
 */
bool irsdk_wait_for_data(int timeout_ms, char *data)
{
    if (g_is_initialized || irsdk_startup()) {
        /* Check before sleeping */
        if (irsdk_get_new_data(data)) {
            return true;
        }

        /* Wait for signal */
        WaitForSingleObject(g_data_valid_event, timeout_ms);

        /* Check again after waking */
        if (irsdk_get_new_data(data)) {
            return true;
        }
    }

    /* Sleep on error */
    if (timeout_ms > 0) {
        Sleep(timeout_ms);
    }

    return false;
}

/*
 * Check connection status
 */
bool irsdk_is_connected(void)
{
    if (g_is_initialized && g_header) {
        int elapsed = (int)difftime(time(NULL), g_last_valid_time);
        return (g_header->status & IRSDK_ST_CONNECTED) && (elapsed < TIMEOUT_SECONDS);
    }
    return false;
}

/*
 * Get the header
 */
const irsdk_Header *irsdk_get_header(void)
{
    if (g_is_initialized) {
        return g_header;
    }
    return NULL;
}

/*
 * Get the session info YAML string
 */
const char *irsdk_get_session_info(void)
{
    if (g_is_initialized && g_header) {
        return g_shared_mem + g_header->session_info_offset;
    }
    return NULL;
}

/*
 * Get session info update counter
 */
int irsdk_get_session_info_update(void)
{
    if (g_is_initialized && g_header) {
        return g_header->session_info_update;
    }
    return -1;
}

/*
 * Get variable headers array
 */
const irsdk_VarHeader *irsdk_get_var_headers(void)
{
    if (g_is_initialized && g_header) {
        return (const irsdk_VarHeader *)(g_shared_mem + g_header->var_header_offset);
    }
    return NULL;
}

/*
 * Get a specific variable header
 */
const irsdk_VarHeader *irsdk_get_var_header(int index)
{
    if (g_is_initialized && g_header) {
        if (index >= 0 && index < g_header->num_vars) {
            const irsdk_VarHeader *headers = irsdk_get_var_headers();
            return &headers[index];
        }
    }
    return NULL;
}

/*
 * Find variable index by name
 */
int irsdk_var_name_to_index(const char *name)
{
    if (!name || !g_is_initialized || !g_header) {
        return -1;
    }

    for (int i = 0; i < g_header->num_vars; i++) {
        const irsdk_VarHeader *var = irsdk_get_var_header(i);
        if (var && strncmp(name, var->name, IRSDK_MAX_STRING) == 0) {
            return i;
        }
    }

    return -1;
}

/*
 * Find variable offset by name
 */
int irsdk_var_name_to_offset(const char *name)
{
    if (!name || !g_is_initialized || !g_header) {
        return -1;
    }

    for (int i = 0; i < g_header->num_vars; i++) {
        const irsdk_VarHeader *var = irsdk_get_var_header(i);
        if (var && strncmp(name, var->name, IRSDK_MAX_STRING) == 0) {
            return var->offset;
        }
    }

    return -1;
}

/*
 * Get the data buffer length
 */
int irsdk_get_buf_len(void)
{
    if (g_is_initialized && g_header) {
        return g_header->buf_len;
    }
    return 0;
}

/*
 * Variable value helpers
 */

bool irsdk_get_var_bool(const char *data, int var_offset, int entry)
{
    if (data && var_offset >= 0) {
        return ((const bool *)(data + var_offset))[entry];
    }
    return false;
}

int irsdk_get_var_int(const char *data, int var_offset, int entry)
{
    if (data && var_offset >= 0) {
        return ((const int *)(data + var_offset))[entry];
    }
    return 0;
}

float irsdk_get_var_float(const char *data, int var_offset, int entry)
{
    if (data && var_offset >= 0) {
        return ((const float *)(data + var_offset))[entry];
    }
    return 0.0f;
}

double irsdk_get_var_double(const char *data, int var_offset, int entry)
{
    if (data && var_offset >= 0) {
        return ((const double *)(data + var_offset))[entry];
    }
    return 0.0;
}

/*
 * Broadcast message ID
 */
static unsigned int get_broadcast_msg_id(void)
{
    static unsigned int msg_id = 0;
    if (msg_id == 0) {
        msg_id = RegisterWindowMessageA(IRSDK_BROADCASTMSGNAME);
    }
    return msg_id;
}

/*
 * Broadcast message with 3 short parameters
 */
void irsdk_broadcast_msg(irsdk_BroadcastMsg msg, int var1, int var2, int var3)
{
    irsdk_broadcast_msg_int(msg, var1, MAKELONG(var2, var3));
}

/*
 * Broadcast message with float parameter
 */
void irsdk_broadcast_msg_float(irsdk_BroadcastMsg msg, int var1, float var2)
{
    /* Multiply by 2^16-1 to move fractional part to integer */
    int real = (int)(var2 * 65536.0f);
    irsdk_broadcast_msg_int(msg, var1, real);
}

/*
 * Broadcast message with int parameter
 */
void irsdk_broadcast_msg_int(irsdk_BroadcastMsg msg, int var1, int var2)
{
    unsigned int msg_id = get_broadcast_msg_id();

    if (msg_id && msg >= 0 && msg < IRSDK_BROADCAST_LAST) {
        SendNotifyMessageA(HWND_BROADCAST, msg_id, MAKELONG(msg, var1), var2);
    }
}

/*
 * Pad car number with leading zeros
 */
int irsdk_pad_car_num(int num, int zeros)
{
    int result = num;
    int num_place = 1;

    if (num > 99) {
        num_place = 3;
    } else if (num > 9) {
        num_place = 2;
    }

    if (zeros) {
        num_place += zeros;
        result = num + 1000 * num_place;
    }

    return result;
}
