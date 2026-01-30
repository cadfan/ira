/*
 * ira - iRacing Application
 * OAuth2 Authentication (Authorization Code Flow with PKCE)
 *
 * Copyright (c) 2026 Christopher Griffiths
 */

#ifndef IRA_OAUTH_H
#define IRA_OAUTH_H

#include <stdbool.h>
#include <time.h>

/*
 * OAuth2 Token structure
 */
typedef struct {
    char *access_token;
    char *refresh_token;
    char *token_type;       /* Always "Bearer" */
    time_t access_expires;  /* Absolute expiry time */
    time_t refresh_expires; /* Absolute expiry time */
    char *scope;
} oauth_token;

/*
 * OAuth2 Configuration
 */
typedef struct {
    char *client_id;
    char *client_secret;    /* Optional - only for server apps */
    char *redirect_uri;     /* e.g., "http://localhost:8080/callback" */
    int callback_port;      /* Port for local callback server */
    char *scope;            /* e.g., "iracing.auth" */
} oauth_config;

/*
 * OAuth2 Client
 */
typedef struct oauth_client oauth_client;

/*
 * Lifecycle
 */

/* Create OAuth client with configuration */
oauth_client *oauth_create(const oauth_config *config);

/* Destroy OAuth client */
void oauth_destroy(oauth_client *client);

/*
 * Token Management
 */

/* Get current access token (NULL if not authenticated) */
const char *oauth_get_access_token(oauth_client *client);

/* Check if access token is valid (not expired) */
bool oauth_token_valid(oauth_client *client);

/* Check if access token needs refresh (within margin_seconds of expiry) */
bool oauth_token_expiring(oauth_client *client, int margin_seconds);

/*
 * Authentication Flow
 */

/*
 * Start the OAuth authorization flow.
 *
 * This will:
 * 1. Generate PKCE code_verifier and code_challenge
 * 2. Start a local HTTP server to receive the callback
 * 3. Open the user's browser to the iRacing authorization page
 * 4. Wait for the user to authorize (blocking, with timeout)
 * 5. Exchange the authorization code for tokens
 *
 * Returns true on success, false on error.
 * Use oauth_get_error() for error details.
 */
bool oauth_authorize(oauth_client *client);

/*
 * Refresh the access token using the refresh token.
 *
 * Returns true on success, false on error.
 */
bool oauth_refresh(oauth_client *client);

/*
 * Token Persistence
 */

/* Save tokens to a JSON file */
bool oauth_save_tokens(oauth_client *client, const char *filename);

/* Load tokens from a JSON file */
bool oauth_load_tokens(oauth_client *client, const char *filename);

/*
 * Error Handling
 */

/* Get last error message */
const char *oauth_get_error(oauth_client *client);

/*
 * Constants
 */

#define OAUTH_AUTH_URL      "https://oauth.iracing.com/oauth2/authorize"
#define OAUTH_TOKEN_URL     "https://oauth.iracing.com/oauth2/token"
#define OAUTH_DEFAULT_PORT  8080
#define OAUTH_DEFAULT_SCOPE "iracing.auth"

#endif /* IRA_OAUTH_H */
