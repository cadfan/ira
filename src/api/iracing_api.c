/*
 * ira - iRacing Application
 * iRacing Data API Client - Stub Implementation
 *
 * Copyright (c) 2026 Christopher Griffiths
 *
 * TODO: Implement when OAuth access is approved by iRacing.
 * This stub provides the interface and returns NOT_IMPLEMENTED errors.
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>

#include "iracing_api.h"

/*
 * Lifecycle
 */

iracing_api *api_create(void)
{
    iracing_api *api = calloc(1, sizeof(iracing_api));
    if (!api) return NULL;

    api->state = AUTH_STATE_NONE;
    api->timeout_ms = 30000;  /* 30 second default */
    api->last_error = API_OK;

    return api;
}

void api_destroy(iracing_api *api)
{
    if (!api) return;

    if (api->access_token) free(api->access_token);
    if (api->refresh_token) free(api->refresh_token);
    if (api->username) free(api->username);
    if (api->password_hash) free(api->password_hash);

    free(api);
}

/*
 * Configuration
 */

void api_set_credentials(iracing_api *api, const char *email, const char *password)
{
    if (!api) return;

    /* Store email */
    if (api->username) free(api->username);
    api->username = email ? strdup(email) : NULL;

    /* TODO: Hash password properly: SHA256(password + lowercase(email)), then base64 */
    /* For now, just store placeholder */
    if (api->password_hash) free(api->password_hash);
    api->password_hash = password ? strdup(password) : NULL;

    (void)password;  /* Suppress unused warning until implemented */
}

void api_set_timeout(iracing_api *api, int timeout_ms)
{
    if (!api) return;
    api->timeout_ms = timeout_ms;
}

bool api_load_tokens(iracing_api *api, const char *filename)
{
    (void)api;
    (void)filename;
    /* TODO: Load tokens from JSON file */
    return false;
}

bool api_save_tokens(iracing_api *api, const char *filename)
{
    (void)api;
    (void)filename;
    /* TODO: Save tokens to JSON file */
    return false;
}

/*
 * Authentication
 */

api_error api_authenticate(iracing_api *api)
{
    if (!api) return API_ERROR_INVALID_CREDENTIALS;

    /*
     * TODO: Implement authentication
     *
     * 1. POST to https://members-ng.iracing.com/auth
     * 2. Body: {"email": "...", "password": "base64(sha256(password + lowercase(email)))"}
     * 3. Headers: Content-Type: application/json
     * 4. Response sets cookies for session
     * 5. Store authcode from response
     */

    api->last_error = API_ERROR_NOT_IMPLEMENTED;
    snprintf(api->last_error_msg, sizeof(api->last_error_msg),
             "Authentication not yet implemented. Waiting for OAuth approval.");

    return API_ERROR_NOT_IMPLEMENTED;
}

api_error api_refresh_token(iracing_api *api)
{
    if (!api) return API_ERROR_NOT_AUTHENTICATED;

    api->last_error = API_ERROR_NOT_IMPLEMENTED;
    return API_ERROR_NOT_IMPLEMENTED;
}

bool api_is_authenticated(iracing_api *api)
{
    if (!api) return false;
    return api->state == AUTH_STATE_AUTHENTICATED;
}

bool api_token_expiring(iracing_api *api, int margin_seconds)
{
    if (!api || api->token_expires == 0) return true;

    time_t now = time(NULL);
    return (api->token_expires - now) < margin_seconds;
}

/*
 * Data Fetching - Stubs
 */

api_error api_fetch_cars(iracing_api *api, ira_database *db)
{
    (void)db;
    if (!api) return API_ERROR_NOT_AUTHENTICATED;

    /*
     * TODO: GET /data/car/get
     * Returns array of car objects with:
     * - car_id, car_name, car_name_abbreviated
     * - car_make, car_model, hp, car_weight
     * - categories[], price, free_with_subscription
     * - retired, ai_enabled, rain_enabled
     */

    api->last_error = API_ERROR_NOT_IMPLEMENTED;
    return API_ERROR_NOT_IMPLEMENTED;
}

api_error api_fetch_tracks(iracing_api *api, ira_database *db)
{
    (void)db;
    if (!api) return API_ERROR_NOT_AUTHENTICATED;

    /*
     * TODO: GET /data/track/get
     * Returns array of track objects with:
     * - track_id, track_name, config_name
     * - category_id, is_oval, is_dirt
     * - track_config_length, corners_per_lap
     * - price, free_with_subscription, retired
     */

    api->last_error = API_ERROR_NOT_IMPLEMENTED;
    return API_ERROR_NOT_IMPLEMENTED;
}

api_error api_fetch_car_classes(iracing_api *api, ira_database *db)
{
    (void)db;
    if (!api) return API_ERROR_NOT_AUTHENTICATED;

    api->last_error = API_ERROR_NOT_IMPLEMENTED;
    return API_ERROR_NOT_IMPLEMENTED;
}

api_error api_fetch_series(iracing_api *api, ira_database *db)
{
    (void)db;
    if (!api) return API_ERROR_NOT_AUTHENTICATED;

    /*
     * TODO: GET /data/series/get
     * Returns array of series with:
     * - series_id, series_name, series_short_name
     * - category_id, allowed_licenses[]
     * - min_starters, max_starters
     */

    api->last_error = API_ERROR_NOT_IMPLEMENTED;
    return API_ERROR_NOT_IMPLEMENTED;
}

api_error api_fetch_seasons(iracing_api *api, ira_database *db, int year, int quarter)
{
    (void)db;
    (void)year;
    (void)quarter;
    if (!api) return API_ERROR_NOT_AUTHENTICATED;

    /*
     * TODO: GET /data/series/seasons?season_year=YYYY&season_quarter=Q
     * Returns array of season objects with schedule
     */

    api->last_error = API_ERROR_NOT_IMPLEMENTED;
    return API_ERROR_NOT_IMPLEMENTED;
}

api_error api_fetch_season_schedule(iracing_api *api, ira_database *db, int season_id)
{
    (void)db;
    (void)season_id;
    if (!api) return API_ERROR_NOT_AUTHENTICATED;

    api->last_error = API_ERROR_NOT_IMPLEMENTED;
    return API_ERROR_NOT_IMPLEMENTED;
}

api_error api_fetch_member_info(iracing_api *api, ira_database *db)
{
    (void)db;
    if (!api) return API_ERROR_NOT_AUTHENTICATED;

    /*
     * TODO: GET /data/member/info
     * Returns authenticated member's info:
     * - cust_id, display_name
     * - licenses (per category with level, sr, ir)
     * - club_id, club_name
     */

    api->last_error = API_ERROR_NOT_IMPLEMENTED;
    return API_ERROR_NOT_IMPLEMENTED;
}

api_error api_fetch_owned_content(iracing_api *api, ira_database *db)
{
    (void)db;
    if (!api) return API_ERROR_NOT_AUTHENTICATED;

    /*
     * TODO: Need to find correct endpoint for owned content
     * May need to iterate through cars/tracks and check purchasable status
     * or use member profile endpoint
     */

    api->last_error = API_ERROR_NOT_IMPLEMENTED;
    return API_ERROR_NOT_IMPLEMENTED;
}

api_error api_fetch_race_guide(iracing_api *api, ira_database *db)
{
    (void)db;
    if (!api) return API_ERROR_NOT_AUTHENTICATED;

    /*
     * TODO: GET /data/season/race_guide
     * Returns upcoming races in next 3 hours:
     * - session start times
     * - series/season info
     * - registration counts
     */

    api->last_error = API_ERROR_NOT_IMPLEMENTED;
    return API_ERROR_NOT_IMPLEMENTED;
}

api_error api_fetch_session_registrations(iracing_api *api, int session_id, int *count)
{
    (void)session_id;
    if (!api) return API_ERROR_NOT_AUTHENTICATED;
    if (count) *count = 0;

    api->last_error = API_ERROR_NOT_IMPLEMENTED;
    return API_ERROR_NOT_IMPLEMENTED;
}

/*
 * Convenience Functions
 */

api_error api_fetch_static_data(iracing_api *api, ira_database *db)
{
    api_error err;

    err = api_fetch_cars(api, db);
    if (err != API_OK && err != API_ERROR_NOT_IMPLEMENTED) return err;

    err = api_fetch_tracks(api, db);
    if (err != API_OK && err != API_ERROR_NOT_IMPLEMENTED) return err;

    err = api_fetch_series(api, db);
    if (err != API_OK && err != API_ERROR_NOT_IMPLEMENTED) return err;

    return API_ERROR_NOT_IMPLEMENTED;
}

api_error api_fetch_filter_data(iracing_api *api, ira_database *db)
{
    api_error err;

    err = api_fetch_static_data(api, db);
    if (err != API_OK && err != API_ERROR_NOT_IMPLEMENTED) return err;

    /* Get current year/quarter */
    time_t now = time(NULL);
    struct tm *tm = localtime(&now);
    int year = tm->tm_year + 1900;
    int quarter = (tm->tm_mon / 3) + 1;

    err = api_fetch_seasons(api, db, year, quarter);
    if (err != API_OK && err != API_ERROR_NOT_IMPLEMENTED) return err;

    err = api_fetch_owned_content(api, db);
    if (err != API_OK && err != API_ERROR_NOT_IMPLEMENTED) return err;

    return API_ERROR_NOT_IMPLEMENTED;
}

api_error api_refresh_stale_data(iracing_api *api, ira_database *db)
{
    api_error err = API_OK;

    /* Refresh cars/tracks if older than 7 days */
    if (database_cars_stale(db, 7 * 24)) {
        err = api_fetch_cars(api, db);
        if (err != API_OK && err != API_ERROR_NOT_IMPLEMENTED) return err;
    }

    if (database_tracks_stale(db, 7 * 24)) {
        err = api_fetch_tracks(api, db);
        if (err != API_OK && err != API_ERROR_NOT_IMPLEMENTED) return err;
    }

    /* Refresh seasons if older than 1 hour */
    if (database_seasons_stale(db, 1)) {
        time_t now = time(NULL);
        struct tm *tm = localtime(&now);
        int year = tm->tm_year + 1900;
        int quarter = (tm->tm_mon / 3) + 1;

        err = api_fetch_seasons(api, db, year, quarter);
    }

    return err;
}

/*
 * Error Handling
 */

const char *api_error_string(api_error err)
{
    switch (err) {
        case API_OK:                      return "Success";
        case API_ERROR_NOT_AUTHENTICATED: return "Not authenticated";
        case API_ERROR_INVALID_CREDENTIALS: return "Invalid credentials";
        case API_ERROR_RATE_LIMITED:      return "Rate limited";
        case API_ERROR_NETWORK:           return "Network error";
        case API_ERROR_TIMEOUT:           return "Request timeout";
        case API_ERROR_SERVER_ERROR:      return "Server error";
        case API_ERROR_INVALID_RESPONSE:  return "Invalid response";
        case API_ERROR_NOT_IMPLEMENTED:   return "Not implemented";
        default:                          return "Unknown error";
    }
}

const char *api_get_last_error(iracing_api *api)
{
    if (!api) return "Invalid API handle";
    if (api->last_error_msg[0]) return api->last_error_msg;
    return api_error_string(api->last_error);
}

int api_get_rate_limit_remaining(iracing_api *api)
{
    return api ? api->rate_limit_remaining : 0;
}

time_t api_get_rate_limit_reset(iracing_api *api)
{
    return api ? api->rate_limit_reset : 0;
}
