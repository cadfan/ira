/*
 * ira - iRacing Application
 * YAML Session Info Parser
 *
 * Copyright (c) 2026 Christopher Griffiths
 *
 * Adapted from iRacing SDK (yaml_parser.cpp)
 * Original Copyright (c) 2013, iRacing.com Motorsport Simulations, LLC.
 */

#ifndef IRA_YAML_PARSER_H
#define IRA_YAML_PARSER_H

#include <stdbool.h>

/*
 * Parse a YAML string and extract a value by path.
 *
 * Path format: "Key1:Key2:Key3" for nested values
 * Array access: "Key1:{index}Key2" where {index} matches a value
 *
 * Example paths:
 *   "WeekendInfo:TrackName"
 *   "DriverInfo:DriverCarIdx"
 *   "DriverInfo:Drivers:{CarIdx}UserName"  (where CarIdx matches a number)
 *
 * Parameters:
 *   data - The YAML string to parse
 *   path - The path to the value
 *   val  - Output: pointer to the start of the value in data
 *   len  - Output: length of the value string
 *
 * Returns: true if found, false otherwise
 */
bool yaml_parse(const char *data, const char *path, const char **val, int *len);

/*
 * Parse a YAML value into a string buffer.
 * Copies the value and null-terminates it.
 *
 * Returns: true if found and copied, false otherwise
 */
bool yaml_parse_string(const char *data, const char *path, char *buf, int buf_size);

/*
 * Parse a YAML value as an integer.
 *
 * Returns: true if found and parsed, false otherwise
 */
bool yaml_parse_int(const char *data, const char *path, int *value);

/*
 * Parse a YAML value as a float.
 *
 * Returns: true if found and parsed, false otherwise
 */
bool yaml_parse_float(const char *data, const char *path, float *value);

/*
 * Parse a YAML value as a double.
 *
 * Returns: true if found and parsed, false otherwise
 */
bool yaml_parse_double(const char *data, const char *path, double *value);

#endif /* IRA_YAML_PARSER_H */
