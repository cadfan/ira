/*
 * ira - iRacing Application
 * Race Filter Implementation
 *
 * Copyright (c) 2026 Christopher Griffiths
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>

#include "race_filter.h"

#define INITIAL_CAPACITY 64

/*
 * Lifecycle
 */

filter_results *filter_results_create(void)
{
    filter_results *results = calloc(1, sizeof(filter_results));
    if (!results) return NULL;

    results->races = calloc(INITIAL_CAPACITY, sizeof(filtered_race));
    if (!results->races) {
        free(results);
        return NULL;
    }

    results->races_capacity = INITIAL_CAPACITY;
    return results;
}

void filter_results_destroy(filter_results *results)
{
    if (!results) return;

    if (results->races) {
        free(results->races);
    }
    free(results);
}

void filter_results_clear(filter_results *results)
{
    if (!results) return;

    results->race_count = 0;
    results->total_checked = 0;
    results->passed_count = 0;
    results->failed_ownership = 0;
    results->failed_category = 0;
    results->failed_license = 0;
    results->failed_other = 0;
}

/*
 * Internal: ensure capacity for more races
 */
static bool ensure_capacity(filter_results *results, int needed)
{
    if (results->race_count + needed <= results->races_capacity) {
        return true;
    }

    int new_cap = results->races_capacity * 2;
    while (new_cap < results->race_count + needed) {
        new_cap *= 2;
    }

    filtered_race *new_races = realloc(results->races, new_cap * sizeof(filtered_race));
    if (!new_races) return false;

    results->races = new_races;
    results->races_capacity = new_cap;
    return true;
}

/*
 * Internal: add a race to results
 */
static bool add_race(filter_results *results, filtered_race *race)
{
    if (!ensure_capacity(results, 1)) return false;

    results->races[results->race_count++] = *race;
    return true;
}

/*
 * Utility: check if category is in filter
 */
bool filter_has_category(ira_filter *filter, race_category cat)
{
    if (!filter || filter->category_count == 0) {
        /* No categories specified = all categories allowed */
        return true;
    }

    for (int i = 0; i < filter->category_count; i++) {
        if (filter->categories[i] == cat) return true;

        /* Handle legacy road -> sports_car/formula mapping */
        if (filter->categories[i] == CATEGORY_ROAD) {
            if (cat == CATEGORY_SPORTS_CAR || cat == CATEGORY_FORMULA) {
                return true;
            }
        }
    }

    return false;
}

/*
 * Utility: check if series is excluded
 */
bool filter_series_excluded(ira_filter *filter, int series_id)
{
    if (!filter || !filter->excluded_series) return false;

    for (int i = 0; i < filter->excluded_series_count; i++) {
        if (filter->excluded_series[i] == series_id) return true;
    }
    return false;
}

/*
 * Utility: check if track is excluded
 */
bool filter_track_excluded(ira_filter *filter, int track_id)
{
    if (!filter || !filter->excluded_tracks) return false;

    for (int i = 0; i < filter->excluded_track_count; i++) {
        if (filter->excluded_tracks[i] == track_id) return true;
    }
    return false;
}

/*
 * Check ownership of any car in the week's car list
 */
static bool owns_any_car(ira_database *db, ira_schedule_week *week)
{
    if (!db || !week) return false;

    /* If no cars specified, assume free content */
    if (week->car_count == 0) return true;

    for (int i = 0; i < week->car_count; i++) {
        if (database_owns_car(db, week->car_ids[i])) {
            return true;
        }
    }
    return false;
}

/*
 * Check a single race week against filter criteria
 */
filter_match_flags filter_check_week(
    ira_database *db,
    ira_season *season,
    ira_schedule_week *week)
{
    if (!db || !season || !week) return MATCH_RETIRED;

    ira_filter *filter = &db->filter;
    filter_match_flags flags = MATCH_OK;

    /* Get related data */
    ira_series *series = database_get_series(db, season->series_id);
    ira_track *track = database_get_track(db, week->track_id);

    /* Check if content is retired */
    if (track && track->retired) {
        flags |= MATCH_RETIRED;
    }

    /* Check series exclusion */
    if (filter_series_excluded(filter, season->series_id)) {
        flags |= MATCH_SERIES_EXCLUDED;
    }

    /* Check track exclusion */
    if (filter_track_excluded(filter, week->track_id)) {
        flags |= MATCH_TRACK_EXCLUDED;
    }

    /* Check category */
    race_category cat = CATEGORY_UNKNOWN;
    if (series) {
        cat = series->category;
    } else if (track) {
        cat = track->category;
    }

    if (!filter_has_category(filter, cat)) {
        flags |= MATCH_WRONG_CATEGORY;
    }

    /* Check license level */
    if (series) {
        if (series->min_license < filter->min_license ||
            series->min_license > filter->max_license) {
            flags |= MATCH_WRONG_LICENSE;
        }
    } else if (season->license_group != 0) {
        if (season->license_group < (int)filter->min_license ||
            season->license_group > (int)filter->max_license) {
            flags |= MATCH_WRONG_LICENSE;
        }
    }

    /* Check setup type */
    if (filter->fixed_setup_only && !season->fixed_setup) {
        flags |= MATCH_WRONG_SETUP;
    }
    if (filter->open_setup_only && season->fixed_setup) {
        flags |= MATCH_WRONG_SETUP;
    }

    /* Check official status */
    if (filter->official_only && !season->official) {
        flags |= MATCH_NOT_OFFICIAL;
    }

    /* Check race duration */
    int duration = week->race_time_limit_mins;
    if (duration == 0 && week->race_lap_limit > 0) {
        /* Estimate lap-based race duration (rough estimate) */
        duration = week->race_lap_limit * 2;  /* ~2 min per lap average */
    }

    if (filter->min_race_mins > 0 && duration > 0 && duration < filter->min_race_mins) {
        flags |= MATCH_TOO_SHORT;
    }
    if (filter->max_race_mins > 0 && duration > 0 && duration > filter->max_race_mins) {
        flags |= MATCH_TOO_LONG;
    }

    /* Check ownership (only if filter requires it) */
    if (filter->owned_content_only) {
        if (!database_owns_track(db, week->track_id)) {
            flags |= MATCH_NO_TRACK;
        }
        if (!owns_any_car(db, week)) {
            flags |= MATCH_NO_CAR;
        }
    }

    return flags;
}

/*
 * Apply filter to a single season
 */
bool filter_season(ira_database *db, ira_season *season, filter_results *results)
{
    if (!db || !season || !results) return false;

    /* Only filter the current week (or all weeks if desired) */
    int week_idx = season->current_week;
    if (week_idx < 0 || week_idx >= season->schedule_count) {
        return true;  /* No current week data */
    }

    ira_schedule_week *week = &season->schedule[week_idx];
    results->total_checked++;

    filter_match_flags match = filter_check_week(db, season, week);

    /* Create filtered race entry */
    filtered_race race = {0};
    race.season = season;
    race.week = week;
    race.series = database_get_series(db, season->series_id);
    race.track = database_get_track(db, week->track_id);
    race.match = match;
    race.owns_car = owns_any_car(db, week);
    race.owns_track = database_owns_track(db, week->track_id);

    /* Calculate duration */
    if (week->race_time_limit_mins > 0) {
        race.race_duration_mins = week->race_time_limit_mins;
    } else if (week->race_lap_limit > 0) {
        race.race_duration_mins = week->race_lap_limit * 2;  /* Estimate */
    }

    /* Calculate next race time */
    race.next_race_time = filter_next_race_time(season, week);

    /* Update statistics */
    if (match == MATCH_OK) {
        results->passed_count++;
    } else {
        if (match & (MATCH_NO_CAR | MATCH_NO_TRACK)) {
            results->failed_ownership++;
        } else if (match & MATCH_WRONG_CATEGORY) {
            results->failed_category++;
        } else if (match & MATCH_WRONG_LICENSE) {
            results->failed_license++;
        } else {
            results->failed_other++;
        }
    }

    /* Add to results (include failed ones for "show all" mode) */
    add_race(results, &race);

    return true;
}

/*
 * Apply filter to all seasons
 */
bool filter_apply(ira_database *db, filter_results *results)
{
    if (!db || !results) return false;

    filter_results_clear(results);

    for (int i = 0; i < db->season_count; i++) {
        ira_season *season = &db->seasons[i];

        /* Skip inactive/complete seasons */
        if (!season->active || season->complete) continue;

        filter_season(db, season, results);
    }

    return true;
}

/*
 * Comparison functions for sorting
 */

static int cmp_by_start_time(const void *a, const void *b)
{
    const filtered_race *ra = (const filtered_race *)a;
    const filtered_race *rb = (const filtered_race *)b;

    if (ra->next_race_time < rb->next_race_time) return -1;
    if (ra->next_race_time > rb->next_race_time) return 1;
    return 0;
}

static int cmp_by_series_name(const void *a, const void *b)
{
    const filtered_race *ra = (const filtered_race *)a;
    const filtered_race *rb = (const filtered_race *)b;

    const char *name_a = ra->series ? ra->series->series_name : "";
    const char *name_b = rb->series ? rb->series->series_name : "";

    return strcmp(name_a, name_b);
}

static int cmp_by_category(const void *a, const void *b)
{
    const filtered_race *ra = (const filtered_race *)a;
    const filtered_race *rb = (const filtered_race *)b;

    race_category cat_a = ra->series ? ra->series->category : CATEGORY_UNKNOWN;
    race_category cat_b = rb->series ? rb->series->category : CATEGORY_UNKNOWN;

    if (cat_a < cat_b) return -1;
    if (cat_a > cat_b) return 1;
    return 0;
}

static int cmp_by_license(const void *a, const void *b)
{
    const filtered_race *ra = (const filtered_race *)a;
    const filtered_race *rb = (const filtered_race *)b;

    license_level lic_a = ra->series ? ra->series->min_license : LICENSE_ROOKIE;
    license_level lic_b = rb->series ? rb->series->min_license : LICENSE_ROOKIE;

    if (lic_a < lic_b) return -1;
    if (lic_a > lic_b) return 1;
    return 0;
}

static int cmp_by_duration(const void *a, const void *b)
{
    const filtered_race *ra = (const filtered_race *)a;
    const filtered_race *rb = (const filtered_race *)b;

    if (ra->race_duration_mins < rb->race_duration_mins) return -1;
    if (ra->race_duration_mins > rb->race_duration_mins) return 1;
    return 0;
}

/*
 * Sort filter results
 */
void filter_results_sort(filter_results *results, race_sort_order order, bool ascending)
{
    if (!results || results->race_count < 2) return;

    int (*cmp_func)(const void *, const void *) = NULL;

    switch (order) {
        case SORT_BY_START_TIME:  cmp_func = cmp_by_start_time; break;
        case SORT_BY_SERIES_NAME: cmp_func = cmp_by_series_name; break;
        case SORT_BY_CATEGORY:    cmp_func = cmp_by_category; break;
        case SORT_BY_LICENSE:     cmp_func = cmp_by_license; break;
        case SORT_BY_DURATION:    cmp_func = cmp_by_duration; break;
        case SORT_BY_POPULARITY:  cmp_func = cmp_by_start_time; break;  /* Fallback */
        default:                  cmp_func = cmp_by_start_time; break;
    }

    qsort(results->races, results->race_count, sizeof(filtered_race), cmp_func);

    /* Reverse if descending */
    if (!ascending) {
        for (int i = 0; i < results->race_count / 2; i++) {
            int j = results->race_count - 1 - i;
            filtered_race tmp = results->races[i];
            results->races[i] = results->races[j];
            results->races[j] = tmp;
        }
    }
}

/*
 * Get human-readable filter failure reason
 */
const char *filter_match_to_string(filter_match_flags flags)
{
    if (flags == MATCH_OK) return "OK";
    if (flags & MATCH_NO_CAR) return "Missing car";
    if (flags & MATCH_NO_TRACK) return "Missing track";
    if (flags & MATCH_WRONG_CATEGORY) return "Wrong category";
    if (flags & MATCH_WRONG_LICENSE) return "License mismatch";
    if (flags & MATCH_WRONG_SETUP) return "Setup type mismatch";
    if (flags & MATCH_NOT_OFFICIAL) return "Unofficial";
    if (flags & MATCH_TOO_SHORT) return "Too short";
    if (flags & MATCH_TOO_LONG) return "Too long";
    if (flags & MATCH_SERIES_EXCLUDED) return "Series excluded";
    if (flags & MATCH_TRACK_EXCLUDED) return "Track excluded";
    if (flags & MATCH_RETIRED) return "Retired content";
    return "Filtered";
}

/*
 * Calculate next race start time
 * TODO: This needs actual schedule data from the API
 */
time_t filter_next_race_time(ira_season *season, ira_schedule_week *week)
{
    (void)season;
    (void)week;

    /* For now, return current time as placeholder */
    /* Real implementation would calculate based on:
     * - Race start interval (e.g., every 2 hours)
     * - Current time
     * - Week start/end dates
     */
    return time(NULL);
}

/*
 * Format race duration
 */
void filter_format_duration(ira_schedule_week *week, char *buf, size_t buf_size)
{
    if (!week || !buf || buf_size == 0) return;

    if (week->race_time_limit_mins > 0) {
        if (week->race_time_limit_mins >= 60) {
            int hours = week->race_time_limit_mins / 60;
            int mins = week->race_time_limit_mins % 60;
            if (mins > 0) {
                snprintf(buf, buf_size, "%dh %dm", hours, mins);
            } else {
                snprintf(buf, buf_size, "%dh", hours);
            }
        } else {
            snprintf(buf, buf_size, "%d min", week->race_time_limit_mins);
        }
    } else if (week->race_lap_limit > 0) {
        snprintf(buf, buf_size, "%d laps", week->race_lap_limit);
    } else {
        snprintf(buf, buf_size, "Unknown");
    }
}

/*
 * Format time until race
 */
void filter_format_time_until(time_t race_time, char *buf, size_t buf_size)
{
    if (!buf || buf_size == 0) return;

    time_t now = time(NULL);
    double diff = difftime(race_time, now);

    if (diff < 0) {
        snprintf(buf, buf_size, "Started");
        return;
    }

    int total_mins = (int)(diff / 60);
    int hours = total_mins / 60;
    int mins = total_mins % 60;

    if (hours > 0) {
        snprintf(buf, buf_size, "in %dh %dm", hours, mins);
    } else if (mins > 0) {
        snprintf(buf, buf_size, "in %d min", mins);
    } else {
        snprintf(buf, buf_size, "Starting now");
    }
}
