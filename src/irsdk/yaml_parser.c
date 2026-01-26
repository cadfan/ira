/*
 * ira - iRacing Application
 * YAML Session Info Parser
 *
 * Copyright (c) 2026 Christopher Griffiths
 *
 * Adapted from iRacing SDK (yaml_parser.cpp)
 * Original Copyright (c) 2013, iRacing.com Motorsport Simulations, LLC.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "yaml_parser.h"

/* Parser state machine states */
typedef enum {
    YAML_STATE_SPACE,
    YAML_STATE_KEY,
    YAML_STATE_KEYSEP,
    YAML_STATE_VALUE,
    YAML_STATE_NEWLINE
} yaml_state;

/*
 * Parse YAML and extract value by path
 */
bool yaml_parse(const char *data, const char *path, const char **val, int *len)
{
    if (!data || !path || !val || !len) {
        return false;
    }

    /* Initialize output */
    *val = NULL;
    *len = 0;

    int depth = 0;
    yaml_state state = YAML_STATE_SPACE;

    const char *keystr = NULL;
    int keylen = 0;

    const char *valuestr = NULL;
    int valuelen = 0;

    const char *pathptr = path;
    int pathdepth = 0;

    while (*data) {
        switch (*data) {
        case ' ':
            if (state == YAML_STATE_NEWLINE) {
                state = YAML_STATE_SPACE;
            }
            if (state == YAML_STATE_SPACE) {
                depth++;
            } else if (state == YAML_STATE_KEY) {
                keylen++;
            } else if (state == YAML_STATE_VALUE) {
                valuelen++;
            }
            break;

        case '-':
            if (state == YAML_STATE_NEWLINE) {
                state = YAML_STATE_SPACE;
            }
            if (state == YAML_STATE_SPACE) {
                depth++;
            } else if (state == YAML_STATE_KEY) {
                keylen++;
            } else if (state == YAML_STATE_VALUE) {
                valuelen++;
            } else if (state == YAML_STATE_KEYSEP) {
                state = YAML_STATE_VALUE;
                valuestr = data;
                valuelen = 1;
            }
            break;

        case ':':
            if (state == YAML_STATE_KEY) {
                state = YAML_STATE_KEYSEP;
                keylen++;
            } else if (state == YAML_STATE_KEYSEP) {
                state = YAML_STATE_VALUE;
                valuestr = data;
            } else if (state == YAML_STATE_VALUE) {
                valuelen++;
            }
            break;

        case '\n':
        case '\r':
            if (state != YAML_STATE_NEWLINE) {
                if (depth < pathdepth) {
                    return false;
                } else if (keylen && strncmp(keystr, pathptr, keylen) == 0) {
                    bool found = true;

                    /* Do we need to test the value? */
                    if (*(pathptr + keylen) == '{') {
                        /* Search for closing brace */
                        int pathvaluelen = keylen + 1;
                        while (*(pathptr + pathvaluelen) && *(pathptr + pathvaluelen) != '}') {
                            pathvaluelen++;
                        }

                        if (valuelen == pathvaluelen - (keylen + 1) &&
                            strncmp(valuestr, (pathptr + keylen + 1), valuelen) == 0) {
                            pathptr += valuelen + 2;
                        } else {
                            found = false;
                        }
                    }

                    if (found) {
                        pathptr += keylen;
                        pathdepth = depth;

                        if (*pathptr == '\0') {
                            *val = valuestr;
                            *len = valuelen;
                            return true;
                        }
                    }
                }

                depth = 0;
                keylen = 0;
                valuelen = 0;
            }
            state = YAML_STATE_NEWLINE;
            break;

        default:
            if (state == YAML_STATE_SPACE || state == YAML_STATE_NEWLINE) {
                state = YAML_STATE_KEY;
                keystr = data;
                keylen = 0;
            } else if (state == YAML_STATE_KEYSEP) {
                state = YAML_STATE_VALUE;
                valuestr = data;
                valuelen = 0;
            }
            if (state == YAML_STATE_KEY) {
                keylen++;
            }
            if (state == YAML_STATE_VALUE) {
                valuelen++;
            }
            break;
        }

        data++;
    }

    return false;
}

/*
 * Parse YAML value into string buffer
 */
bool yaml_parse_string(const char *data, const char *path, char *buf, int buf_size)
{
    const char *val = NULL;
    int len = 0;

    if (!buf || buf_size <= 0) {
        return false;
    }

    buf[0] = '\0';

    if (yaml_parse(data, path, &val, &len)) {
        if (val && len > 0) {
            int copy_len = (len < buf_size - 1) ? len : (buf_size - 1);
            strncpy(buf, val, copy_len);
            buf[copy_len] = '\0';
            return true;
        }
    }

    return false;
}

/*
 * Parse YAML value as integer
 */
bool yaml_parse_int(const char *data, const char *path, int *value)
{
    const char *val = NULL;
    int len = 0;

    if (!value) {
        return false;
    }

    *value = 0;

    if (yaml_parse(data, path, &val, &len)) {
        if (val && len > 0) {
            /* Create temporary null-terminated string */
            char temp[64];
            int copy_len = (len < 63) ? len : 63;
            strncpy(temp, val, copy_len);
            temp[copy_len] = '\0';
            *value = atoi(temp);
            return true;
        }
    }

    return false;
}

/*
 * Parse YAML value as float
 */
bool yaml_parse_float(const char *data, const char *path, float *value)
{
    const char *val = NULL;
    int len = 0;

    if (!value) {
        return false;
    }

    *value = 0.0f;

    if (yaml_parse(data, path, &val, &len)) {
        if (val && len > 0) {
            char temp[64];
            int copy_len = (len < 63) ? len : 63;
            strncpy(temp, val, copy_len);
            temp[copy_len] = '\0';
            *value = (float)atof(temp);
            return true;
        }
    }

    return false;
}

/*
 * Parse YAML value as double
 */
bool yaml_parse_double(const char *data, const char *path, double *value)
{
    const char *val = NULL;
    int len = 0;

    if (!value) {
        return false;
    }

    *value = 0.0;

    if (yaml_parse(data, path, &val, &len)) {
        if (val && len > 0) {
            char temp[64];
            int copy_len = (len < 63) ? len : 63;
            strncpy(temp, val, copy_len);
            temp[copy_len] = '\0';
            *value = atof(temp);
            return true;
        }
    }

    return false;
}
