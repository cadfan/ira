/*
 * ira - iRacing Application
 * Data Models for iRacing API
 *
 * Copyright (c) 2026 Christopher Griffiths
 */

#ifndef IRA_MODELS_H
#define IRA_MODELS_H

#include <stdbool.h>
#include <stdint.h>
#include <time.h>

/*
 * Racing Categories
 * Note: CATEGORY_ROAD (2) is legacy - replaced by SPORTS_CAR and FORMULA
 */
typedef enum {
    CATEGORY_UNKNOWN    = 0,
    CATEGORY_OVAL       = 1,
    CATEGORY_ROAD       = 2,  /* Legacy - may map to sports_car */
    CATEGORY_DIRT_OVAL  = 3,
    CATEGORY_DIRT_ROAD  = 4,
    CATEGORY_SPORTS_CAR = 5,
    CATEGORY_FORMULA    = 6
} race_category;

/*
 * License Levels
 */
typedef enum {
    LICENSE_ROOKIE = 1,
    LICENSE_D      = 2,
    LICENSE_C      = 3,
    LICENSE_B      = 4,
    LICENSE_A      = 5,
    LICENSE_PRO    = 6,
    LICENSE_PRO_WC = 7
} license_level;

/*
 * Track Configuration
 */
typedef struct {
    int track_id;
    char track_name[128];
    char config_name[64];
    race_category category;
    bool is_oval;
    bool is_dirt;
    float length_km;
    int corners;
    int max_cars;
    int grid_stalls;
    int pit_speed_kph;
    float price;
    bool free_with_subscription;
    bool retired;
    int package_id;
    int sku;
    /* Location */
    char location[128];
    float latitude;
    float longitude;
    /* Features */
    bool night_lighting;
    bool ai_enabled;
} ira_track;

/*
 * Car
 */
typedef struct {
    int car_id;
    char car_name[128];
    char car_abbrev[32];
    char car_make[64];
    char car_model[64];
    int hp;
    int weight_kg;
    race_category categories[4];  /* A car can be in multiple categories */
    int category_count;
    float price;
    bool free_with_subscription;
    bool retired;
    bool rain_enabled;
    bool ai_enabled;
    int package_id;
    int sku;
} ira_car;

/*
 * Car Class (group of cars)
 */
typedef struct {
    int car_class_id;
    char car_class_name[64];
    char short_name[32];
    int car_ids[32];
    int car_count;
} ira_car_class;

/*
 * Series
 */
typedef struct {
    int series_id;
    char series_name[128];
    char short_name[64];
    race_category category;
    license_level min_license;
    int min_starters;
    int max_starters;
} ira_series;

/*
 * Schedule Entry (one race week)
 */
typedef struct {
    int race_week_num;          /* 0-indexed */
    int track_id;
    char track_name[128];
    char config_name[64];
    time_t start_date;
    time_t end_date;
    int race_time_limit_mins;   /* 0 = lap-based */
    int race_lap_limit;         /* 0 = time-based */
    int practice_mins;
    int qualify_mins;
    int warmup_mins;
    int car_ids[16];
    int car_count;
} ira_schedule_week;

/*
 * Season (instance of a series for a year/quarter)
 */
typedef struct {
    int season_id;
    int series_id;
    char season_name[128];
    char short_name[64];
    int season_year;
    int season_quarter;         /* 1-4 */
    bool fixed_setup;
    bool official;
    bool active;
    bool complete;
    license_level license_group;
    int max_weeks;
    int current_week;
    bool multiclass;
    bool has_supersessions;
    int car_class_ids[8];
    int car_class_count;
    /* Embedded schedule */
    ira_schedule_week *schedule;
    int schedule_count;
} ira_season;

/*
 * Owned Content (user's inventory)
 */
typedef struct {
    int cust_id;
    time_t last_updated;
    int *owned_car_ids;
    int owned_car_count;
    int *owned_track_ids;
    int owned_track_count;
} ira_owned_content;

/*
 * Filter Criteria
 */
typedef struct {
    bool owned_content_only;
    race_category categories[6];
    int category_count;
    license_level min_license;
    license_level max_license;
    bool fixed_setup_only;
    bool open_setup_only;
    bool official_only;
    int min_race_mins;
    int max_race_mins;
    int *excluded_series;
    int excluded_series_count;
    int *excluded_tracks;
    int excluded_track_count;
} ira_filter;

/*
 * Utility functions
 */

/* Convert category enum to string */
const char *category_to_string(race_category cat);

/* Convert string to category enum */
race_category string_to_category(const char *str);

/* Convert license enum to string (e.g., "A", "B", "Rookie") */
const char *license_to_string(license_level lic);

/* Convert string to license enum */
license_level string_to_license(const char *str);

/* Check if a category is active (non-legacy) */
bool category_is_active(race_category cat);

/*
 * Memory management
 */

/* Free a season's schedule array */
void season_free_schedule(ira_season *season);

/* Free owned content arrays */
void owned_content_free(ira_owned_content *content);

/* Free filter arrays */
void filter_free(ira_filter *filter);

#endif /* IRA_MODELS_H */
