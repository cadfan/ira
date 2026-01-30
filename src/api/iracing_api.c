/*
 * ira - iRacing Application
 * iRacing Data API Client - Legacy Authentication Implementation
 *
 * Copyright (c) 2026 Christopher Griffiths
 *
 * Uses cookie-based authentication with hashed password.
 * See: https://forums.iracing.com/discussion/15068/general-availability-of-data-api
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>

#include "iracing_api.h"
#include "../util/http.h"
#include "../util/crypto.h"
#include "../util/json.h"
#include "../util/oauth.h"

/*
 * Helper: Set API error from HTTP response
 */
static api_error map_http_status(iracing_api *api, http_response *resp)
{
    if (!resp) {
        api->last_error = API_ERROR_NETWORK;
        snprintf(api->last_error_msg, sizeof(api->last_error_msg),
                 "Network error: %s", http_session_get_error(api->http));
        return API_ERROR_NETWORK;
    }

    /* Update rate limit info */
    api->rate_limit_remaining = resp->rate_limit_remaining;

    switch (resp->status_code) {
        case 200:
        case 201:
        case 204:
            api->last_error = API_OK;
            api->last_error_msg[0] = '\0';
            return API_OK;

        case 401:
            api->last_error = API_ERROR_NOT_AUTHENTICATED;
            snprintf(api->last_error_msg, sizeof(api->last_error_msg),
                     "Not authenticated (401)");
            api->state = AUTH_STATE_EXPIRED;
            return API_ERROR_NOT_AUTHENTICATED;

        case 403:
            api->last_error = API_ERROR_INVALID_CREDENTIALS;
            snprintf(api->last_error_msg, sizeof(api->last_error_msg),
                     "Invalid credentials (403)");
            api->state = AUTH_STATE_FAILED;
            return API_ERROR_INVALID_CREDENTIALS;

        case 429:
            api->last_error = API_ERROR_RATE_LIMITED;
            snprintf(api->last_error_msg, sizeof(api->last_error_msg),
                     "Rate limited (429). Reset in %d seconds.",
                     resp->rate_limit_reset);
            return API_ERROR_RATE_LIMITED;

        default:
            if (resp->status_code >= 500) {
                api->last_error = API_ERROR_SERVER_ERROR;
                snprintf(api->last_error_msg, sizeof(api->last_error_msg),
                         "Server error (%d)", resp->status_code);
                return API_ERROR_SERVER_ERROR;
            }
            api->last_error = API_ERROR_INVALID_RESPONSE;
            snprintf(api->last_error_msg, sizeof(api->last_error_msg),
                     "Unexpected response (%d)", resp->status_code);
            return API_ERROR_INVALID_RESPONSE;
    }
}

/*
 * Helper: Fetch data from an iRacing endpoint with link-redirect pattern.
 *
 * iRacing data endpoints return {"link": "https://s3..."} which must be fetched.
 * Returns parsed JSON from the final URL, or NULL on error.
 */
static json_value *fetch_data_endpoint(iracing_api *api, const char *endpoint)
{
    if (!api || !endpoint) return NULL;

    char url[512];
    snprintf(url, sizeof(url), "%s%s", IRACING_API_BASE, endpoint);

    /* First request to get the link */
    http_response *resp;

    /* Use OAuth token if available */
    if (api->oauth && oauth_token_valid(api->oauth)) {
        const char *token = oauth_get_access_token(api->oauth);
        resp = http_get_with_token(api->http, url, token);
    } else {
        resp = http_get(api->http, url);
    }
    if (!resp) {
        map_http_status(api, NULL);
        return NULL;
    }

    if (!http_response_ok(resp)) {
        map_http_status(api, resp);
        http_response_free(resp);
        return NULL;
    }

    /* Parse response to extract link */
    json_value *link_json = json_parse(resp->body);
    http_response_free(resp);

    if (!link_json) {
        api->last_error = API_ERROR_INVALID_RESPONSE;
        snprintf(api->last_error_msg, sizeof(api->last_error_msg),
                 "Failed to parse link response");
        return NULL;
    }

    const char *link = json_get_string(json_object_get(link_json, "link"));
    if (!link) {
        json_free(link_json);
        api->last_error = API_ERROR_INVALID_RESPONSE;
        snprintf(api->last_error_msg, sizeof(api->last_error_msg),
                 "No link in response");
        return NULL;
    }

    /* Fetch the actual data from S3 */
    resp = http_get(api->http, link);
    json_free(link_json);

    if (!resp) {
        map_http_status(api, NULL);
        return NULL;
    }

    if (!http_response_ok(resp)) {
        map_http_status(api, resp);
        http_response_free(resp);
        return NULL;
    }

    /* Parse the actual data */
    json_value *data = json_parse(resp->body);
    http_response_free(resp);

    if (!data) {
        api->last_error = API_ERROR_INVALID_RESPONSE;
        snprintf(api->last_error_msg, sizeof(api->last_error_msg),
                 "Failed to parse data response");
        return NULL;
    }

    api->last_error = API_OK;
    return data;
}

/*
 * Helper: Safe string copy
 */
static void safe_strcpy(char *dest, size_t dest_size, const char *src)
{
    if (!dest || !src || dest_size == 0) return;
    strncpy(dest, src, dest_size - 1);
    dest[dest_size - 1] = '\0';
}

/*
 * Lifecycle
 */

iracing_api *api_create(void)
{
    iracing_api *api = calloc(1, sizeof(iracing_api));
    if (!api) return NULL;

    api->http = http_session_create();
    if (!api->http) {
        free(api);
        return NULL;
    }

    api->state = AUTH_STATE_NONE;
    api->timeout_ms = 30000;  /* 30 second default */
    api->last_error = API_OK;

    http_session_set_timeout(api->http, api->timeout_ms);

    return api;
}

void api_destroy(iracing_api *api)
{
    if (!api) return;

    if (api->http) http_session_destroy(api->http);
    if (api->oauth) oauth_destroy(api->oauth);
    if (api->access_token) free(api->access_token);
    if (api->refresh_token) free(api->refresh_token);
    if (api->username) free(api->username);
    if (api->password_hash) {
        /* Clear sensitive data before freeing */
        memset(api->password_hash, 0, strlen(api->password_hash));
        free(api->password_hash);
    }

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

    /* Hash password: Base64(SHA256(password + lowercase(email))) */
    if (api->password_hash) {
        memset(api->password_hash, 0, strlen(api->password_hash));
        free(api->password_hash);
        api->password_hash = NULL;
    }

    if (email && password) {
        api->password_hash = crypto_iracing_password_hash(password, email);
    }

    /* Reset auth state when credentials change */
    api->state = AUTH_STATE_NONE;
}

void api_set_oauth(iracing_api *api, const char *client_id, const char *client_secret)
{
    if (!api || !client_id) return;

    /* Destroy existing OAuth client */
    if (api->oauth) {
        oauth_destroy(api->oauth);
        api->oauth = NULL;
    }

    /* Create new OAuth client */
    oauth_config config = {0};
    config.client_id = (char*)client_id;
    config.client_secret = (char*)client_secret;
    config.redirect_uri = "http://localhost:8080/callback";
    config.callback_port = 8080;
    config.scope = "iracing.auth";

    api->oauth = oauth_create(&config);

    /* Reset auth state */
    api->state = AUTH_STATE_NONE;
}

void api_set_timeout(iracing_api *api, int timeout_ms)
{
    if (!api) return;
    api->timeout_ms = timeout_ms;
    if (api->http) {
        http_session_set_timeout(api->http, timeout_ms);
    }
}

bool api_load_tokens(iracing_api *api, const char *filename)
{
    (void)api;
    (void)filename;
    /* Legacy auth uses session cookies managed by WinHTTP, not tokens */
    return false;
}

bool api_save_tokens(iracing_api *api, const char *filename)
{
    (void)api;
    (void)filename;
    /* Legacy auth uses session cookies managed by WinHTTP, not tokens */
    return false;
}

/*
 * Authentication
 */

api_error api_authenticate(iracing_api *api)
{
    if (!api) return API_ERROR_INVALID_CREDENTIALS;

    /* Prefer OAuth if configured */
    if (api->oauth) {
        api->state = AUTH_STATE_AUTHENTICATING;

        /* Try to load existing tokens first */
        if (oauth_load_tokens(api->oauth, "oauth_tokens.json")) {
            if (oauth_token_valid(api->oauth)) {
                api->state = AUTH_STATE_AUTHENTICATED;
                api->token_expires = time(NULL) + 3600;  /* Estimate */
                return API_OK;
            } else if (oauth_token_expiring(api->oauth, 0)) {
                /* Try to refresh */
                if (oauth_refresh(api->oauth)) {
                    oauth_save_tokens(api->oauth, "oauth_tokens.json");
                    api->state = AUTH_STATE_AUTHENTICATED;
                    api->token_expires = time(NULL) + 3600;
                    return API_OK;
                }
            }
        }

        /* Need to do full OAuth flow */
        printf("\n");
        printf("=== OAuth2 Authorization Required ===\n");
        printf("A browser window will open for you to log in to iRacing.\n");
        printf("After logging in, you'll be redirected back to this application.\n");
        printf("\n");

        if (oauth_authorize(api->oauth)) {
            oauth_save_tokens(api->oauth, "oauth_tokens.json");
            api->state = AUTH_STATE_AUTHENTICATED;
            api->token_expires = time(NULL) + 3600;
            printf("Authentication successful!\n");
            return API_OK;
        } else {
            api->state = AUTH_STATE_FAILED;
            api->last_error = API_ERROR_INVALID_CREDENTIALS;
            snprintf(api->last_error_msg, sizeof(api->last_error_msg),
                     "OAuth authentication failed: %s", oauth_get_error(api->oauth));
            return API_ERROR_INVALID_CREDENTIALS;
        }
    }

    /* Legacy auth (no longer supported by iRacing since Dec 2025) */
    if (!api->username || !api->password_hash) {
        api->last_error = API_ERROR_INVALID_CREDENTIALS;
        snprintf(api->last_error_msg, sizeof(api->last_error_msg),
                 "No credentials set. Use api_set_oauth() for OAuth2 authentication.");
        return API_ERROR_INVALID_CREDENTIALS;
    }

    api->state = AUTH_STATE_AUTHENTICATING;

    /* Build auth request body */
    char body[1024];
    snprintf(body, sizeof(body),
             "{\"email\":\"%s\",\"password\":\"%s\"}",
             api->username, api->password_hash);

    /* POST to auth endpoint */
    char url[256];
    snprintf(url, sizeof(url), "%s%s", IRACING_API_BASE, IRACING_AUTH_ENDPOINT);

    http_response *resp = http_post_json(api->http, url, body);

    /* Clear request body (contains credentials) */
    memset(body, 0, sizeof(body));

    if (!resp) {
        api->state = AUTH_STATE_FAILED;
        return map_http_status(api, NULL);
    }

    api_error err = map_http_status(api, resp);

    if (http_response_ok(resp)) {
        /* Parse response for authcode (optional - we use cookies) */
        json_value *json = json_parse(resp->body);
        if (json) {
            /* Check for verification_required (2FA enabled) */
            json_value *verify = json_object_get(json, "verificationRequired");
            if (verify && json_get_bool(verify)) {
                api->state = AUTH_STATE_FAILED;
                api->last_error = API_ERROR_INVALID_CREDENTIALS;
                snprintf(api->last_error_msg, sizeof(api->last_error_msg),
                         "Account has 2FA enabled. Legacy auth requires 2FA disabled.");
                json_free(json);
                http_response_free(resp);
                return API_ERROR_INVALID_CREDENTIALS;
            }

            json_free(json);
        }

        api->state = AUTH_STATE_AUTHENTICATED;
        api->token_expires = time(NULL) + (2 * 60 * 60);  /* ~2 hours */
        err = API_OK;
    } else {
        api->state = AUTH_STATE_FAILED;
    }

    http_response_free(resp);
    return err;
}

api_error api_refresh_token(iracing_api *api)
{
    /* Legacy auth: re-authenticate to refresh session */
    if (!api) return API_ERROR_NOT_AUTHENTICATED;

    if (api->username && api->password_hash) {
        return api_authenticate(api);
    }

    api->last_error = API_ERROR_NOT_AUTHENTICATED;
    return API_ERROR_NOT_AUTHENTICATED;
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
 * Data Fetching - Cars
 */
api_error api_fetch_cars(iracing_api *api, ira_database *db)
{
    if (!api) return API_ERROR_NOT_AUTHENTICATED;
    if (!api_is_authenticated(api)) return API_ERROR_NOT_AUTHENTICATED;

    json_value *data = fetch_data_endpoint(api, API_CARS_GET);
    if (!data) return api->last_error;

    /* Data is an array of car objects */
    if (json_get_type(data) != JSON_ARRAY) {
        json_free(data);
        api->last_error = API_ERROR_INVALID_RESPONSE;
        snprintf(api->last_error_msg, sizeof(api->last_error_msg),
                 "Expected array of cars");
        return API_ERROR_INVALID_RESPONSE;
    }

    int count = json_array_length(data);

    /* Free existing cars */
    if (db->cars) {
        free(db->cars);
        db->cars = NULL;
        db->car_count = 0;
    }

    if (count == 0) {
        json_free(data);
        db->cars_updated = time(NULL);
        return API_OK;
    }

    db->cars = calloc(count, sizeof(ira_car));
    if (!db->cars) {
        json_free(data);
        api->last_error = API_ERROR_INVALID_RESPONSE;
        return API_ERROR_INVALID_RESPONSE;
    }

    db->car_count = count;

    for (int i = 0; i < count; i++) {
        json_value *c = json_array_get(data, i);
        if (!c) continue;

        ira_car *car = &db->cars[i];

        car->car_id = json_get_int(json_object_get(c, "car_id"));

        const char *name = json_get_string(json_object_get(c, "car_name"));
        if (name) safe_strcpy(car->car_name, sizeof(car->car_name), name);

        const char *abbrev = json_get_string(json_object_get(c, "car_name_abbreviated"));
        if (abbrev) safe_strcpy(car->car_abbrev, sizeof(car->car_abbrev), abbrev);

        const char *make = json_get_string(json_object_get(c, "car_make"));
        if (make) safe_strcpy(car->car_make, sizeof(car->car_make), make);

        const char *model = json_get_string(json_object_get(c, "car_model"));
        if (model) safe_strcpy(car->car_model, sizeof(car->car_model), model);

        car->hp = json_get_int(json_object_get(c, "hp"));
        car->weight_kg = json_get_int(json_object_get(c, "car_weight"));
        car->price = (float)json_get_number(json_object_get(c, "price"));
        car->free_with_subscription = json_get_bool(json_object_get(c, "free_with_subscription"));
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
                json_value *cat_str = json_array_get(cats, j);
                if (json_get_type(cat_str) == JSON_STRING) {
                    car->categories[j] = string_to_category(json_get_string(cat_str));
                }
            }
        }
    }

    db->cars_updated = time(NULL);
    json_free(data);
    return API_OK;
}

/*
 * Data Fetching - Tracks
 */
api_error api_fetch_tracks(iracing_api *api, ira_database *db)
{
    if (!api) return API_ERROR_NOT_AUTHENTICATED;
    if (!api_is_authenticated(api)) return API_ERROR_NOT_AUTHENTICATED;

    json_value *data = fetch_data_endpoint(api, API_TRACKS_GET);
    if (!data) return api->last_error;

    if (json_get_type(data) != JSON_ARRAY) {
        json_free(data);
        api->last_error = API_ERROR_INVALID_RESPONSE;
        snprintf(api->last_error_msg, sizeof(api->last_error_msg),
                 "Expected array of tracks");
        return API_ERROR_INVALID_RESPONSE;
    }

    int count = json_array_length(data);

    /* Free existing tracks */
    if (db->tracks) {
        free(db->tracks);
        db->tracks = NULL;
        db->track_count = 0;
    }

    if (count == 0) {
        json_free(data);
        db->tracks_updated = time(NULL);
        return API_OK;
    }

    db->tracks = calloc(count, sizeof(ira_track));
    if (!db->tracks) {
        json_free(data);
        api->last_error = API_ERROR_INVALID_RESPONSE;
        return API_ERROR_INVALID_RESPONSE;
    }

    db->track_count = count;

    for (int i = 0; i < count; i++) {
        json_value *t = json_array_get(data, i);
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
        track->length_km = (float)json_get_number(json_object_get(t, "track_config_length"));
        track->corners = json_get_int(json_object_get(t, "corners_per_lap"));
        track->max_cars = json_get_int(json_object_get(t, "max_cars"));
        track->grid_stalls = json_get_int(json_object_get(t, "grid_stalls"));
        track->pit_speed_kph = json_get_int(json_object_get(t, "pit_road_speed_limit"));
        track->price = (float)json_get_number(json_object_get(t, "price"));
        track->free_with_subscription = json_get_bool(json_object_get(t, "free_with_subscription"));
        track->retired = json_get_bool(json_object_get(t, "retired"));
        track->package_id = json_get_int(json_object_get(t, "package_id"));
        track->sku = json_get_int(json_object_get(t, "sku"));

        const char *loc = json_get_string(json_object_get(t, "location"));
        if (loc) safe_strcpy(track->location, sizeof(track->location), loc);

        track->latitude = (float)json_get_number(json_object_get(t, "latitude"));
        track->longitude = (float)json_get_number(json_object_get(t, "longitude"));
        track->night_lighting = json_get_bool(json_object_get(t, "has_opt_path"));
        track->ai_enabled = json_get_bool(json_object_get(t, "ai_enabled"));
    }

    db->tracks_updated = time(NULL);
    json_free(data);
    return API_OK;
}

/*
 * Data Fetching - Car Classes
 */
api_error api_fetch_car_classes(iracing_api *api, ira_database *db)
{
    if (!api) return API_ERROR_NOT_AUTHENTICATED;
    if (!api_is_authenticated(api)) return API_ERROR_NOT_AUTHENTICATED;

    json_value *data = fetch_data_endpoint(api, API_CARCLASS_GET);
    if (!data) return api->last_error;

    if (json_get_type(data) != JSON_ARRAY) {
        json_free(data);
        api->last_error = API_ERROR_INVALID_RESPONSE;
        return API_ERROR_INVALID_RESPONSE;
    }

    int count = json_array_length(data);

    /* Free existing car classes */
    if (db->car_classes) {
        free(db->car_classes);
        db->car_classes = NULL;
        db->car_class_count = 0;
    }

    if (count == 0) {
        json_free(data);
        db->car_classes_updated = time(NULL);
        return API_OK;
    }

    db->car_classes = calloc(count, sizeof(ira_car_class));
    if (!db->car_classes) {
        json_free(data);
        api->last_error = API_ERROR_INVALID_RESPONSE;
        return API_ERROR_INVALID_RESPONSE;
    }

    db->car_class_count = count;

    for (int i = 0; i < count; i++) {
        json_value *cc = json_array_get(data, i);
        if (!cc) continue;

        ira_car_class *car_class = &db->car_classes[i];

        car_class->car_class_id = json_get_int(json_object_get(cc, "car_class_id"));

        const char *name = json_get_string(json_object_get(cc, "name"));
        if (name) safe_strcpy(car_class->car_class_name, sizeof(car_class->car_class_name), name);

        const char *short_name = json_get_string(json_object_get(cc, "short_name"));
        if (short_name) safe_strcpy(car_class->short_name, sizeof(car_class->short_name), short_name);

        /* Parse cars_in_class array */
        json_value *cars = json_object_get(cc, "cars_in_class");
        if (cars && json_get_type(cars) == JSON_ARRAY) {
            int car_count = json_array_length(cars);
            if (car_count > 32) car_count = 32;
            car_class->car_count = car_count;
            for (int j = 0; j < car_count; j++) {
                json_value *car_entry = json_array_get(cars, j);
                if (car_entry) {
                    car_class->car_ids[j] = json_get_int(json_object_get(car_entry, "car_id"));
                }
            }
        }
    }

    db->car_classes_updated = time(NULL);
    json_free(data);
    return API_OK;
}

/*
 * Data Fetching - Series
 */
api_error api_fetch_series(iracing_api *api, ira_database *db)
{
    if (!api) return API_ERROR_NOT_AUTHENTICATED;
    if (!api_is_authenticated(api)) return API_ERROR_NOT_AUTHENTICATED;

    json_value *data = fetch_data_endpoint(api, API_SERIES_GET);
    if (!data) return api->last_error;

    if (json_get_type(data) != JSON_ARRAY) {
        json_free(data);
        api->last_error = API_ERROR_INVALID_RESPONSE;
        return API_ERROR_INVALID_RESPONSE;
    }

    int count = json_array_length(data);

    /* Free existing series */
    if (db->series) {
        free(db->series);
        db->series = NULL;
        db->series_count = 0;
    }

    if (count == 0) {
        json_free(data);
        db->series_updated = time(NULL);
        return API_OK;
    }

    db->series = calloc(count, sizeof(ira_series));
    if (!db->series) {
        json_free(data);
        api->last_error = API_ERROR_INVALID_RESPONSE;
        return API_ERROR_INVALID_RESPONSE;
    }

    db->series_count = count;

    for (int i = 0; i < count; i++) {
        json_value *s = json_array_get(data, i);
        if (!s) continue;

        ira_series *series = &db->series[i];

        series->series_id = json_get_int(json_object_get(s, "series_id"));

        const char *name = json_get_string(json_object_get(s, "series_name"));
        if (name) safe_strcpy(series->series_name, sizeof(series->series_name), name);

        const char *short_name = json_get_string(json_object_get(s, "series_short_name"));
        if (short_name) safe_strcpy(series->short_name, sizeof(series->short_name), short_name);

        series->category = json_get_int(json_object_get(s, "category_id"));

        /* Parse allowed_licenses to get min_license */
        json_value *licenses = json_object_get(s, "allowed_licenses");
        if (licenses && json_get_type(licenses) == JSON_ARRAY && json_array_length(licenses) > 0) {
            json_value *first_lic = json_array_get(licenses, 0);
            if (first_lic) {
                series->min_license = json_get_int(json_object_get(first_lic, "group_name"));
            }
        }

        series->min_starters = json_get_int(json_object_get(s, "min_starters"));
        series->max_starters = json_get_int(json_object_get(s, "max_starters"));
    }

    db->series_updated = time(NULL);
    json_free(data);
    return API_OK;
}

/*
 * Data Fetching - Seasons
 */
api_error api_fetch_seasons(iracing_api *api, ira_database *db, int year, int quarter)
{
    if (!api) return API_ERROR_NOT_AUTHENTICATED;
    if (!api_is_authenticated(api)) return API_ERROR_NOT_AUTHENTICATED;

    char endpoint[256];
    snprintf(endpoint, sizeof(endpoint), "%s?season_year=%d&season_quarter=%d",
             API_SERIES_SEASONS, year, quarter);

    json_value *data = fetch_data_endpoint(api, endpoint);
    if (!data) return api->last_error;

    if (json_get_type(data) != JSON_ARRAY) {
        json_free(data);
        api->last_error = API_ERROR_INVALID_RESPONSE;
        return API_ERROR_INVALID_RESPONSE;
    }

    int count = json_array_length(data);

    /* Free existing seasons */
    if (db->seasons) {
        for (int i = 0; i < db->season_count; i++) {
            season_free_schedule(&db->seasons[i]);
        }
        free(db->seasons);
        db->seasons = NULL;
        db->season_count = 0;
    }

    db->season_year = year;
    db->season_quarter = quarter;

    if (count == 0) {
        json_free(data);
        db->seasons_updated = time(NULL);
        return API_OK;
    }

    db->seasons = calloc(count, sizeof(ira_season));
    if (!db->seasons) {
        json_free(data);
        api->last_error = API_ERROR_INVALID_RESPONSE;
        return API_ERROR_INVALID_RESPONSE;
    }

    db->season_count = count;

    for (int i = 0; i < count; i++) {
        json_value *s = json_array_get(data, i);
        if (!s) continue;

        ira_season *season = &db->seasons[i];

        season->season_id = json_get_int(json_object_get(s, "season_id"));
        season->series_id = json_get_int(json_object_get(s, "series_id"));

        const char *name = json_get_string(json_object_get(s, "season_name"));
        if (name) safe_strcpy(season->season_name, sizeof(season->season_name), name);

        const char *short_name = json_get_string(json_object_get(s, "season_short_name"));
        if (short_name) safe_strcpy(season->short_name, sizeof(season->short_name), short_name);

        season->season_year = json_get_int(json_object_get(s, "season_year"));
        season->season_quarter = json_get_int(json_object_get(s, "season_quarter"));
        season->fixed_setup = json_get_bool(json_object_get(s, "fixed_setup"));
        season->official = json_get_bool(json_object_get(s, "official"));
        season->active = json_get_bool(json_object_get(s, "active"));
        season->license_group = json_get_int(json_object_get(s, "license_group"));

        /* Parse schedules array */
        json_value *schedules = json_object_get(s, "schedules");
        if (schedules && json_get_type(schedules) == JSON_ARRAY) {
            int sched_count = json_array_length(schedules);
            if (sched_count > 0) {
                season->schedule = calloc(sched_count, sizeof(ira_schedule_week));
                if (season->schedule) {
                    season->schedule_count = sched_count;
                    season->max_weeks = sched_count;

                    for (int j = 0; j < sched_count; j++) {
                        json_value *w = json_array_get(schedules, j);
                        if (!w) continue;

                        ira_schedule_week *week = &season->schedule[j];

                        week->race_week_num = json_get_int(json_object_get(w, "race_week_num"));

                        /* Track info may be nested */
                        json_value *track_obj = json_object_get(w, "track");
                        if (track_obj) {
                            week->track_id = json_get_int(json_object_get(track_obj, "track_id"));
                            const char *track_name = json_get_string(json_object_get(track_obj, "track_name"));
                            if (track_name) safe_strcpy(week->track_name, sizeof(week->track_name), track_name);
                            const char *config = json_get_string(json_object_get(track_obj, "config_name"));
                            if (config) safe_strcpy(week->config_name, sizeof(week->config_name), config);
                        }

                        week->race_time_limit_mins = json_get_int(json_object_get(w, "race_time_limit"));
                        week->race_lap_limit = json_get_int(json_object_get(w, "race_lap_limit"));

                        /* Parse car_class_ids or car_ids */
                        json_value *car_classes = json_object_get(w, "car_class_ids");
                        if (car_classes && json_get_type(car_classes) == JSON_ARRAY) {
                            /* Would need to resolve car class to cars */
                        }
                    }
                }
            }
        }

        /* Parse car_class_ids */
        json_value *cc_ids = json_object_get(s, "car_class_ids");
        if (cc_ids && json_get_type(cc_ids) == JSON_ARRAY) {
            int cc_count = json_array_length(cc_ids);
            if (cc_count > 8) cc_count = 8;
            season->car_class_count = cc_count;
            for (int j = 0; j < cc_count; j++) {
                season->car_class_ids[j] = json_get_int(json_array_get(cc_ids, j));
            }
        }
    }

    db->seasons_updated = time(NULL);
    json_free(data);
    return API_OK;
}

api_error api_fetch_season_schedule(iracing_api *api, ira_database *db, int season_id)
{
    (void)db;
    (void)season_id;
    if (!api) return API_ERROR_NOT_AUTHENTICATED;
    if (!api_is_authenticated(api)) return API_ERROR_NOT_AUTHENTICATED;

    /* Season schedules are embedded in api_fetch_seasons response */
    api->last_error = API_OK;
    return API_OK;
}

/*
 * Data Fetching - Member Data
 */
api_error api_fetch_member_info(iracing_api *api, ira_database *db)
{
    if (!api) return API_ERROR_NOT_AUTHENTICATED;
    if (!api_is_authenticated(api)) return API_ERROR_NOT_AUTHENTICATED;

    json_value *data = fetch_data_endpoint(api, API_MEMBER_INFO);
    if (!data) return api->last_error;

    /* Extract cust_id */
    db->owned.cust_id = json_get_int(json_object_get(data, "cust_id"));

    json_free(data);
    return API_OK;
}

api_error api_fetch_owned_content(iracing_api *api, ira_database *db)
{
    if (!api) return API_ERROR_NOT_AUTHENTICATED;
    if (!api_is_authenticated(api)) return API_ERROR_NOT_AUTHENTICATED;

    /*
     * iRacing doesn't have a direct "owned content" endpoint.
     * We determine owned content by checking what's free + purchased.
     *
     * For now, mark all free_with_subscription items as owned,
     * and actual purchases would need to come from member profile.
     */

    /* Free existing owned lists */
    owned_content_free(&db->owned);

    /* Count free cars and tracks */
    int free_cars = 0;
    int free_tracks = 0;

    for (int i = 0; i < db->car_count; i++) {
        if (db->cars[i].free_with_subscription) free_cars++;
    }
    for (int i = 0; i < db->track_count; i++) {
        if (db->tracks[i].free_with_subscription) free_tracks++;
    }

    /* Allocate owned arrays */
    if (free_cars > 0) {
        db->owned.owned_car_ids = malloc(free_cars * sizeof(int));
        if (db->owned.owned_car_ids) {
            int idx = 0;
            for (int i = 0; i < db->car_count; i++) {
                if (db->cars[i].free_with_subscription) {
                    db->owned.owned_car_ids[idx++] = db->cars[i].car_id;
                }
            }
            db->owned.owned_car_count = free_cars;
        }
    }

    if (free_tracks > 0) {
        db->owned.owned_track_ids = malloc(free_tracks * sizeof(int));
        if (db->owned.owned_track_ids) {
            int idx = 0;
            for (int i = 0; i < db->track_count; i++) {
                if (db->tracks[i].free_with_subscription) {
                    db->owned.owned_track_ids[idx++] = db->tracks[i].track_id;
                }
            }
            db->owned.owned_track_count = free_tracks;
        }
    }

    db->owned.last_updated = time(NULL);
    return API_OK;
}

api_error api_fetch_race_guide(iracing_api *api, ira_database *db)
{
    (void)db;
    if (!api) return API_ERROR_NOT_AUTHENTICATED;
    if (!api_is_authenticated(api)) return API_ERROR_NOT_AUTHENTICATED;

    /* TODO: Implement race guide parsing */
    api->last_error = API_ERROR_NOT_IMPLEMENTED;
    return API_ERROR_NOT_IMPLEMENTED;
}

api_error api_fetch_session_registrations(iracing_api *api, int session_id, int *count)
{
    (void)session_id;
    if (!api) return API_ERROR_NOT_AUTHENTICATED;
    if (count) *count = 0;

    /* TODO: Implement session registrations */
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
    if (err != API_OK) return err;

    err = api_fetch_tracks(api, db);
    if (err != API_OK) return err;

    err = api_fetch_series(api, db);
    if (err != API_OK) return err;

    return API_OK;
}

api_error api_fetch_filter_data(iracing_api *api, ira_database *db)
{
    api_error err;

    err = api_fetch_static_data(api, db);
    if (err != API_OK) return err;

    /* Get current year/quarter */
    time_t now = time(NULL);
    struct tm *tm = localtime(&now);
    if (!tm) {
        api->last_error = API_ERROR_INVALID_RESPONSE;
        snprintf(api->last_error_msg, sizeof(api->last_error_msg), "Failed to get current time");
        return API_ERROR_INVALID_RESPONSE;
    }
    int year = tm->tm_year + 1900;
    int quarter = (tm->tm_mon / 3) + 1;

    err = api_fetch_seasons(api, db, year, quarter);
    if (err != API_OK) return err;

    err = api_fetch_owned_content(api, db);
    if (err != API_OK) return err;

    return API_OK;
}

api_error api_refresh_stale_data(iracing_api *api, ira_database *db)
{
    api_error err = API_OK;

    /* Refresh cars/tracks if older than 7 days */
    if (database_cars_stale(db, 7 * 24)) {
        err = api_fetch_cars(api, db);
        if (err != API_OK) return err;
    }

    if (database_tracks_stale(db, 7 * 24)) {
        err = api_fetch_tracks(api, db);
        if (err != API_OK) return err;
    }

    /* Refresh seasons if older than 1 hour */
    if (database_seasons_stale(db, 1)) {
        time_t now = time(NULL);
        struct tm *tm = localtime(&now);
        if (tm) {
            int year = tm->tm_year + 1900;
            int quarter = (tm->tm_mon / 3) + 1;
            err = api_fetch_seasons(api, db, year, quarter);
        }
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
