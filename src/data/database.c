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

/* Stub implementations for save functions not yet needed */
bool database_load_car_classes(ira_database *db, const char *filename) { (void)db; (void)filename; return false; }
bool database_load_series(ira_database *db, const char *filename) { (void)db; (void)filename; return false; }
bool database_load_seasons(ira_database *db, const char *filename) { (void)db; (void)filename; return false; }
bool database_save_tracks(ira_database *db, const char *filename) { (void)db; (void)filename; return false; }
bool database_save_cars(ira_database *db, const char *filename) { (void)db; (void)filename; return false; }
bool database_save_car_classes(ira_database *db, const char *filename) { (void)db; (void)filename; return false; }
bool database_save_series(ira_database *db, const char *filename) { (void)db; (void)filename; return false; }
bool database_save_seasons(ira_database *db, const char *filename) { (void)db; (void)filename; return false; }
bool database_save_owned(ira_database *db, const char *filename) { (void)db; (void)filename; return false; }
bool database_owns_season_content(ira_database *db, ira_season *season) { (void)db; (void)season; return false; }
