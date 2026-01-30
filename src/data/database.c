/*
 * ira - iRacing Application
 * Database Implementation
 *
 * Copyright (c) 2026 Christopher Griffiths
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#ifdef _WIN32
#include <windows.h>
#endif

#include "database.h"
#include "../util/json.h"

/* Default data file names */
#define TRACKS_FILE      "tracks.json"
#define CARS_FILE        "cars.json"
#define CAR_CLASSES_FILE "car_classes.json"
#define SERIES_FILE      "series.json"
#define SEASONS_FILE     "seasons.json"
#define OWNED_FILE       "owned_content.json"
#define FILTER_FILE      "filter.json"

/* Static path buffers */
static char g_tracks_path[MAX_PATH];
static char g_cars_path[MAX_PATH];
static char g_car_classes_path[MAX_PATH];
static char g_series_path[MAX_PATH];
static char g_seasons_path[MAX_PATH];
static char g_owned_path[MAX_PATH];
static char g_filter_path[MAX_PATH];
static bool g_paths_initialized = false;

/*
 * Initialize paths based on executable location
 */
static void init_paths(void)
{
    if (g_paths_initialized) return;

    char exe_path[MAX_PATH];
    DWORD len = GetModuleFileNameA(NULL, exe_path, MAX_PATH);

    if (len > 0 && len < MAX_PATH) {
        char *last_slash = strrchr(exe_path, '\\');
        if (last_slash) {
            *last_slash = '\0';
            snprintf(g_tracks_path, MAX_PATH, "%s\\%s", exe_path, TRACKS_FILE);
            snprintf(g_cars_path, MAX_PATH, "%s\\%s", exe_path, CARS_FILE);
            snprintf(g_car_classes_path, MAX_PATH, "%s\\%s", exe_path, CAR_CLASSES_FILE);
            snprintf(g_series_path, MAX_PATH, "%s\\%s", exe_path, SERIES_FILE);
            snprintf(g_seasons_path, MAX_PATH, "%s\\%s", exe_path, SEASONS_FILE);
            snprintf(g_owned_path, MAX_PATH, "%s\\%s", exe_path, OWNED_FILE);
            snprintf(g_filter_path, MAX_PATH, "%s\\%s", exe_path, FILTER_FILE);
            g_paths_initialized = true;
            return;
        }
    }

    /* Fallback to current directory */
    strncpy(g_tracks_path, TRACKS_FILE, MAX_PATH);
    strncpy(g_cars_path, CARS_FILE, MAX_PATH);
    strncpy(g_car_classes_path, CAR_CLASSES_FILE, MAX_PATH);
    strncpy(g_series_path, SERIES_FILE, MAX_PATH);
    strncpy(g_seasons_path, SEASONS_FILE, MAX_PATH);
    strncpy(g_owned_path, OWNED_FILE, MAX_PATH);
    strncpy(g_filter_path, FILTER_FILE, MAX_PATH);
    g_paths_initialized = true;
}

/*
 * Path getters
 */

const char *database_get_tracks_path(void)
{
    init_paths();
    return g_tracks_path;
}

const char *database_get_cars_path(void)
{
    init_paths();
    return g_cars_path;
}

const char *database_get_car_classes_path(void)
{
    init_paths();
    return g_car_classes_path;
}

const char *database_get_series_path(void)
{
    init_paths();
    return g_series_path;
}

const char *database_get_seasons_path(void)
{
    init_paths();
    return g_seasons_path;
}

const char *database_get_owned_path(void)
{
    init_paths();
    return g_owned_path;
}

const char *database_get_filter_path(void)
{
    init_paths();
    return g_filter_path;
}

/*
 * Lifecycle
 */

ira_database *database_create(void)
{
    ira_database *db = calloc(1, sizeof(ira_database));
    if (!db) return NULL;

    /* Set default filter values */
    db->filter.owned_content_only = true;
    db->filter.min_license = LICENSE_ROOKIE;
    db->filter.max_license = LICENSE_PRO_WC;
    db->filter.official_only = false;
    db->filter.min_race_mins = 0;
    db->filter.max_race_mins = 0;  /* 0 = no limit */

    return db;
}

void database_destroy(ira_database *db)
{
    if (!db) return;

    /* Free tracks */
    if (db->tracks) {
        free(db->tracks);
    }

    /* Free cars */
    if (db->cars) {
        free(db->cars);
    }

    /* Free car classes */
    if (db->car_classes) {
        free(db->car_classes);
    }

    /* Free series */
    if (db->series) {
        free(db->series);
    }

    /* Free seasons and their schedules */
    if (db->seasons) {
        for (int i = 0; i < db->season_count; i++) {
            season_free_schedule(&db->seasons[i]);
        }
        free(db->seasons);
    }

    /* Free owned content */
    owned_content_free(&db->owned);

    /* Free filter */
    filter_free(&db->filter);

    free(db);
}

/*
 * Helper: safe string copy
 */
static void safe_strcpy(char *dest, size_t dest_size, const char *src)
{
    if (!dest || !src || dest_size == 0) return;
    strncpy(dest, src, dest_size - 1);
    dest[dest_size - 1] = '\0';
}

/*
 * Helper: parse ISO timestamp to time_t
 */
static time_t parse_timestamp(const char *str)
{
    if (!str) return 0;

    struct tm tm = {0};
    int year, month, day, hour, min, sec;

    if (sscanf(str, "%d-%d-%dT%d:%d:%d", &year, &month, &day, &hour, &min, &sec) >= 3) {
        tm.tm_year = year - 1900;
        tm.tm_mon = month - 1;
        tm.tm_mday = day;
        tm.tm_hour = hour;
        tm.tm_min = min;
        tm.tm_sec = sec;
        return mktime(&tm);
    }

    return 0;
}

/*
 * Load tracks from JSON
 */
bool database_load_tracks(ira_database *db, const char *filename)
{
    if (!db || !filename) return false;

    json_value *root = json_parse_file(filename);
    if (!root) return false;

    json_value *updated = json_object_get(root, "last_updated");
    if (updated && json_get_type(updated) == JSON_STRING) {
        db->tracks_updated = parse_timestamp(json_get_string(updated));
    }

    json_value *tracks_arr = json_object_get(root, "tracks");
    if (!tracks_arr || json_get_type(tracks_arr) != JSON_ARRAY) {
        json_free(root);
        return false;
    }

    int count = json_array_length(tracks_arr);
    if (count == 0) {
        json_free(root);
        return true;
    }

    /* Allocate tracks array */
    db->tracks = calloc(count, sizeof(ira_track));
    if (!db->tracks) {
        json_free(root);
        return false;
    }
    db->track_count = count;

    /* Parse each track */
    for (int i = 0; i < count; i++) {
        json_value *t = json_array_get(tracks_arr, i);
        if (!t) continue;

        ira_track *track = &db->tracks[i];

        track->track_id = json_get_int(json_object_get(t, "track_id"));

        const char *name = json_get_string(json_object_get(t, "track_name"));
        if (name) safe_strcpy(track->track_name, sizeof(track->track_name), name);

        const char *config = json_get_string(json_object_get(t, "config_name"));
        if (config) safe_strcpy(track->config_name, sizeof(track->config_name), config);

        track->category = json_get_int(json_object_get(t, "category_id"));
        track->is_oval = json_get_bool(json_object_get(t, "is_oval"));
        track->is_dirt = json_get_bool(json_object_get(t, "is_dirt"));
        track->length_km = (float)json_get_number(json_object_get(t, "length_km"));
        track->corners = json_get_int(json_object_get(t, "corners"));
        track->max_cars = json_get_int(json_object_get(t, "max_cars"));
        track->grid_stalls = json_get_int(json_object_get(t, "grid_stalls"));
        track->pit_speed_kph = json_get_int(json_object_get(t, "pit_speed_kph"));
        track->price = (float)json_get_number(json_object_get(t, "price"));
        track->free_with_subscription = json_get_bool(json_object_get(t, "free"));
        track->retired = json_get_bool(json_object_get(t, "retired"));
        track->package_id = json_get_int(json_object_get(t, "package_id"));
        track->sku = json_get_int(json_object_get(t, "sku"));

        const char *loc = json_get_string(json_object_get(t, "location"));
        if (loc) safe_strcpy(track->location, sizeof(track->location), loc);

        track->latitude = (float)json_get_number(json_object_get(t, "latitude"));
        track->longitude = (float)json_get_number(json_object_get(t, "longitude"));
        track->night_lighting = json_get_bool(json_object_get(t, "night_lighting"));
        track->ai_enabled = json_get_bool(json_object_get(t, "ai_enabled"));
    }

    json_free(root);
    return true;
}

/*
 * Load cars from JSON
 */
bool database_load_cars(ira_database *db, const char *filename)
{
    if (!db || !filename) return false;

    json_value *root = json_parse_file(filename);
    if (!root) return false;

    json_value *updated = json_object_get(root, "last_updated");
    if (updated && json_get_type(updated) == JSON_STRING) {
        db->cars_updated = parse_timestamp(json_get_string(updated));
    }

    json_value *cars_arr = json_object_get(root, "cars");
    if (!cars_arr || json_get_type(cars_arr) != JSON_ARRAY) {
        json_free(root);
        return false;
    }

    int count = json_array_length(cars_arr);
    if (count == 0) {
        json_free(root);
        return true;
    }

    db->cars = calloc(count, sizeof(ira_car));
    if (!db->cars) {
        json_free(root);
        return false;
    }
    db->car_count = count;

    for (int i = 0; i < count; i++) {
        json_value *c = json_array_get(cars_arr, i);
        if (!c) continue;

        ira_car *car = &db->cars[i];

        car->car_id = json_get_int(json_object_get(c, "car_id"));

        const char *name = json_get_string(json_object_get(c, "car_name"));
        if (name) safe_strcpy(car->car_name, sizeof(car->car_name), name);

        const char *abbrev = json_get_string(json_object_get(c, "car_abbrev"));
        if (abbrev) safe_strcpy(car->car_abbrev, sizeof(car->car_abbrev), abbrev);

        const char *make = json_get_string(json_object_get(c, "make"));
        if (make) safe_strcpy(car->car_make, sizeof(car->car_make), make);

        const char *model = json_get_string(json_object_get(c, "model"));
        if (model) safe_strcpy(car->car_model, sizeof(car->car_model), model);

        car->hp = json_get_int(json_object_get(c, "hp"));
        car->weight_kg = json_get_int(json_object_get(c, "weight_kg"));
        car->price = (float)json_get_number(json_object_get(c, "price"));
        car->free_with_subscription = json_get_bool(json_object_get(c, "free"));
        car->retired = json_get_bool(json_object_get(c, "retired"));
        car->rain_enabled = json_get_bool(json_object_get(c, "rain_enabled"));
        car->ai_enabled = json_get_bool(json_object_get(c, "ai_enabled"));
        car->package_id = json_get_int(json_object_get(c, "package_id"));
        car->sku = json_get_int(json_object_get(c, "sku"));

        /* Parse categories array */
        json_value *cats = json_object_get(c, "categories");
        if (cats && json_get_type(cats) == JSON_ARRAY) {
            int cat_count = json_array_length(cats);
            if (cat_count > 4) cat_count = 4;
            car->category_count = cat_count;
            for (int j = 0; j < cat_count; j++) {
                json_value *cat = json_array_get(cats, j);
                if (json_get_type(cat) == JSON_STRING) {
                    car->categories[j] = string_to_category(json_get_string(cat));
                } else {
                    car->categories[j] = (race_category)json_get_int(cat);
                }
            }
        }
    }

    json_free(root);
    return true;
}

/*
 * Load owned content from JSON
 */
bool database_load_owned(ira_database *db, const char *filename)
{
    if (!db || !filename) return false;

    json_value *root = json_parse_file(filename);
    if (!root) return false;

    db->owned.cust_id = json_get_int(json_object_get(root, "cust_id"));

    json_value *updated = json_object_get(root, "last_updated");
    if (updated && json_get_type(updated) == JSON_STRING) {
        db->owned.last_updated = parse_timestamp(json_get_string(updated));
    }

    /* Load owned cars */
    json_value *cars = json_object_get(root, "owned_cars");
    if (cars && json_get_type(cars) == JSON_ARRAY) {
        int count = json_array_length(cars);
        if (count > 0) {
            db->owned.owned_car_ids = malloc(count * sizeof(int));
            if (db->owned.owned_car_ids) {
                db->owned.owned_car_count = count;
                for (int i = 0; i < count; i++) {
                    db->owned.owned_car_ids[i] = json_get_int(json_array_get(cars, i));
                }
            }
        }
    }

    /* Load owned tracks */
    json_value *tracks = json_object_get(root, "owned_tracks");
    if (tracks && json_get_type(tracks) == JSON_ARRAY) {
        int count = json_array_length(tracks);
        if (count > 0) {
            db->owned.owned_track_ids = malloc(count * sizeof(int));
            if (db->owned.owned_track_ids) {
                db->owned.owned_track_count = count;
                for (int i = 0; i < count; i++) {
                    db->owned.owned_track_ids[i] = json_get_int(json_array_get(tracks, i));
                }
            }
        }
    }

    json_free(root);
    return true;
}

/*
 * Load filter settings from JSON
 */
bool database_load_filter(ira_database *db, const char *filename)
{
    if (!db || !filename) return false;

    json_value *root = json_parse_file(filename);
    if (!root) return false;

    json_value *filters = json_object_get(root, "filters");
    if (!filters) {
        json_free(root);
        return false;
    }

    db->filter.owned_content_only = json_get_bool(json_object_get(filters, "owned_content_only"));
    db->filter.fixed_setup_only = json_get_bool(json_object_get(filters, "fixed_setup_only"));
    db->filter.open_setup_only = json_get_bool(json_object_get(filters, "open_setup_only"));
    db->filter.official_only = json_get_bool(json_object_get(filters, "official_only"));
    db->filter.min_race_mins = json_get_int(json_object_get(filters, "min_race_minutes"));
    db->filter.max_race_mins = json_get_int(json_object_get(filters, "max_race_minutes"));

    const char *min_lic = json_get_string(json_object_get(filters, "min_license"));
    if (min_lic) db->filter.min_license = string_to_license(min_lic);

    const char *max_lic = json_get_string(json_object_get(filters, "max_license"));
    if (max_lic) db->filter.max_license = string_to_license(max_lic);

    /* Parse categories */
    json_value *cats = json_object_get(filters, "categories");
    if (cats && json_get_type(cats) == JSON_ARRAY) {
        int count = json_array_length(cats);
        if (count > 6) count = 6;
        db->filter.category_count = count;
        for (int i = 0; i < count; i++) {
            const char *cat_str = json_get_string(json_array_get(cats, i));
            if (cat_str) {
                db->filter.categories[i] = string_to_category(cat_str);
            }
        }
    }

    /* Parse excluded series */
    json_value *excl_series = json_object_get(filters, "exclude_series");
    if (excl_series && json_get_type(excl_series) == JSON_ARRAY) {
        int count = json_array_length(excl_series);
        if (count > 0) {
            db->filter.excluded_series = malloc(count * sizeof(int));
            if (db->filter.excluded_series) {
                db->filter.excluded_series_count = count;
                for (int i = 0; i < count; i++) {
                    db->filter.excluded_series[i] = json_get_int(json_array_get(excl_series, i));
                }
            }
        }
    }

    json_free(root);
    return true;
}

/*
 * Load all data
 */
bool database_load_all(ira_database *db)
{
    if (!db) return false;

    init_paths();

    /* Load each file - failures are not fatal, just means no cached data */
    database_load_tracks(db, g_tracks_path);
    database_load_cars(db, g_cars_path);
    database_load_series(db, g_series_path);
    database_load_seasons(db, g_seasons_path);
    database_load_owned(db, g_owned_path);
    database_load_filter(db, g_filter_path);

    return true;
}

/*
 * Save filter settings to JSON
 */
bool database_save_filter(ira_database *db, const char *filename)
{
    if (!db || !filename) return false;

    json_value *root = json_new_object();
    json_value *filters = json_new_object();

    json_object_set(filters, "owned_content_only", json_new_bool(db->filter.owned_content_only));
    json_object_set(filters, "fixed_setup_only", json_new_bool(db->filter.fixed_setup_only));
    json_object_set(filters, "open_setup_only", json_new_bool(db->filter.open_setup_only));
    json_object_set(filters, "official_only", json_new_bool(db->filter.official_only));
    json_object_set(filters, "min_race_minutes", json_new_number(db->filter.min_race_mins));
    json_object_set(filters, "max_race_minutes", json_new_number(db->filter.max_race_mins));
    json_object_set(filters, "min_license", json_new_string(license_to_string(db->filter.min_license)));
    json_object_set(filters, "max_license", json_new_string(license_to_string(db->filter.max_license)));

    /* Categories */
    json_value *cats = json_new_array();
    for (int i = 0; i < db->filter.category_count; i++) {
        json_array_push(cats, json_new_string(category_to_string(db->filter.categories[i])));
    }
    json_object_set(filters, "categories", cats);

    /* Excluded series */
    json_value *excl = json_new_array();
    for (int i = 0; i < db->filter.excluded_series_count; i++) {
        json_array_push(excl, json_new_number(db->filter.excluded_series[i]));
    }
    json_object_set(filters, "exclude_series", excl);

    json_object_set(root, "filters", filters);

    bool result = json_write_file(root, filename, true);
    json_free(root);
    return result;
}

/*
 * Save all data
 */
bool database_save_all(ira_database *db)
{
    if (!db) return false;

    init_paths();

    /* Save filter settings */
    database_save_filter(db, g_filter_path);

    return true;
}

/*
 * Lookups
 */

ira_track *database_get_track(ira_database *db, int track_id)
{
    if (!db || !db->tracks) return NULL;

    for (int i = 0; i < db->track_count; i++) {
        if (db->tracks[i].track_id == track_id) {
            return &db->tracks[i];
        }
    }
    return NULL;
}

ira_car *database_get_car(ira_database *db, int car_id)
{
    if (!db || !db->cars) return NULL;

    for (int i = 0; i < db->car_count; i++) {
        if (db->cars[i].car_id == car_id) {
            return &db->cars[i];
        }
    }
    return NULL;
}

ira_car_class *database_get_car_class(ira_database *db, int car_class_id)
{
    if (!db || !db->car_classes) return NULL;

    for (int i = 0; i < db->car_class_count; i++) {
        if (db->car_classes[i].car_class_id == car_class_id) {
            return &db->car_classes[i];
        }
    }
    return NULL;
}

ira_series *database_get_series(ira_database *db, int series_id)
{
    if (!db || !db->series) return NULL;

    for (int i = 0; i < db->series_count; i++) {
        if (db->series[i].series_id == series_id) {
            return &db->series[i];
        }
    }
    return NULL;
}

ira_season *database_get_season(ira_database *db, int season_id)
{
    if (!db || !db->seasons) return NULL;

    for (int i = 0; i < db->season_count; i++) {
        if (db->seasons[i].season_id == season_id) {
            return &db->seasons[i];
        }
    }
    return NULL;
}

/*
 * Ownership checks
 */

bool database_owns_car(ira_database *db, int car_id)
{
    if (!db) return false;

    /* Check if free with subscription */
    ira_car *car = database_get_car(db, car_id);
    if (car && car->free_with_subscription) return true;

    /* Check owned list */
    for (int i = 0; i < db->owned.owned_car_count; i++) {
        if (db->owned.owned_car_ids[i] == car_id) return true;
    }
    return false;
}

bool database_owns_track(ira_database *db, int track_id)
{
    if (!db) return false;

    /* Check if free with subscription */
    ira_track *track = database_get_track(db, track_id);
    if (track && track->free_with_subscription) return true;

    /* Check owned list */
    for (int i = 0; i < db->owned.owned_track_count; i++) {
        if (db->owned.owned_track_ids[i] == track_id) return true;
    }
    return false;
}

/*
 * Data staleness checks
 */

bool database_tracks_stale(ira_database *db, int max_age_hours)
{
    if (!db || db->tracks_updated == 0) return true;

    time_t now = time(NULL);
    double hours = difftime(now, db->tracks_updated) / 3600.0;
    return hours > max_age_hours;
}

bool database_cars_stale(ira_database *db, int max_age_hours)
{
    if (!db || db->cars_updated == 0) return true;

    time_t now = time(NULL);
    double hours = difftime(now, db->cars_updated) / 3600.0;
    return hours > max_age_hours;
}

bool database_seasons_stale(ira_database *db, int max_age_hours)
{
    if (!db || db->seasons_updated == 0) return true;

    time_t now = time(NULL);
    double hours = difftime(now, db->seasons_updated) / 3600.0;
    return hours > max_age_hours;
}

/* Stub for car classes - not yet needed */
bool database_load_car_classes(ira_database *db, const char *filename) { (void)db; (void)filename; return false; }

/*
 * Load series from JSON
 */
bool database_load_series(ira_database *db, const char *filename)
{
    if (!db || !filename) return false;

    json_value *root = json_parse_file(filename);
    if (!root) return false;

    json_value *series_arr = json_object_get(root, "series");
    if (!series_arr || json_get_type(series_arr) != JSON_ARRAY) {
        json_free(root);
        return false;
    }

    int count = json_array_length(series_arr);
    if (count == 0) {
        json_free(root);
        return true;
    }

    db->series = calloc(count, sizeof(ira_series));
    if (!db->series) {
        json_free(root);
        return false;
    }
    db->series_count = count;

    for (int i = 0; i < count; i++) {
        json_value *s = json_array_get(series_arr, i);
        if (!s) continue;

        ira_series *series = &db->series[i];

        series->series_id = json_get_int(json_object_get(s, "series_id"));

        const char *name = json_get_string(json_object_get(s, "series_name"));
        if (name) safe_strcpy(series->series_name, sizeof(series->series_name), name);

        const char *short_name = json_get_string(json_object_get(s, "short_name"));
        if (short_name) safe_strcpy(series->short_name, sizeof(series->short_name), short_name);

        series->category = json_get_int(json_object_get(s, "category_id"));
        series->min_license = json_get_int(json_object_get(s, "min_license"));
        series->min_starters = json_get_int(json_object_get(s, "min_starters"));
        series->max_starters = json_get_int(json_object_get(s, "max_starters"));
    }

    json_free(root);
    return true;
}

/*
 * Load seasons from JSON
 */
bool database_load_seasons(ira_database *db, const char *filename)
{
    if (!db || !filename) return false;

    json_value *root = json_parse_file(filename);
    if (!root) return false;

    json_value *updated = json_object_get(root, "last_updated");
    if (updated && json_get_type(updated) == JSON_STRING) {
        db->seasons_updated = parse_timestamp(json_get_string(updated));
    }

    db->season_year = json_get_int(json_object_get(root, "year"));
    db->season_quarter = json_get_int(json_object_get(root, "quarter"));

    json_value *seasons_arr = json_object_get(root, "seasons");
    if (!seasons_arr || json_get_type(seasons_arr) != JSON_ARRAY) {
        json_free(root);
        return false;
    }

    int count = json_array_length(seasons_arr);
    if (count == 0) {
        json_free(root);
        return true;
    }

    db->seasons = calloc(count, sizeof(ira_season));
    if (!db->seasons) {
        json_free(root);
        return false;
    }
    db->season_count = count;

    for (int i = 0; i < count; i++) {
        json_value *s = json_array_get(seasons_arr, i);
        if (!s) continue;

        ira_season *season = &db->seasons[i];

        season->season_id = json_get_int(json_object_get(s, "season_id"));
        season->series_id = json_get_int(json_object_get(s, "series_id"));

        const char *name = json_get_string(json_object_get(s, "season_name"));
        if (name) safe_strcpy(season->season_name, sizeof(season->season_name), name);

        const char *short_name = json_get_string(json_object_get(s, "short_name"));
        if (short_name) safe_strcpy(season->short_name, sizeof(season->short_name), short_name);

        season->season_year = json_get_int(json_object_get(s, "season_year"));
        season->season_quarter = json_get_int(json_object_get(s, "season_quarter"));
        season->fixed_setup = json_get_bool(json_object_get(s, "fixed_setup"));
        season->official = json_get_bool(json_object_get(s, "official"));
        season->active = json_get_bool(json_object_get(s, "active"));
        season->complete = json_get_bool(json_object_get(s, "complete"));
        season->license_group = json_get_int(json_object_get(s, "license_group"));
        season->max_weeks = json_get_int(json_object_get(s, "max_weeks"));
        season->current_week = json_get_int(json_object_get(s, "current_week"));
        season->multiclass = json_get_bool(json_object_get(s, "multiclass"));
        season->has_supersessions = json_get_bool(json_object_get(s, "has_supersessions"));

        /* Parse schedule array */
        json_value *sched_arr = json_object_get(s, "schedule");
        if (sched_arr && json_get_type(sched_arr) == JSON_ARRAY) {
            int sched_count = json_array_length(sched_arr);
            if (sched_count > 0) {
                season->schedule = calloc(sched_count, sizeof(ira_schedule_week));
                if (season->schedule) {
                    season->schedule_count = sched_count;

                    for (int j = 0; j < sched_count; j++) {
                        json_value *w = json_array_get(sched_arr, j);
                        if (!w) continue;

                        ira_schedule_week *week = &season->schedule[j];

                        week->race_week_num = json_get_int(json_object_get(w, "week"));
                        week->track_id = json_get_int(json_object_get(w, "track_id"));

                        const char *track_name = json_get_string(json_object_get(w, "track_name"));
                        if (track_name) safe_strcpy(week->track_name, sizeof(week->track_name), track_name);

                        const char *config = json_get_string(json_object_get(w, "config_name"));
                        if (config) safe_strcpy(week->config_name, sizeof(week->config_name), config);

                        week->race_time_limit_mins = json_get_int(json_object_get(w, "race_time_limit_mins"));
                        week->race_lap_limit = json_get_int(json_object_get(w, "race_lap_limit"));
                        week->practice_mins = json_get_int(json_object_get(w, "practice_mins"));
                        week->qualify_mins = json_get_int(json_object_get(w, "qualify_mins"));
                        week->warmup_mins = json_get_int(json_object_get(w, "warmup_mins"));

                        /* Parse car_ids array */
                        json_value *cars = json_object_get(w, "car_ids");
                        if (cars && json_get_type(cars) == JSON_ARRAY) {
                            int car_count = json_array_length(cars);
                            if (car_count > 16) car_count = 16;
                            week->car_count = car_count;
                            for (int k = 0; k < car_count; k++) {
                                week->car_ids[k] = json_get_int(json_array_get(cars, k));
                            }
                        }
                    }
                }
            }
        }
    }

    json_free(root);
    return true;
}
/*
 * Helper: Format timestamp as ISO string
 */
static void format_timestamp(time_t t, char *buf, size_t buf_size)
{
    if (!buf || buf_size == 0) return;

    struct tm *tm = localtime(&t);
    if (tm) {
        snprintf(buf, buf_size, "%04d-%02d-%02dT%02d:%02d:%02d",
                 tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday,
                 tm->tm_hour, tm->tm_min, tm->tm_sec);
    } else {
        /* Fallback: use epoch time as string */
        snprintf(buf, buf_size, "%lld", (long long)t);
    }
}

/*
 * Save tracks to JSON
 */
bool database_save_tracks(ira_database *db, const char *filename)
{
    if (!db || !filename) return false;

    json_value *root = json_new_object();
    if (!root) return false;

    /* Add last_updated timestamp */
    char timestamp[32];
    format_timestamp(db->tracks_updated, timestamp, sizeof(timestamp));
    json_object_set(root, "last_updated", json_new_string(timestamp));

    /* Create tracks array */
    json_value *tracks_arr = json_new_array();
    if (!tracks_arr) {
        json_free(root);
        return false;
    }

    for (int i = 0; i < db->track_count; i++) {
        ira_track *track = &db->tracks[i];
        json_value *t = json_new_object();
        if (!t) continue;

        json_object_set(t, "track_id", json_new_number(track->track_id));
        json_object_set(t, "track_name", json_new_string(track->track_name));
        json_object_set(t, "config_name", json_new_string(track->config_name));
        json_object_set(t, "category_id", json_new_number(track->category));
        json_object_set(t, "is_oval", json_new_bool(track->is_oval));
        json_object_set(t, "is_dirt", json_new_bool(track->is_dirt));
        json_object_set(t, "length_km", json_new_number(track->length_km));
        json_object_set(t, "corners", json_new_number(track->corners));
        json_object_set(t, "max_cars", json_new_number(track->max_cars));
        json_object_set(t, "grid_stalls", json_new_number(track->grid_stalls));
        json_object_set(t, "pit_speed_kph", json_new_number(track->pit_speed_kph));
        json_object_set(t, "price", json_new_number(track->price));
        json_object_set(t, "free", json_new_bool(track->free_with_subscription));
        json_object_set(t, "retired", json_new_bool(track->retired));
        json_object_set(t, "package_id", json_new_number(track->package_id));
        json_object_set(t, "sku", json_new_number(track->sku));
        json_object_set(t, "location", json_new_string(track->location));
        json_object_set(t, "latitude", json_new_number(track->latitude));
        json_object_set(t, "longitude", json_new_number(track->longitude));
        json_object_set(t, "night_lighting", json_new_bool(track->night_lighting));
        json_object_set(t, "ai_enabled", json_new_bool(track->ai_enabled));

        json_array_push(tracks_arr, t);
    }

    json_object_set(root, "tracks", tracks_arr);

    bool result = json_write_file(root, filename, true);
    json_free(root);
    return result;
}

/*
 * Save cars to JSON
 */
bool database_save_cars(ira_database *db, const char *filename)
{
    if (!db || !filename) return false;

    json_value *root = json_new_object();
    if (!root) return false;

    char timestamp[32];
    format_timestamp(db->cars_updated, timestamp, sizeof(timestamp));
    json_object_set(root, "last_updated", json_new_string(timestamp));

    json_value *cars_arr = json_new_array();
    if (!cars_arr) {
        json_free(root);
        return false;
    }

    for (int i = 0; i < db->car_count; i++) {
        ira_car *car = &db->cars[i];
        json_value *c = json_new_object();
        if (!c) continue;

        json_object_set(c, "car_id", json_new_number(car->car_id));
        json_object_set(c, "car_name", json_new_string(car->car_name));
        json_object_set(c, "car_abbrev", json_new_string(car->car_abbrev));
        json_object_set(c, "make", json_new_string(car->car_make));
        json_object_set(c, "model", json_new_string(car->car_model));
        json_object_set(c, "hp", json_new_number(car->hp));
        json_object_set(c, "weight_kg", json_new_number(car->weight_kg));
        json_object_set(c, "price", json_new_number(car->price));
        json_object_set(c, "free", json_new_bool(car->free_with_subscription));
        json_object_set(c, "retired", json_new_bool(car->retired));
        json_object_set(c, "rain_enabled", json_new_bool(car->rain_enabled));
        json_object_set(c, "ai_enabled", json_new_bool(car->ai_enabled));
        json_object_set(c, "package_id", json_new_number(car->package_id));
        json_object_set(c, "sku", json_new_number(car->sku));

        /* Categories array */
        json_value *cats = json_new_array();
        for (int j = 0; j < car->category_count; j++) {
            json_array_push(cats, json_new_string(category_to_string(car->categories[j])));
        }
        json_object_set(c, "categories", cats);

        json_array_push(cars_arr, c);
    }

    json_object_set(root, "cars", cars_arr);

    bool result = json_write_file(root, filename, true);
    json_free(root);
    return result;
}

/*
 * Save car classes to JSON
 */
bool database_save_car_classes(ira_database *db, const char *filename)
{
    if (!db || !filename) return false;

    json_value *root = json_new_object();
    if (!root) return false;

    char timestamp[32];
    format_timestamp(db->car_classes_updated, timestamp, sizeof(timestamp));
    json_object_set(root, "last_updated", json_new_string(timestamp));

    json_value *classes_arr = json_new_array();
    if (!classes_arr) {
        json_free(root);
        return false;
    }

    for (int i = 0; i < db->car_class_count; i++) {
        ira_car_class *cc = &db->car_classes[i];
        json_value *c = json_new_object();
        if (!c) continue;

        json_object_set(c, "car_class_id", json_new_number(cc->car_class_id));
        json_object_set(c, "car_class_name", json_new_string(cc->car_class_name));
        json_object_set(c, "short_name", json_new_string(cc->short_name));

        json_value *cars = json_new_array();
        for (int j = 0; j < cc->car_count; j++) {
            json_array_push(cars, json_new_number(cc->car_ids[j]));
        }
        json_object_set(c, "car_ids", cars);

        json_array_push(classes_arr, c);
    }

    json_object_set(root, "car_classes", classes_arr);

    bool result = json_write_file(root, filename, true);
    json_free(root);
    return result;
}

/*
 * Save series to JSON
 */
bool database_save_series(ira_database *db, const char *filename)
{
    if (!db || !filename) return false;

    json_value *root = json_new_object();
    if (!root) return false;

    char timestamp[32];
    format_timestamp(db->series_updated, timestamp, sizeof(timestamp));
    json_object_set(root, "last_updated", json_new_string(timestamp));

    json_value *series_arr = json_new_array();
    if (!series_arr) {
        json_free(root);
        return false;
    }

    for (int i = 0; i < db->series_count; i++) {
        ira_series *series = &db->series[i];
        json_value *s = json_new_object();
        if (!s) continue;

        json_object_set(s, "series_id", json_new_number(series->series_id));
        json_object_set(s, "series_name", json_new_string(series->series_name));
        json_object_set(s, "short_name", json_new_string(series->short_name));
        json_object_set(s, "category_id", json_new_number(series->category));
        json_object_set(s, "min_license", json_new_number(series->min_license));
        json_object_set(s, "min_starters", json_new_number(series->min_starters));
        json_object_set(s, "max_starters", json_new_number(series->max_starters));

        json_array_push(series_arr, s);
    }

    json_object_set(root, "series", series_arr);

    bool result = json_write_file(root, filename, true);
    json_free(root);
    return result;
}

/*
 * Save seasons to JSON
 */
bool database_save_seasons(ira_database *db, const char *filename)
{
    if (!db || !filename) return false;

    json_value *root = json_new_object();
    if (!root) return false;

    char timestamp[32];
    format_timestamp(db->seasons_updated, timestamp, sizeof(timestamp));
    json_object_set(root, "last_updated", json_new_string(timestamp));
    json_object_set(root, "year", json_new_number(db->season_year));
    json_object_set(root, "quarter", json_new_number(db->season_quarter));

    json_value *seasons_arr = json_new_array();
    if (!seasons_arr) {
        json_free(root);
        return false;
    }

    for (int i = 0; i < db->season_count; i++) {
        ira_season *season = &db->seasons[i];
        json_value *s = json_new_object();
        if (!s) continue;

        json_object_set(s, "season_id", json_new_number(season->season_id));
        json_object_set(s, "series_id", json_new_number(season->series_id));
        json_object_set(s, "season_name", json_new_string(season->season_name));
        json_object_set(s, "short_name", json_new_string(season->short_name));
        json_object_set(s, "season_year", json_new_number(season->season_year));
        json_object_set(s, "season_quarter", json_new_number(season->season_quarter));
        json_object_set(s, "fixed_setup", json_new_bool(season->fixed_setup));
        json_object_set(s, "official", json_new_bool(season->official));
        json_object_set(s, "active", json_new_bool(season->active));
        json_object_set(s, "complete", json_new_bool(season->complete));
        json_object_set(s, "license_group", json_new_number(season->license_group));
        json_object_set(s, "max_weeks", json_new_number(season->max_weeks));
        json_object_set(s, "current_week", json_new_number(season->current_week));
        json_object_set(s, "multiclass", json_new_bool(season->multiclass));
        json_object_set(s, "has_supersessions", json_new_bool(season->has_supersessions));

        /* Schedule array */
        json_value *sched_arr = json_new_array();
        for (int j = 0; j < season->schedule_count; j++) {
            ira_schedule_week *week = &season->schedule[j];
            json_value *w = json_new_object();
            if (!w) continue;

            json_object_set(w, "week", json_new_number(week->race_week_num));
            json_object_set(w, "track_id", json_new_number(week->track_id));
            json_object_set(w, "track_name", json_new_string(week->track_name));
            json_object_set(w, "config_name", json_new_string(week->config_name));
            json_object_set(w, "race_time_limit_mins", json_new_number(week->race_time_limit_mins));
            json_object_set(w, "race_lap_limit", json_new_number(week->race_lap_limit));
            json_object_set(w, "practice_mins", json_new_number(week->practice_mins));
            json_object_set(w, "qualify_mins", json_new_number(week->qualify_mins));
            json_object_set(w, "warmup_mins", json_new_number(week->warmup_mins));

            json_value *cars = json_new_array();
            for (int k = 0; k < week->car_count; k++) {
                json_array_push(cars, json_new_number(week->car_ids[k]));
            }
            json_object_set(w, "car_ids", cars);

            json_array_push(sched_arr, w);
        }
        json_object_set(s, "schedule", sched_arr);

        json_array_push(seasons_arr, s);
    }

    json_object_set(root, "seasons", seasons_arr);

    bool result = json_write_file(root, filename, true);
    json_free(root);
    return result;
}

/*
 * Save owned content to JSON
 */
bool database_save_owned(ira_database *db, const char *filename)
{
    if (!db || !filename) return false;

    json_value *root = json_new_object();
    if (!root) return false;

    json_object_set(root, "cust_id", json_new_number(db->owned.cust_id));

    char timestamp[32];
    format_timestamp(db->owned.last_updated, timestamp, sizeof(timestamp));
    json_object_set(root, "last_updated", json_new_string(timestamp));

    /* Owned cars */
    json_value *cars = json_new_array();
    for (int i = 0; i < db->owned.owned_car_count; i++) {
        json_array_push(cars, json_new_number(db->owned.owned_car_ids[i]));
    }
    json_object_set(root, "owned_cars", cars);

    /* Owned tracks */
    json_value *tracks = json_new_array();
    for (int i = 0; i < db->owned.owned_track_count; i++) {
        json_array_push(tracks, json_new_number(db->owned.owned_track_ids[i]));
    }
    json_object_set(root, "owned_tracks", tracks);

    bool result = json_write_file(root, filename, true);
    json_free(root);
    return result;
}

/*
 * Check if user owns all content for a season's current week
 */
bool database_owns_season_content(ira_database *db, ira_season *season)
{
    if (!db || !season) return false;

    /* Get current week's schedule */
    if (season->current_week < 0 || season->current_week >= season->schedule_count) {
        return false;
    }

    ira_schedule_week *week = &season->schedule[season->current_week];

    /* Check track ownership */
    if (!database_owns_track(db, week->track_id)) {
        return false;
    }

    /* Check car ownership - need at least one owned car */
    bool has_car = false;
    for (int i = 0; i < week->car_count; i++) {
        if (database_owns_car(db, week->car_ids[i])) {
            has_car = true;
            break;
        }
    }

    return has_car;
}
