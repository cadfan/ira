/*
 * ira - iRacing Application
 * iRacing Data API Client
 *
 * Copyright (c) 2026 Christopher Griffiths
 *
 * Requires OAuth authentication with iRacing.
 * See: https://forums.iracing.com/discussion/15068/general-availability-of-data-api
 */

#ifndef IRA_IRACING_API_H
#define IRA_IRACING_API_H

#include <stdbool.h>
#include <stddef.h>

#include "../data/models.h"
#include "../data/database.h"

/*
 * API Error Codes
 */
typedef enum {
    API_OK = 0,
    API_ERROR_NOT_AUTHENTICATED,
    API_ERROR_INVALID_CREDENTIALS,
    API_ERROR_RATE_LIMITED,
    API_ERROR_NETWORK,
    API_ERROR_TIMEOUT,
    API_ERROR_SERVER_ERROR,
    API_ERROR_INVALID_RESPONSE,
    API_ERROR_NOT_IMPLEMENTED
} api_error;

/*
 * Authentication state
 */
typedef enum {
    AUTH_STATE_NONE,
    AUTH_STATE_AUTHENTICATING,
    AUTH_STATE_AUTHENTICATED,
    AUTH_STATE_FAILED,
    AUTH_STATE_EXPIRED
} auth_state;

/*
 * API Client structure
 */
typedef struct {
    /* Authentication */
    auth_state state;
    char *access_token;
    char *refresh_token;
    time_t token_expires;

    /* Rate limiting */
    int rate_limit_remaining;
    time_t rate_limit_reset;

    /* Configuration */
    char *username;
    char *password_hash;  /* SHA256(password + lowercase(email)) base64 encoded */
    int timeout_ms;

    /* Last error */
    api_error last_error;
    char last_error_msg[256];
} iracing_api;

/*
 * Lifecycle
 */

/* Create API client */
iracing_api *api_create(void);

/* Destroy API client and free resources */
void api_destroy(iracing_api *api);

/*
 * Configuration
 */

/* Set credentials for authentication */
void api_set_credentials(iracing_api *api, const char *email, const char *password);

/* Set request timeout in milliseconds */
void api_set_timeout(iracing_api *api, int timeout_ms);

/* Load saved tokens from file (for session persistence) */
bool api_load_tokens(iracing_api *api, const char *filename);

/* Save tokens to file */
bool api_save_tokens(iracing_api *api, const char *filename);

/*
 * Authentication
 */

/* Authenticate with iRacing (blocking) */
api_error api_authenticate(iracing_api *api);

/* Refresh access token using refresh token */
api_error api_refresh_token(iracing_api *api);

/* Check if currently authenticated */
bool api_is_authenticated(iracing_api *api);

/* Check if token needs refresh */
bool api_token_expiring(iracing_api *api, int margin_seconds);

/*
 * Data Fetching - Cars & Tracks
 * These are relatively static and can be cached for days
 */

/* Fetch all cars, populates db->cars */
api_error api_fetch_cars(iracing_api *api, ira_database *db);

/* Fetch all tracks, populates db->tracks */
api_error api_fetch_tracks(iracing_api *api, ira_database *db);

/* Fetch all car classes, populates db->car_classes */
api_error api_fetch_car_classes(iracing_api *api, ira_database *db);

/*
 * Data Fetching - Series & Seasons
 * Series are static, seasons change quarterly
 */

/* Fetch all series, populates db->series */
api_error api_fetch_series(iracing_api *api, ira_database *db);

/* Fetch current seasons (year/quarter), populates db->seasons */
api_error api_fetch_seasons(iracing_api *api, ira_database *db, int year, int quarter);

/* Fetch schedule for a specific season */
api_error api_fetch_season_schedule(iracing_api *api, ira_database *db, int season_id);

/*
 * Data Fetching - Member Data
 * Requires authentication as the member
 */

/* Fetch member info (cust_id, licenses, etc.) */
api_error api_fetch_member_info(iracing_api *api, ira_database *db);

/* Fetch owned cars and tracks, populates db->owned */
api_error api_fetch_owned_content(iracing_api *api, ira_database *db);

/*
 * Data Fetching - Live Data
 * Changes frequently, minimal caching
 */

/* Fetch race guide (upcoming races in next 3 hours) */
api_error api_fetch_race_guide(iracing_api *api, ira_database *db);

/* Fetch registration count for a session */
api_error api_fetch_session_registrations(iracing_api *api, int session_id, int *count);

/*
 * Convenience Functions
 */

/* Fetch all static data (cars, tracks, series) */
api_error api_fetch_static_data(iracing_api *api, ira_database *db);

/* Fetch all data needed for filtering (static + seasons + owned) */
api_error api_fetch_filter_data(iracing_api *api, ira_database *db);

/* Refresh stale data based on age thresholds */
api_error api_refresh_stale_data(iracing_api *api, ira_database *db);

/*
 * Error Handling
 */

/* Get human-readable error message */
const char *api_error_string(api_error err);

/* Get last error message from API */
const char *api_get_last_error(iracing_api *api);

/* Get rate limit info */
int api_get_rate_limit_remaining(iracing_api *api);
time_t api_get_rate_limit_reset(iracing_api *api);

/*
 * Constants - API Endpoints
 */

#define IRACING_API_BASE      "https://members-ng.iracing.com"
#define IRACING_AUTH_ENDPOINT "/auth"
#define IRACING_DATA_BASE     "/data"

/* Data endpoints */
#define API_CARS_GET            "/data/car/get"
#define API_CAR_ASSETS          "/data/car/assets"
#define API_CARCLASS_GET        "/data/carclass/get"
#define API_TRACKS_GET          "/data/track/get"
#define API_TRACK_ASSETS        "/data/track/assets"
#define API_SERIES_GET          "/data/series/get"
#define API_SERIES_ASSETS       "/data/series/assets"
#define API_SERIES_SEASONS      "/data/series/seasons"
#define API_SEASON_LIST         "/data/season/list"
#define API_SEASON_RACE_GUIDE   "/data/season/race_guide"
#define API_MEMBER_INFO         "/data/member/info"
#define API_MEMBER_PROFILE      "/data/member/profile"
#define API_CONSTANTS_CATEGORIES "/data/constants/categories"
#define API_CONSTANTS_DIVISIONS  "/data/constants/divisions"

#endif /* IRA_IRACING_API_H */
