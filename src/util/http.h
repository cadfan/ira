/*
 * ira - iRacing Application
 * HTTP Client (Windows WinHTTP)
 *
 * Copyright (c) 2026 Christopher Griffiths
 */

#ifndef IRA_HTTP_H
#define IRA_HTTP_H

#include <stdbool.h>
#include <stddef.h>

/*
 * HTTP Response
 */
typedef struct http_response {
    int status_code;
    char *body;
    size_t body_len;
    char *content_type;
    /* Rate limiting info from iRacing */
    int rate_limit_remaining;
    int rate_limit_reset;
} http_response;

/*
 * HTTP Session (opaque type)
 * Maintains cookies and connection state across requests.
 */
typedef struct http_session http_session;

/*
 * Session Lifecycle
 */

/* Create a new HTTP session. Returns NULL on error. */
http_session *http_session_create(void);

/* Destroy session and free resources. */
void http_session_destroy(http_session *session);

/*
 * Session Configuration
 */

/* Set request timeout in milliseconds (default: 30000). */
void http_session_set_timeout(http_session *session, int timeout_ms);

/* Set User-Agent header (default: "ira/0.1"). */
void http_session_set_user_agent(http_session *session, const char *user_agent);

/*
 * HTTP Requests
 */

/*
 * Send a POST request with JSON body.
 *
 * Content-Type is set to "application/json".
 * Returns response or NULL on error.
 * Caller must free response with http_response_free().
 */
http_response *http_post_json(http_session *session, const char *url, const char *json_body);

/*
 * Send a POST request with form-urlencoded body.
 *
 * Content-Type is set to "application/x-www-form-urlencoded".
 * Returns response or NULL on error.
 * Caller must free response with http_response_free().
 */
http_response *http_post_form(http_session *session, const char *url, const char *form_body);

/*
 * Send a GET request with Authorization Bearer token.
 *
 * Returns response or NULL on error.
 * Caller must free response with http_response_free().
 */
http_response *http_get_with_token(http_session *session, const char *url, const char *bearer_token);

/*
 * Send a GET request.
 *
 * Returns response or NULL on error.
 * Caller must free response with http_response_free().
 */
http_response *http_get(http_session *session, const char *url);

/*
 * Response Handling
 */

/* Free a response and its contents. */
void http_response_free(http_response *resp);

/* Check if response indicates success (2xx status). */
bool http_response_ok(http_response *resp);

/* Get response body as string (null-terminated). */
const char *http_response_body(http_response *resp);

/*
 * Error Handling
 */

/* Get last error message from session. */
const char *http_session_get_error(http_session *session);

#endif /* IRA_HTTP_H */
