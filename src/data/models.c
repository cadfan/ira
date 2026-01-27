/*
 * ira - iRacing Application
 * Data Models Implementation
 *
 * Copyright (c) 2026 Christopher Griffiths
 */

#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "models.h"

/*
 * Category conversion
 */

const char *category_to_string(race_category cat)
{
    switch (cat) {
        case CATEGORY_OVAL:       return "oval";
        case CATEGORY_ROAD:       return "road";
        case CATEGORY_DIRT_OVAL:  return "dirt_oval";
        case CATEGORY_DIRT_ROAD:  return "dirt_road";
        case CATEGORY_SPORTS_CAR: return "sports_car";
        case CATEGORY_FORMULA:    return "formula";
        default:                  return "unknown";
    }
}

race_category string_to_category(const char *str)
{
    if (!str) return CATEGORY_UNKNOWN;

    /* Convert to lowercase for comparison */
    char lower[32];
    size_t i;
    for (i = 0; i < sizeof(lower) - 1 && str[i]; i++) {
        lower[i] = (char)tolower((unsigned char)str[i]);
    }
    lower[i] = '\0';

    if (strcmp(lower, "oval") == 0)        return CATEGORY_OVAL;
    if (strcmp(lower, "road") == 0)        return CATEGORY_ROAD;
    if (strcmp(lower, "dirt_oval") == 0)   return CATEGORY_DIRT_OVAL;
    if (strcmp(lower, "dirt oval") == 0)   return CATEGORY_DIRT_OVAL;
    if (strcmp(lower, "dirt_road") == 0)   return CATEGORY_DIRT_ROAD;
    if (strcmp(lower, "dirt road") == 0)   return CATEGORY_DIRT_ROAD;
    if (strcmp(lower, "sports_car") == 0)  return CATEGORY_SPORTS_CAR;
    if (strcmp(lower, "sports car") == 0)  return CATEGORY_SPORTS_CAR;
    if (strcmp(lower, "sportscar") == 0)   return CATEGORY_SPORTS_CAR;
    if (strcmp(lower, "formula") == 0)     return CATEGORY_FORMULA;
    if (strcmp(lower, "formula_car") == 0) return CATEGORY_FORMULA;
    if (strcmp(lower, "formula car") == 0) return CATEGORY_FORMULA;

    return CATEGORY_UNKNOWN;
}

bool category_is_active(race_category cat)
{
    switch (cat) {
        case CATEGORY_OVAL:
        case CATEGORY_DIRT_OVAL:
        case CATEGORY_DIRT_ROAD:
        case CATEGORY_SPORTS_CAR:
        case CATEGORY_FORMULA:
            return true;
        default:
            return false;
    }
}

/*
 * License conversion
 */

const char *license_to_string(license_level lic)
{
    switch (lic) {
        case LICENSE_ROOKIE: return "R";
        case LICENSE_D:      return "D";
        case LICENSE_C:      return "C";
        case LICENSE_B:      return "B";
        case LICENSE_A:      return "A";
        case LICENSE_PRO:    return "Pro";
        case LICENSE_PRO_WC: return "Pro/WC";
        default:             return "?";
    }
}

license_level string_to_license(const char *str)
{
    if (!str) return LICENSE_ROOKIE;

    /* Handle single character */
    if (str[1] == '\0' || str[1] == ' ') {
        switch (toupper((unsigned char)str[0])) {
            case 'R': return LICENSE_ROOKIE;
            case 'D': return LICENSE_D;
            case 'C': return LICENSE_C;
            case 'B': return LICENSE_B;
            case 'A': return LICENSE_A;
            case 'P': return LICENSE_PRO;
        }
    }

    /* Handle full names */
    char lower[32];
    size_t i;
    for (i = 0; i < sizeof(lower) - 1 && str[i]; i++) {
        lower[i] = (char)tolower((unsigned char)str[i]);
    }
    lower[i] = '\0';

    if (strcmp(lower, "rookie") == 0)  return LICENSE_ROOKIE;
    if (strcmp(lower, "pro") == 0)     return LICENSE_PRO;
    if (strcmp(lower, "pro/wc") == 0)  return LICENSE_PRO_WC;
    if (strcmp(lower, "prowc") == 0)   return LICENSE_PRO_WC;

    return LICENSE_ROOKIE;
}

/*
 * Memory management
 */

void season_free_schedule(ira_season *season)
{
    if (!season) return;

    if (season->schedule) {
        free(season->schedule);
        season->schedule = NULL;
        season->schedule_count = 0;
    }
}

void owned_content_free(ira_owned_content *content)
{
    if (!content) return;

    if (content->owned_car_ids) {
        free(content->owned_car_ids);
        content->owned_car_ids = NULL;
        content->owned_car_count = 0;
    }

    if (content->owned_track_ids) {
        free(content->owned_track_ids);
        content->owned_track_ids = NULL;
        content->owned_track_count = 0;
    }
}

void filter_free(ira_filter *filter)
{
    if (!filter) return;

    if (filter->excluded_series) {
        free(filter->excluded_series);
        filter->excluded_series = NULL;
        filter->excluded_series_count = 0;
    }

    if (filter->excluded_tracks) {
        free(filter->excluded_tracks);
        filter->excluded_tracks = NULL;
        filter->excluded_track_count = 0;
    }
}
