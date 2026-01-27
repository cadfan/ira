/*
 * ira - iRacing Application
 * Race Filter - Filter and sort races based on user criteria
 *
 * Copyright (c) 2026 Christopher Griffiths
 */

#ifndef IRA_RACE_FILTER_H
#define IRA_RACE_FILTER_H

#include <stdbool.h>
#include <time.h>

#include "../data/models.h"
#include "../data/database.h"

/*
 * Sort options for race results
 */
typedef enum {
    SORT_BY_START_TIME,     /* Soonest first */
    SORT_BY_SERIES_NAME,    /* Alphabetical */
    SORT_BY_CATEGORY,       /* Group by category */
    SORT_BY_LICENSE,        /* Rookie first or Pro first */
    SORT_BY_DURATION,       /* Shortest first */
    SORT_BY_POPULARITY      /* Most participants first (if available) */
} race_sort_order;

/*
 * Filter match result - why a race was included/excluded
 */
typedef enum {
    MATCH_OK              = 0,
    MATCH_NO_CAR          = (1 << 0),  /* Don't own required car */
    MATCH_NO_TRACK        = (1 << 1),  /* Don't own required track */
    MATCH_WRONG_CATEGORY  = (1 << 2),  /* Category not in filter */
    MATCH_WRONG_LICENSE   = (1 << 3),  /* License level mismatch */
    MATCH_WRONG_SETUP     = (1 << 4),  /* Fixed/open mismatch */
    MATCH_NOT_OFFICIAL    = (1 << 5),  /* Unofficial when official required */
    MATCH_TOO_SHORT       = (1 << 6),  /* Race too short */
    MATCH_TOO_LONG        = (1 << 7),  /* Race too long */
    MATCH_SERIES_EXCLUDED = (1 << 8),  /* Series in exclude list */
    MATCH_TRACK_EXCLUDED  = (1 << 9),  /* Track in exclude list */
    MATCH_RETIRED         = (1 << 10)  /* Content is retired */
} filter_match_flags;

/*
 * Filtered race entry - a race that passed (or failed) filtering
 */
typedef struct {
    /* Source data */
    ira_season *season;
    ira_schedule_week *week;
    ira_series *series;
    ira_track *track;

    /* Computed fields */
    time_t next_race_time;      /* When the next session starts */
    int race_duration_mins;     /* Total race length */
    int registered_count;       /* Number registered (if known) */
    int sof_estimate;           /* Estimated SOF (if known) */

    /* Filter result */
    filter_match_flags match;   /* 0 = passed, flags = why excluded */
    bool owns_car;
    bool owns_track;
} filtered_race;

/*
 * Filter results container
 */
typedef struct {
    filtered_race *races;
    int race_count;
    int races_capacity;

    /* Statistics */
    int total_checked;
    int passed_count;
    int failed_ownership;
    int failed_category;
    int failed_license;
    int failed_other;
} filter_results;

/*
 * Lifecycle
 */

/* Create empty filter results */
filter_results *filter_results_create(void);

/* Free filter results */
void filter_results_destroy(filter_results *results);

/* Clear results for reuse */
void filter_results_clear(filter_results *results);

/*
 * Filtering
 */

/* Apply filter to all seasons in database, populates results */
bool filter_apply(ira_database *db, filter_results *results);

/* Apply filter to a single season */
bool filter_season(ira_database *db, ira_season *season, filter_results *results);

/* Check if a single race week passes the filter */
filter_match_flags filter_check_week(
    ira_database *db,
    ira_season *season,
    ira_schedule_week *week
);

/*
 * Sorting
 */

/* Sort filter results */
void filter_results_sort(filter_results *results, race_sort_order order, bool ascending);

/*
 * Utilities
 */

/* Get human-readable reason for filter failure */
const char *filter_match_to_string(filter_match_flags flags);

/* Check if a category is enabled in the filter */
bool filter_has_category(ira_filter *filter, race_category cat);

/* Check if a series is excluded */
bool filter_series_excluded(ira_filter *filter, int series_id);

/* Check if a track is excluded */
bool filter_track_excluded(ira_filter *filter, int track_id);

/* Calculate next race start time for a series (based on schedule intervals) */
time_t filter_next_race_time(ira_season *season, ira_schedule_week *week);

/*
 * Display helpers
 */

/* Format race duration as string (e.g., "45 min" or "60 laps") */
void filter_format_duration(ira_schedule_week *week, char *buf, size_t buf_size);

/* Format time until race (e.g., "in 23 min", "in 2h 15m") */
void filter_format_time_until(time_t race_time, char *buf, size_t buf_size);

#endif /* IRA_RACE_FILTER_H */
