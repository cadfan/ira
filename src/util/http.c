/*
 * ira - iRacing Application
 * HTTP Client Implementation (Windows WinHTTP)
 *
 * Copyright (c) 2026 Christopher Griffiths
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winhttp.h>

#include "http.h"

#define DEFAULT_TIMEOUT_MS 30000
#define DEFAULT_USER_AGENT L"Mozilla/5.0 (Windows NT 10.0; Win64; x64) ira/0.1"
#define INITIAL_BUFFER_SIZE 4096
#define MAX_ERROR_MSG 256

/*
 * HTTP Session structure
 */
struct http_session {
    HINTERNET session;
    int timeout_ms;
    wchar_t *user_agent;
    char last_error[MAX_ERROR_MSG];
};

/*
 * Helper: Convert UTF-8 to wide string
 */
static wchar_t *utf8_to_wide(const char *str)
{
    if (!str) return NULL;

    int len = MultiByteToWideChar(CP_UTF8, 0, str, -1, NULL, 0);
    if (len <= 0) return NULL;

    wchar_t *wide = malloc(len * sizeof(wchar_t));
    if (!wide) return NULL;

    MultiByteToWideChar(CP_UTF8, 0, str, -1, wide, len);
    return wide;
}

/*
 * Helper: Set error message
 */
static void set_error(http_session *session, const char *msg)
{
    if (session && msg) {
        strncpy(session->last_error, msg, MAX_ERROR_MSG - 1);
        session->last_error[MAX_ERROR_MSG - 1] = '\0';
    }
}

/*
 * Helper: Set Windows error message
 */
static void set_win_error(http_session *session, const char *prefix, DWORD err)
{
    if (!session) return;

    char msg[MAX_ERROR_MSG];
    snprintf(msg, sizeof(msg), "%s: error %lu", prefix, (unsigned long)err);
    set_error(session, msg);
}

/*
 * Helper: Parse URL into components
 */
typedef struct {
    wchar_t *host;
    wchar_t *path;
    INTERNET_PORT port;
    bool secure;
} url_parts;

static bool parse_url(const char *url, url_parts *parts)
{
    if (!url || !parts) return false;

    memset(parts, 0, sizeof(url_parts));

    wchar_t *wide_url = utf8_to_wide(url);
    if (!wide_url) return false;

    URL_COMPONENTS components;
    memset(&components, 0, sizeof(components));
    components.dwStructSize = sizeof(components);

    /* Request host and path parsing */
    wchar_t host[256] = {0};
    wchar_t path[2048] = {0};

    components.lpszHostName = host;
    components.dwHostNameLength = sizeof(host) / sizeof(wchar_t);
    components.lpszUrlPath = path;
    components.dwUrlPathLength = sizeof(path) / sizeof(wchar_t);

    if (!WinHttpCrackUrl(wide_url, 0, 0, &components)) {
        free(wide_url);
        return false;
    }

    parts->host = _wcsdup(host);
    parts->path = _wcsdup(path);
    parts->port = components.nPort;
    parts->secure = (components.nScheme == INTERNET_SCHEME_HTTPS);

    free(wide_url);
    return (parts->host && parts->path);
}

static void free_url_parts(url_parts *parts)
{
    if (parts) {
        free(parts->host);
        free(parts->path);
        memset(parts, 0, sizeof(url_parts));
    }
}

/*
 * Helper: Parse rate limit headers
 */
static void parse_rate_limit_headers(HINTERNET request, http_response *resp)
{
    wchar_t buffer[64];
    DWORD size = sizeof(buffer);

    /* X-RateLimit-Remaining */
    if (WinHttpQueryHeaders(request, WINHTTP_QUERY_CUSTOM, L"X-RateLimit-Remaining",
                            buffer, &size, WINHTTP_NO_HEADER_INDEX)) {
        resp->rate_limit_remaining = _wtoi(buffer);
    }

    /* X-RateLimit-Reset */
    size = sizeof(buffer);
    if (WinHttpQueryHeaders(request, WINHTTP_QUERY_CUSTOM, L"X-RateLimit-Reset",
                            buffer, &size, WINHTTP_NO_HEADER_INDEX)) {
        resp->rate_limit_reset = _wtoi(buffer);
    }
}

/*
 * Helper: Read response body
 */
static bool read_response_body(HINTERNET request, http_response *resp)
{
    size_t capacity = INITIAL_BUFFER_SIZE;
    size_t len = 0;
    char *buffer = malloc(capacity);

    if (!buffer) return false;

    DWORD bytes_available;
    DWORD bytes_read;

    while (WinHttpQueryDataAvailable(request, &bytes_available) && bytes_available > 0) {
        /* Expand buffer if needed */
        if (len + bytes_available + 1 > capacity) {
            capacity = (len + bytes_available + 1) * 2;
            char *new_buf = realloc(buffer, capacity);
            if (!new_buf) {
                free(buffer);
                return false;
            }
            buffer = new_buf;
        }

        if (!WinHttpReadData(request, buffer + len, bytes_available, &bytes_read)) {
            free(buffer);
            return false;
        }

        len += bytes_read;
    }

    /* Null-terminate for convenience */
    buffer[len] = '\0';

    resp->body = buffer;
    resp->body_len = len;
    return true;
}

/*
 * Session Lifecycle
 */

http_session *http_session_create(void)
{
    http_session *session = calloc(1, sizeof(http_session));
    if (!session) return NULL;

    session->timeout_ms = DEFAULT_TIMEOUT_MS;
    session->user_agent = _wcsdup(DEFAULT_USER_AGENT);

    if (!session->user_agent) {
        free(session);
        return NULL;
    }

    /* Create WinHTTP session with automatic cookie handling */
    session->session = WinHttpOpen(
        session->user_agent,
        WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
        WINHTTP_NO_PROXY_NAME,
        WINHTTP_NO_PROXY_BYPASS,
        0
    );

    if (!session->session) {
        set_win_error(session, "WinHttpOpen failed", GetLastError());
        free(session->user_agent);
        free(session);
        return NULL;
    }

    /* Enable automatic cookie handling */
    DWORD option = WINHTTP_OPTION_REDIRECT_POLICY_ALWAYS;
    WinHttpSetOption(session->session, WINHTTP_OPTION_REDIRECT_POLICY,
                     &option, sizeof(option));

    return session;
}

void http_session_destroy(http_session *session)
{
    if (!session) return;

    if (session->session) {
        WinHttpCloseHandle(session->session);
    }
    free(session->user_agent);
    free(session);
}

/*
 * Session Configuration
 */

void http_session_set_timeout(http_session *session, int timeout_ms)
{
    if (session) {
        session->timeout_ms = timeout_ms;
    }
}

void http_session_set_user_agent(http_session *session, const char *user_agent)
{
    if (!session || !user_agent) return;

    wchar_t *wide = utf8_to_wide(user_agent);
    if (wide) {
        free(session->user_agent);
        session->user_agent = wide;
    }
}

/*
 * Helper: Send request and get response
 */
static http_response *send_request(http_session *session, const char *url,
                                   const char *method, const char *body,
                                   const wchar_t *content_type)
{
    if (!session || !url) return NULL;

    session->last_error[0] = '\0';

    /* Parse URL */
    url_parts parts;
    if (!parse_url(url, &parts)) {
        set_error(session, "Failed to parse URL");
        return NULL;
    }


    http_response *resp = NULL;
    HINTERNET connect = NULL;
    HINTERNET request = NULL;

    /* Connect to host */
    connect = WinHttpConnect(session->session, parts.host, parts.port, 0);
    if (!connect) {
        set_win_error(session, "WinHttpConnect failed", GetLastError());
        goto cleanup;
    }

    /* Create request */
    wchar_t *wide_method = utf8_to_wide(method);
    DWORD flags = parts.secure ? WINHTTP_FLAG_SECURE : 0;

    request = WinHttpOpenRequest(
        connect,
        wide_method,
        parts.path,
        NULL,  /* HTTP/1.1 */
        WINHTTP_NO_REFERER,
        WINHTTP_DEFAULT_ACCEPT_TYPES,
        flags
    );
    free(wide_method);

    if (!request) {
        set_win_error(session, "WinHttpOpenRequest failed", GetLastError());
        goto cleanup;
    }

    /* Set timeouts */
    int timeout = session->timeout_ms;
    WinHttpSetOption(request, WINHTTP_OPTION_CONNECT_TIMEOUT, &timeout, sizeof(timeout));
    WinHttpSetOption(request, WINHTTP_OPTION_SEND_TIMEOUT, &timeout, sizeof(timeout));
    WinHttpSetOption(request, WINHTTP_OPTION_RECEIVE_TIMEOUT, &timeout, sizeof(timeout));

    /* Add required headers */
    WinHttpAddRequestHeaders(request, L"Accept: application/json", (DWORD)-1, WINHTTP_ADDREQ_FLAG_ADD);

    /* Add Content-Type header if specified */
    if (content_type) {
        size_t ct_len = wcslen(content_type);
        size_t header_len = ct_len + 20;  /* "Content-Type: " + content_type + null */
        wchar_t *header = malloc(header_len * sizeof(wchar_t));
        if (header) {
            swprintf(header, header_len, L"Content-Type: %s", content_type);
            WinHttpAddRequestHeaders(request, header, (DWORD)-1, WINHTTP_ADDREQ_FLAG_ADD);
            free(header);
        }
    }

    /* Send request */
    DWORD body_len = body ? (DWORD)strlen(body) : 0;
    if (!WinHttpSendRequest(request, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                            (LPVOID)body, body_len, body_len, 0)) {
        set_win_error(session, "WinHttpSendRequest failed", GetLastError());
        goto cleanup;
    }

    /* Receive response */
    if (!WinHttpReceiveResponse(request, NULL)) {
        set_win_error(session, "WinHttpReceiveResponse failed", GetLastError());
        goto cleanup;
    }

    /* Allocate response */
    resp = calloc(1, sizeof(http_response));
    if (!resp) {
        set_error(session, "Memory allocation failed");
        goto cleanup;
    }

    /* Get status code */
    DWORD status_code = 0;
    DWORD size = sizeof(status_code);
    WinHttpQueryHeaders(request, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                        WINHTTP_HEADER_NAME_BY_INDEX, &status_code, &size,
                        WINHTTP_NO_HEADER_INDEX);
    resp->status_code = (int)status_code;

    /* Parse rate limit headers */
    parse_rate_limit_headers(request, resp);


    /* Read body */
    if (!read_response_body(request, resp)) {
        set_error(session, "Failed to read response body");
        http_response_free(resp);
        resp = NULL;
        goto cleanup;
    }

cleanup:
    if (request) WinHttpCloseHandle(request);
    if (connect) WinHttpCloseHandle(connect);
    free_url_parts(&parts);

    return resp;
}

/*
 * HTTP Requests
 */

http_response *http_post_json(http_session *session, const char *url, const char *json_body)
{
    return send_request(session, url, "POST", json_body, L"application/json");
}

http_response *http_post_form(http_session *session, const char *url, const char *form_body)
{
    return send_request(session, url, "POST", form_body, L"application/x-www-form-urlencoded");
}

http_response *http_get(http_session *session, const char *url)
{
    return send_request(session, url, "GET", NULL, NULL);
}

http_response *http_get_with_token(http_session *session, const char *url, const char *bearer_token)
{
    if (!session || !url) return NULL;

    session->last_error[0] = '\0';

    /* Parse URL */
    url_parts parts;
    if (!parse_url(url, &parts)) {
        set_error(session, "Failed to parse URL");
        return NULL;
    }


    http_response *resp = NULL;
    HINTERNET connect = NULL;
    HINTERNET request = NULL;

    /* Connect to host */
    connect = WinHttpConnect(session->session, parts.host, parts.port, 0);
    if (!connect) {
        set_win_error(session, "WinHttpConnect failed", GetLastError());
        goto cleanup;
    }

    /* Create request */
    DWORD flags = parts.secure ? WINHTTP_FLAG_SECURE : 0;

    request = WinHttpOpenRequest(
        connect,
        L"GET",
        parts.path,
        NULL,
        WINHTTP_NO_REFERER,
        WINHTTP_DEFAULT_ACCEPT_TYPES,
        flags
    );

    if (!request) {
        set_win_error(session, "WinHttpOpenRequest failed", GetLastError());
        goto cleanup;
    }

    /* Set timeouts */
    int timeout = session->timeout_ms;
    WinHttpSetOption(request, WINHTTP_OPTION_CONNECT_TIMEOUT, &timeout, sizeof(timeout));
    WinHttpSetOption(request, WINHTTP_OPTION_SEND_TIMEOUT, &timeout, sizeof(timeout));
    WinHttpSetOption(request, WINHTTP_OPTION_RECEIVE_TIMEOUT, &timeout, sizeof(timeout));

    /* Add Authorization header */
    if (bearer_token) {
        size_t token_len = strlen(bearer_token);
        size_t header_len = token_len + 30;  /* "Authorization: Bearer " + token + null */
        wchar_t *auth_header = malloc(header_len * sizeof(wchar_t));
        if (auth_header) {
            swprintf(auth_header, header_len, L"Authorization: Bearer %hs", bearer_token);
            WinHttpAddRequestHeaders(request, auth_header, (DWORD)-1, WINHTTP_ADDREQ_FLAG_ADD);
            free(auth_header);
        }
    }

    /* Add Accept header */
    WinHttpAddRequestHeaders(request, L"Accept: application/json", (DWORD)-1, WINHTTP_ADDREQ_FLAG_ADD);

    /* Send request */
    if (!WinHttpSendRequest(request, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                            WINHTTP_NO_REQUEST_DATA, 0, 0, 0)) {
        set_win_error(session, "WinHttpSendRequest failed", GetLastError());
        goto cleanup;
    }

    /* Receive response */
    if (!WinHttpReceiveResponse(request, NULL)) {
        set_win_error(session, "WinHttpReceiveResponse failed", GetLastError());
        goto cleanup;
    }

    /* Allocate response */
    resp = calloc(1, sizeof(http_response));
    if (!resp) {
        set_error(session, "Memory allocation failed");
        goto cleanup;
    }

    /* Get status code */
    DWORD status_code = 0;
    DWORD size = sizeof(status_code);
    WinHttpQueryHeaders(request, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                        WINHTTP_HEADER_NAME_BY_INDEX, &status_code, &size,
                        WINHTTP_NO_HEADER_INDEX);
    resp->status_code = (int)status_code;


    /* Parse rate limit headers */
    parse_rate_limit_headers(request, resp);

    /* Read body */
    if (!read_response_body(request, resp)) {
        set_error(session, "Failed to read response body");
        http_response_free(resp);
        resp = NULL;
        goto cleanup;
    }

cleanup:
    if (request) WinHttpCloseHandle(request);
    if (connect) WinHttpCloseHandle(connect);
    free_url_parts(&parts);

    return resp;
}

/*
 * Response Handling
 */

void http_response_free(http_response *resp)
{
    if (!resp) return;

    free(resp->body);
    free(resp->content_type);
    free(resp);
}

bool http_response_ok(http_response *resp)
{
    return resp && resp->status_code >= 200 && resp->status_code < 300;
}

const char *http_response_body(http_response *resp)
{
    return resp ? resp->body : NULL;
}

/*
 * Error Handling
 */

const char *http_session_get_error(http_session *session)
{
    if (!session) return "Invalid session";
    if (session->last_error[0]) return session->last_error;
    return "No error";
}
