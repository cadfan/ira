/*
 * ira - iRacing Application
 * Database - JSON data storage and retrieval
 *
 * Copyright (c) 2026 Christopher Griffiths
 */

#ifndef IRA_DATABASE_H
#define IRA_DATABASE_H

#include <stdbool.h>
#include <time.h>

#include "models.h"

/*
 * Database structure - holds all cached iRacing data
 */
typedef struct {
    /* Tracks */
    ira_track *tracks;
    int track_count;
    time_t tracks_updated;

    /* Cars */
    ira_car *cars;
    int car_count;
    time_t cars_updated;

    /* Car Classes */
    ira_car_class *car_classes;
    int car_class_count;
    time_t car_classes_updated;

    /* Series */
    ira_series *series;
    int series_count;
    time_t series_updated;

    /* Seasons (current quarter) */
    ira_season *seasons;
    int season_count;
    int season_year;
    int season_quarter;
    time_t seasons_updated;

    /* User's owned content */
    ira_owned_content owned;

    /* User's filter preferences */
    ira_filter filter;
} ira_database;

/*
 * Lifecycle
 */

/* Create a new empty database */
ira_database *database_create(void);

/* Free all database memory */
void database_destroy(ira_database *db);

/*
 * Persistence - Load/Save to JSON files
 * All files stored in same directory as executable
 */

/* Load all data from JSON files */
bool database_load_all(ira_database *db);

/* Save all data to JSON files */
bool database_save_all(ira_database *db);

/* Individual load functions */
bool database_load_tracks(ira_database *db, const char *filename);
bool database_load_cars(ira_database *db, const char *filename);
bool database_load_car_classes(ira_database *db, const char *filename);
bool database_load_series(ira_database *db, const char *filename);
bool database_load_seasons(ira_database *db, const char *filename);
bool database_load_owned(ira_database *db, const char *filename);
bool database_load_filter(ira_database *db, const char *filename);

/* Individual save functions */
bool database_save_tracks(ira_database *db, const char *filename);
bool database_save_cars(ira_database *db, const char *filename);
bool database_save_car_classes(ira_database *db, const char *filename);
bool database_save_series(ira_database *db, const char *filename);
bool database_save_seasons(ira_database *db, const char *filename);
bool database_save_owned(ira_database *db, const char *filename);
bool database_save_filter(ira_database *db, const char *filename);

/*
 * Lookups
 */

/* Find track by ID, returns NULL if not found */
ira_track *database_get_track(ira_database *db, int track_id);

/* Find car by ID, returns NULL if not found */
ira_car *database_get_car(ira_database *db, int car_id);

/* Find car class by ID, returns NULL if not found */
ira_car_class *database_get_car_class(ira_database *db, int car_class_id);

/* Find series by ID, returns NULL if not found */
ira_series *database_get_series(ira_database *db, int series_id);

/* Find season by ID, returns NULL if not found */
ira_season *database_get_season(ira_database *db, int season_id);

/*
 * Ownership checks
 */

/* Check if user owns a specific car */
bool database_owns_car(ira_database *db, int car_id);

/* Check if user owns a specific track */
bool database_owns_track(ira_database *db, int track_id);

/* Check if user owns all required content for a season's current week */
bool database_owns_season_content(ira_database *db, ira_season *season);

/*
 * Data age checks
 */

/* Check if tracks data needs refresh (older than max_age_hours) */
bool database_tracks_stale(ira_database *db, int max_age_hours);

/* Check if cars data needs refresh */
bool database_cars_stale(ira_database *db, int max_age_hours);

/* Check if seasons data needs refresh */
bool database_seasons_stale(ira_database *db, int max_age_hours);

/*
 * Default file paths (relative to executable)
 */

const char *database_get_tracks_path(void);
const char *database_get_cars_path(void);
const char *database_get_car_classes_path(void);
const char *database_get_series_path(void);
const char *database_get_seasons_path(void);
const char *database_get_owned_path(void);
const char *database_get_filter_path(void);

#endif /* IRA_DATABASE_H */
