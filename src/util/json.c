/*
 * ira - iRacing Application
 * Simple JSON Parser/Writer
 *
 * Copyright (c) 2026 Christopher Griffiths
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "json.h"

/* Parser context */
typedef struct {
    const char *str;
    const char *ptr;
    int depth;
} json_parser;

/* Forward declarations */
static json_value *parse_value(json_parser *p);
static void skip_whitespace(json_parser *p);

/*
 * Memory allocation helpers
 */

static char *str_dup(const char *s)
{
    if (!s) return NULL;
    size_t len = strlen(s);
    char *dup = (char *)malloc(len + 1);
    if (dup) {
        memcpy(dup, s, len + 1);
    }
    return dup;
}

static char *str_ndup(const char *s, size_t n)
{
    if (!s) return NULL;
    char *dup = (char *)malloc(n + 1);
    if (dup) {
        memcpy(dup, s, n);
        dup[n] = '\0';
    }
    return dup;
}

/*
 * Value creation
 */

json_value *json_new_null(void)
{
    json_value *val = (json_value *)calloc(1, sizeof(json_value));
    if (val) {
        val->type = JSON_NULL;
    }
    return val;
}

json_value *json_new_bool(bool b)
{
    json_value *val = (json_value *)calloc(1, sizeof(json_value));
    if (val) {
        val->type = JSON_BOOL;
        val->data.bool_val = b;
    }
    return val;
}

json_value *json_new_number(double n)
{
    json_value *val = (json_value *)calloc(1, sizeof(json_value));
    if (val) {
        val->type = JSON_NUMBER;
        val->data.number_val = n;
    }
    return val;
}

json_value *json_new_string(const char *s)
{
    json_value *val = (json_value *)calloc(1, sizeof(json_value));
    if (val) {
        val->type = JSON_STRING;
        val->data.string_val = str_dup(s ? s : "");
    }
    return val;
}

json_value *json_new_array(void)
{
    json_value *val = (json_value *)calloc(1, sizeof(json_value));
    if (val) {
        val->type = JSON_ARRAY;
        val->data.array_val = NULL;
    }
    return val;
}

json_value *json_new_object(void)
{
    json_value *val = (json_value *)calloc(1, sizeof(json_value));
    if (val) {
        val->type = JSON_OBJECT;
        val->data.object_val = NULL;
    }
    return val;
}

/*
 * Value access
 */

json_type json_get_type(const json_value *val)
{
    return val ? val->type : JSON_NULL;
}

bool json_get_bool(const json_value *val)
{
    if (val && val->type == JSON_BOOL) {
        return val->data.bool_val;
    }
    return false;
}

double json_get_number(const json_value *val)
{
    if (val && val->type == JSON_NUMBER) {
        return val->data.number_val;
    }
    return 0.0;
}

int json_get_int(const json_value *val)
{
    return (int)json_get_number(val);
}

const char *json_get_string(const json_value *val)
{
    if (val && val->type == JSON_STRING) {
        return val->data.string_val;
    }
    return NULL;
}

int json_array_length(const json_value *val)
{
    if (!val || val->type != JSON_ARRAY) {
        return 0;
    }
    int count = 0;
    json_element *elem = val->data.array_val;
    while (elem) {
        count++;
        elem = elem->next;
    }
    return count;
}

json_value *json_array_get(const json_value *val, int index)
{
    if (!val || val->type != JSON_ARRAY || index < 0) {
        return NULL;
    }
    json_element *elem = val->data.array_val;
    for (int i = 0; elem && i < index; i++) {
        elem = elem->next;
    }
    return elem ? elem->value : NULL;
}

json_value *json_object_get(const json_value *val, const char *key)
{
    if (!val || val->type != JSON_OBJECT || !key) {
        return NULL;
    }
    json_pair *pair = val->data.object_val;
    while (pair) {
        if (pair->key && strcmp(pair->key, key) == 0) {
            return pair->value;
        }
        pair = pair->next;
    }
    return NULL;
}

bool json_object_has(const json_value *val, const char *key)
{
    return json_object_get(val, key) != NULL;
}

/*
 * Value modification
 */

bool json_array_push(json_value *arr, json_value *val)
{
    if (!arr || arr->type != JSON_ARRAY || !val) {
        return false;
    }

    json_element *elem = (json_element *)calloc(1, sizeof(json_element));
    if (!elem) {
        return false;
    }
    elem->value = val;
    elem->next = NULL;

    if (!arr->data.array_val) {
        arr->data.array_val = elem;
    } else {
        json_element *last = arr->data.array_val;
        while (last->next) {
            last = last->next;
        }
        last->next = elem;
    }
    return true;
}

bool json_object_set(json_value *obj, const char *key, json_value *val)
{
    if (!obj || obj->type != JSON_OBJECT || !key || !val) {
        return false;
    }

    /* Check if key already exists */
    json_pair *pair = obj->data.object_val;
    while (pair) {
        if (pair->key && strcmp(pair->key, key) == 0) {
            json_free(pair->value);
            pair->value = val;
            return true;
        }
        pair = pair->next;
    }

    /* Create new pair */
    pair = (json_pair *)calloc(1, sizeof(json_pair));
    if (!pair) {
        return false;
    }
    pair->key = str_dup(key);
    pair->value = val;
    pair->next = obj->data.object_val;
    obj->data.object_val = pair;
    return true;
}

/*
 * Memory management
 */

void json_free(json_value *val)
{
    if (!val) return;

    switch (val->type) {
    case JSON_STRING:
        free(val->data.string_val);
        break;
    case JSON_ARRAY:
        {
            json_element *elem = val->data.array_val;
            while (elem) {
                json_element *next = elem->next;
                json_free(elem->value);
                free(elem);
                elem = next;
            }
        }
        break;
    case JSON_OBJECT:
        {
            json_pair *pair = val->data.object_val;
            while (pair) {
                json_pair *next = pair->next;
                free(pair->key);
                json_free(pair->value);
                free(pair);
                pair = next;
            }
        }
        break;
    default:
        break;
    }
    free(val);
}

/*
 * Parsing
 */

static void skip_whitespace(json_parser *p)
{
    while (*p->ptr && isspace((unsigned char)*p->ptr)) {
        p->ptr++;
    }
}

static json_value *parse_string(json_parser *p)
{
    if (*p->ptr != '"') return NULL;
    p->ptr++; /* Skip opening quote */

    const char *start = p->ptr;
    size_t len = 0;

    /* Calculate length and check for escapes */
    while (*p->ptr && *p->ptr != '"') {
        if (*p->ptr == '\\' && *(p->ptr + 1)) {
            p->ptr += 2;
            len++;
        } else {
            p->ptr++;
            len++;
        }
    }

    if (*p->ptr != '"') return NULL;

    /* Build the string, handling escapes */
    char *str = (char *)malloc(len + 1);
    if (!str) return NULL;

    const char *src = start;
    char *dst = str;
    while (src < p->ptr) {
        if (*src == '\\' && src + 1 < p->ptr) {
            src++;
            switch (*src) {
            case '"': *dst++ = '"'; break;
            case '\\': *dst++ = '\\'; break;
            case '/': *dst++ = '/'; break;
            case 'b': *dst++ = '\b'; break;
            case 'f': *dst++ = '\f'; break;
            case 'n': *dst++ = '\n'; break;
            case 'r': *dst++ = '\r'; break;
            case 't': *dst++ = '\t'; break;
            default: *dst++ = *src; break;
            }
            src++;
        } else {
            *dst++ = *src++;
        }
    }
    *dst = '\0';

    p->ptr++; /* Skip closing quote */

    json_value *val = json_new_string(str);
    free(str);
    return val;
}

static json_value *parse_number(json_parser *p)
{
    const char *start = p->ptr;

    if (*p->ptr == '-') p->ptr++;

    while (isdigit((unsigned char)*p->ptr)) p->ptr++;

    if (*p->ptr == '.') {
        p->ptr++;
        while (isdigit((unsigned char)*p->ptr)) p->ptr++;
    }

    if (*p->ptr == 'e' || *p->ptr == 'E') {
        p->ptr++;
        if (*p->ptr == '+' || *p->ptr == '-') p->ptr++;
        while (isdigit((unsigned char)*p->ptr)) p->ptr++;
    }

    char *temp = str_ndup(start, p->ptr - start);
    if (!temp) return NULL;

    double num = atof(temp);
    free(temp);

    return json_new_number(num);
}

static json_value *parse_array(json_parser *p)
{
    if (*p->ptr != '[') return NULL;
    p->ptr++;
    p->depth++;

    if (p->depth > JSON_MAX_DEPTH) {
        return NULL;
    }

    json_value *arr = json_new_array();
    if (!arr) {
        p->depth--;
        return NULL;
    }

    skip_whitespace(p);

    if (*p->ptr == ']') {
        p->ptr++;
        p->depth--;
        return arr;
    }

    while (1) {
        skip_whitespace(p);
        json_value *elem = parse_value(p);
        if (!elem) {
            json_free(arr);
            p->depth--;
            return NULL;
        }
        json_array_push(arr, elem);

        skip_whitespace(p);
        if (*p->ptr == ']') {
            p->ptr++;
            break;
        }
        if (*p->ptr != ',') {
            json_free(arr);
            p->depth--;
            return NULL;
        }
        p->ptr++;
    }

    p->depth--;
    return arr;
}

static json_value *parse_object(json_parser *p)
{
    if (*p->ptr != '{') return NULL;
    p->ptr++;
    p->depth++;

    if (p->depth > JSON_MAX_DEPTH) {
        return NULL;
    }

    json_value *obj = json_new_object();
    if (!obj) {
        p->depth--;
        return NULL;
    }

    skip_whitespace(p);

    if (*p->ptr == '}') {
        p->ptr++;
        p->depth--;
        return obj;
    }

    while (1) {
        skip_whitespace(p);

        /* Parse key */
        json_value *key_val = parse_string(p);
        if (!key_val) {
            json_free(obj);
            p->depth--;
            return NULL;
        }
        const char *key = json_get_string(key_val);

        skip_whitespace(p);
        if (*p->ptr != ':') {
            json_free(key_val);
            json_free(obj);
            p->depth--;
            return NULL;
        }
        p->ptr++;

        skip_whitespace(p);
        json_value *val = parse_value(p);
        if (!val) {
            json_free(key_val);
            json_free(obj);
            p->depth--;
            return NULL;
        }

        json_object_set(obj, key, val);
        json_free(key_val);

        skip_whitespace(p);
        if (*p->ptr == '}') {
            p->ptr++;
            break;
        }
        if (*p->ptr != ',') {
            json_free(obj);
            p->depth--;
            return NULL;
        }
        p->ptr++;
    }

    p->depth--;
    return obj;
}

static json_value *parse_value(json_parser *p)
{
    skip_whitespace(p);

    if (*p->ptr == '"') {
        return parse_string(p);
    }
    if (*p->ptr == '[') {
        return parse_array(p);
    }
    if (*p->ptr == '{') {
        return parse_object(p);
    }
    if (*p->ptr == '-' || isdigit((unsigned char)*p->ptr)) {
        return parse_number(p);
    }
    if (strncmp(p->ptr, "true", 4) == 0) {
        p->ptr += 4;
        return json_new_bool(true);
    }
    if (strncmp(p->ptr, "false", 5) == 0) {
        p->ptr += 5;
        return json_new_bool(false);
    }
    if (strncmp(p->ptr, "null", 4) == 0) {
        p->ptr += 4;
        return json_new_null();
    }

    return NULL;
}

json_value *json_parse(const char *str)
{
    if (!str) return NULL;

    json_parser p = { str, str, 0 };
    return parse_value(&p);
}

json_value *json_parse_file(const char *filename)
{
    if (!filename) return NULL;

    FILE *f = fopen(filename, "rb");
    if (!f) return NULL;

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);

    char *str = (char *)malloc(size + 1);
    if (!str) {
        fclose(f);
        return NULL;
    }

    size_t read = fread(str, 1, size, f);
    str[read] = '\0';
    fclose(f);

    json_value *val = json_parse(str);
    free(str);
    return val;
}

/*
 * Serialization
 */

typedef struct {
    char *buf;
    size_t len;
    size_t cap;
    int indent;
    bool pretty;
} json_writer;

static bool writer_grow(json_writer *w, size_t need)
{
    if (w->len + need < w->cap) return true;

    size_t new_cap = w->cap * 2;
    if (new_cap < w->len + need) new_cap = w->len + need + 256;

    char *new_buf = (char *)realloc(w->buf, new_cap);
    if (!new_buf) return false;

    w->buf = new_buf;
    w->cap = new_cap;
    return true;
}

static bool writer_append(json_writer *w, const char *s)
{
    size_t len = strlen(s);
    if (!writer_grow(w, len + 1)) return false;
    memcpy(w->buf + w->len, s, len);
    w->len += len;
    w->buf[w->len] = '\0';
    return true;
}

static bool writer_append_char(json_writer *w, char c)
{
    if (!writer_grow(w, 2)) return false;
    w->buf[w->len++] = c;
    w->buf[w->len] = '\0';
    return true;
}

static bool writer_indent(json_writer *w)
{
    if (!w->pretty) return true;
    for (int i = 0; i < w->indent; i++) {
        if (!writer_append(w, "  ")) return false;
    }
    return true;
}

static bool writer_newline(json_writer *w)
{
    if (!w->pretty) return true;
    return writer_append_char(w, '\n');
}

static bool write_value(json_writer *w, const json_value *val);

static bool write_string(json_writer *w, const char *s)
{
    if (!writer_append_char(w, '"')) return false;

    while (*s) {
        switch (*s) {
        case '"': if (!writer_append(w, "\\\"")) return false; break;
        case '\\': if (!writer_append(w, "\\\\")) return false; break;
        case '\b': if (!writer_append(w, "\\b")) return false; break;
        case '\f': if (!writer_append(w, "\\f")) return false; break;
        case '\n': if (!writer_append(w, "\\n")) return false; break;
        case '\r': if (!writer_append(w, "\\r")) return false; break;
        case '\t': if (!writer_append(w, "\\t")) return false; break;
        default:
            if ((unsigned char)*s < 32) {
                char buf[8];
                snprintf(buf, sizeof(buf), "\\u%04x", (unsigned char)*s);
                if (!writer_append(w, buf)) return false;
            } else {
                if (!writer_append_char(w, *s)) return false;
            }
        }
        s++;
    }

    return writer_append_char(w, '"');
}

static bool write_value(json_writer *w, const json_value *val)
{
    if (!val) {
        return writer_append(w, "null");
    }

    switch (val->type) {
    case JSON_NULL:
        return writer_append(w, "null");

    case JSON_BOOL:
        return writer_append(w, val->data.bool_val ? "true" : "false");

    case JSON_NUMBER:
        {
            char buf[64];
            double n = val->data.number_val;
            if (n == (int)n) {
                snprintf(buf, sizeof(buf), "%d", (int)n);
            } else {
                snprintf(buf, sizeof(buf), "%g", n);
            }
            return writer_append(w, buf);
        }

    case JSON_STRING:
        return write_string(w, val->data.string_val ? val->data.string_val : "");

    case JSON_ARRAY:
        {
            if (!writer_append_char(w, '[')) return false;

            json_element *elem = val->data.array_val;
            bool first = true;

            if (elem && w->pretty) {
                w->indent++;
                if (!writer_newline(w)) return false;
            }

            while (elem) {
                if (!first) {
                    if (!writer_append_char(w, ',')) return false;
                    if (!writer_newline(w)) return false;
                }
                first = false;

                if (!writer_indent(w)) return false;
                if (!write_value(w, elem->value)) return false;

                elem = elem->next;
            }

            if (val->data.array_val && w->pretty) {
                w->indent--;
                if (!writer_newline(w)) return false;
                if (!writer_indent(w)) return false;
            }

            return writer_append_char(w, ']');
        }

    case JSON_OBJECT:
        {
            if (!writer_append_char(w, '{')) return false;

            json_pair *pair = val->data.object_val;
            bool first = true;

            if (pair && w->pretty) {
                w->indent++;
                if (!writer_newline(w)) return false;
            }

            while (pair) {
                if (!first) {
                    if (!writer_append_char(w, ',')) return false;
                    if (!writer_newline(w)) return false;
                }
                first = false;

                if (!writer_indent(w)) return false;
                if (!write_string(w, pair->key ? pair->key : "")) return false;
                if (!writer_append_char(w, ':')) return false;
                if (w->pretty && !writer_append_char(w, ' ')) return false;
                if (!write_value(w, pair->value)) return false;

                pair = pair->next;
            }

            if (val->data.object_val && w->pretty) {
                w->indent--;
                if (!writer_newline(w)) return false;
                if (!writer_indent(w)) return false;
            }

            return writer_append_char(w, '}');
        }
    }

    return false;
}

char *json_stringify(const json_value *val)
{
    json_writer w = { NULL, 0, 0, 0, false };
    w.buf = (char *)malloc(256);
    if (!w.buf) return NULL;
    w.cap = 256;
    w.buf[0] = '\0';

    if (!write_value(&w, val)) {
        free(w.buf);
        return NULL;
    }

    return w.buf;
}

char *json_stringify_pretty(const json_value *val)
{
    json_writer w = { NULL, 0, 0, 0, true };
    w.buf = (char *)malloc(256);
    if (!w.buf) return NULL;
    w.cap = 256;
    w.buf[0] = '\0';

    if (!write_value(&w, val)) {
        free(w.buf);
        return NULL;
    }

    return w.buf;
}

bool json_write_file(const json_value *val, const char *filename, bool pretty)
{
    char *str = pretty ? json_stringify_pretty(val) : json_stringify(val);
    if (!str) return false;

    FILE *f = fopen(filename, "wb");
    if (!f) {
        free(str);
        return false;
    }

    size_t len = strlen(str);
    size_t written = fwrite(str, 1, len, f);
    fclose(f);
    free(str);

    return written == len;
}
