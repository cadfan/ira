/*
 * ira - iRacing Application
 * Simple JSON Parser/Writer
 *
 * Copyright (c) 2026 Christopher Griffiths
 */

#ifndef IRA_JSON_H
#define IRA_JSON_H

#include <stdbool.h>
#include <stddef.h>

/* Maximum nesting depth for JSON objects/arrays */
#define JSON_MAX_DEPTH 32

/* JSON value types */
typedef enum {
    JSON_NULL,
    JSON_BOOL,
    JSON_NUMBER,
    JSON_STRING,
    JSON_ARRAY,
    JSON_OBJECT
} json_type;

/* Forward declaration */
typedef struct json_value json_value;

/* JSON object key-value pair */
typedef struct json_pair {
    char *key;
    json_value *value;
    struct json_pair *next;
} json_pair;

/* JSON array element */
typedef struct json_element {
    json_value *value;
    struct json_element *next;
} json_element;

/* JSON value structure */
struct json_value {
    json_type type;
    union {
        bool bool_val;
        double number_val;
        char *string_val;
        json_element *array_val;
        json_pair *object_val;
    } data;
};

/*
 * Parsing functions
 */

/* Parse a JSON string into a value tree. Returns NULL on error. */
json_value *json_parse(const char *str);

/* Parse a JSON file. Returns NULL on error. */
json_value *json_parse_file(const char *filename);

/*
 * Value access functions
 */

/* Get type of a JSON value */
json_type json_get_type(const json_value *val);

/* Get boolean value. Returns false if not a bool. */
bool json_get_bool(const json_value *val);

/* Get number value. Returns 0.0 if not a number. */
double json_get_number(const json_value *val);

/* Get integer value. Returns 0 if not a number. */
int json_get_int(const json_value *val);

/* Get string value. Returns NULL if not a string. */
const char *json_get_string(const json_value *val);

/* Get array length. Returns 0 if not an array. */
int json_array_length(const json_value *val);

/* Get array element by index. Returns NULL if out of bounds. */
json_value *json_array_get(const json_value *val, int index);

/* Get object property by key. Returns NULL if not found. */
json_value *json_object_get(const json_value *val, const char *key);

/* Check if object has a key */
bool json_object_has(const json_value *val, const char *key);

/*
 * Value creation functions
 */

/* Create a null value */
json_value *json_new_null(void);

/* Create a boolean value */
json_value *json_new_bool(bool val);

/* Create a number value */
json_value *json_new_number(double val);

/* Create a string value (copies the string) */
json_value *json_new_string(const char *val);

/* Create an empty array */
json_value *json_new_array(void);

/* Create an empty object */
json_value *json_new_object(void);

/* Add element to array */
bool json_array_push(json_value *arr, json_value *val);

/* Set object property (takes ownership of val, copies key) */
bool json_object_set(json_value *obj, const char *key, json_value *val);

/*
 * Serialization functions
 */

/* Convert JSON value to string. Caller must free the result. */
char *json_stringify(const json_value *val);

/* Convert JSON value to formatted string. Caller must free the result. */
char *json_stringify_pretty(const json_value *val);

/* Write JSON value to file */
bool json_write_file(const json_value *val, const char *filename, bool pretty);

/*
 * Memory management
 */

/* Free a JSON value and all its children */
void json_free(json_value *val);

#endif /* IRA_JSON_H */
