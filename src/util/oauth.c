/*
 * ira - iRacing Application
 * OAuth2 Authentication Implementation
 *
 * Copyright (c) 2026 Christopher Griffiths
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <shellapi.h>
#include <wincrypt.h>

#include "oauth.h"
#include "crypto.h"
#include "http.h"
#include "json.h"

#pragma comment(lib, "ws2_32.lib")

#define MAX_ERROR_MSG 512
#define CODE_VERIFIER_LEN 64
#define STATE_LEN 32
#define CALLBACK_TIMEOUT_SEC 300  /* 5 minutes */

/*
 * OAuth2 Client structure
 */
struct oauth_client {
    /* Configuration */
    oauth_config config;

    /* Current tokens */
    oauth_token tokens;

    /* PKCE state (during auth flow) */
    char *code_verifier;
    char *code_challenge;
    char *state;

    /* HTTP session for token requests */
    http_session *http;

    /* Error handling */
    char last_error[MAX_ERROR_MSG];
};

/*
 * Helper: Set error message
 */
static void set_error(oauth_client *client, const char *msg)
{
    if (client && msg) {
        strncpy(client->last_error, msg, MAX_ERROR_MSG - 1);
        client->last_error[MAX_ERROR_MSG - 1] = '\0';
    }
}

/*
 * Helper: Generate random alphanumeric string
 */
static char *generate_random_string(int length)
{
    static const char charset[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-._~";

    char *str = malloc(length + 1);
    if (!str) return NULL;

    HCRYPTPROV prov;
    if (!CryptAcquireContext(&prov, NULL, NULL, PROV_RSA_FULL, CRYPT_VERIFYCONTEXT)) {
        /* Fallback to less secure random */
        srand((unsigned int)time(NULL));
        for (int i = 0; i < length; i++) {
            str[i] = charset[rand() % (sizeof(charset) - 1)];
        }
    } else {
        unsigned char *buf = malloc(length);
        if (buf && CryptGenRandom(prov, length, buf)) {
            for (int i = 0; i < length; i++) {
                str[i] = charset[buf[i] % (sizeof(charset) - 1)];
            }
        }
        free(buf);
        CryptReleaseContext(prov, 0);
    }

    str[length] = '\0';
    return str;
}

/*
 * Helper: Base64URL encode (no padding, URL-safe)
 */
static char *base64url_encode(const unsigned char *data, size_t len)
{
    char *base64 = crypto_base64_encode(data, len);
    if (!base64) return NULL;

    /* Convert to URL-safe: + -> -, / -> _, remove = padding */
    size_t out_len = strlen(base64);
    for (size_t i = 0; i < out_len; i++) {
        if (base64[i] == '+') base64[i] = '-';
        else if (base64[i] == '/') base64[i] = '_';
    }

    /* Remove padding */
    while (out_len > 0 && base64[out_len - 1] == '=') {
        base64[--out_len] = '\0';
    }

    return base64;
}

/*
 * Helper: Generate PKCE code challenge from verifier
 */
static char *generate_code_challenge(const char *verifier)
{
    if (!verifier) return NULL;

    /* SHA256 hash of verifier */
    unsigned char *hash = crypto_sha256(verifier, strlen(verifier));
    if (!hash) return NULL;

    /* Base64URL encode */
    char *challenge = base64url_encode(hash, 32);
    free(hash);

    return challenge;
}

/*
 * Helper: URL encode a string
 */
static char *url_encode(const char *str)
{
    if (!str) return NULL;

    size_t len = strlen(str);
    /* Worst case: every char needs encoding (3x size) */
    char *encoded = malloc(len * 3 + 1);
    if (!encoded) return NULL;

    char *out = encoded;
    for (size_t i = 0; i < len; i++) {
        char c = str[i];
        if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
            (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.' || c == '~') {
            *out++ = c;
        } else {
            sprintf(out, "%%%02X", (unsigned char)c);
            out += 3;
        }
    }
    *out = '\0';

    return encoded;
}

/*
 * Helper: Open URL in default browser
 */
static bool open_browser(const char *url)
{
    HINSTANCE result = ShellExecuteA(NULL, "open", url, NULL, NULL, SW_SHOWNORMAL);
    return (INT_PTR)result > 32;
}

/*
 * Helper: Simple HTTP callback server
 * Waits for a single GET request and extracts the 'code' parameter
 */
static char *wait_for_callback(int port, const char *expected_state, int timeout_sec)
{
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        return NULL;
    }

    char *auth_code = NULL;
    SOCKET server_socket = INVALID_SOCKET;
    SOCKET client_socket = INVALID_SOCKET;

    /* Create socket */
    server_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (server_socket == INVALID_SOCKET) {
        goto cleanup;
    }

    /* Allow address reuse */
    int opt = 1;
    setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, (char*)&opt, sizeof(opt));

    /* Bind to port */
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons((u_short)port);

    if (bind(server_socket, (struct sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR) {
        goto cleanup;
    }

    /* Listen */
    if (listen(server_socket, 1) == SOCKET_ERROR) {
        goto cleanup;
    }

    /* Set timeout */
    DWORD timeout_ms = timeout_sec * 1000;
    setsockopt(server_socket, SOL_SOCKET, SO_RCVTIMEO, (char*)&timeout_ms, sizeof(timeout_ms));

    printf("Waiting for authorization (timeout: %d seconds)...\n", timeout_sec);

    /* Accept connection */
    client_socket = accept(server_socket, NULL, NULL);
    if (client_socket == INVALID_SOCKET) {
        goto cleanup;
    }

    /* Read request */
    char buffer[4096];
    int recv_len = recv(client_socket, buffer, sizeof(buffer) - 1, 0);
    if (recv_len <= 0) {
        goto cleanup;
    }
    buffer[recv_len] = '\0';

    /* Parse the request to extract code and state */
    /* Expected: GET /callback?code=XXX&state=YYY HTTP/1.1 */
    char *code_start = strstr(buffer, "code=");
    char *state_start = strstr(buffer, "state=");

    if (code_start && state_start) {
        /* Extract code */
        code_start += 5;  /* Skip "code=" */
        char *code_end = code_start;
        while (*code_end && *code_end != '&' && *code_end != ' ') code_end++;

        /* Extract state */
        state_start += 6;  /* Skip "state=" */
        char *state_end = state_start;
        while (*state_end && *state_end != '&' && *state_end != ' ') state_end++;

        /* Verify state */
        size_t state_len = state_end - state_start;
        if (state_len == strlen(expected_state) &&
            strncmp(state_start, expected_state, state_len) == 0) {
            /* State matches - extract code */
            size_t code_len = code_end - code_start;
            auth_code = malloc(code_len + 1);
            if (auth_code) {
                memcpy(auth_code, code_start, code_len);
                auth_code[code_len] = '\0';
            }
        }
    }

    /* Send response to browser */
    const char *response_body = auth_code
        ? "<html><body><h1>Authorization Successful!</h1><p>You can close this window and return to the application.</p></body></html>"
        : "<html><body><h1>Authorization Failed</h1><p>State mismatch or missing code.</p></body></html>";

    char response[1024];
    snprintf(response, sizeof(response),
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: text/html\r\n"
        "Content-Length: %zu\r\n"
        "Connection: close\r\n"
        "\r\n%s",
        strlen(response_body), response_body);

    send(client_socket, response, (int)strlen(response), 0);

cleanup:
    if (client_socket != INVALID_SOCKET) closesocket(client_socket);
    if (server_socket != INVALID_SOCKET) closesocket(server_socket);
    WSACleanup();

    return auth_code;
}

/*
 * Helper: Exchange authorization code for tokens
 */
static bool exchange_code_for_tokens(oauth_client *client, const char *code)
{
    if (!client || !code) return false;

    /* Build request body */
    char *encoded_uri = url_encode(client->config.redirect_uri);
    if (!encoded_uri) {
        set_error(client, "Failed to encode redirect_uri");
        return false;
    }

    char body[2048];
    snprintf(body, sizeof(body),
        "grant_type=authorization_code"
        "&client_id=%s"
        "&code=%s"
        "&redirect_uri=%s"
        "&code_verifier=%s",
        client->config.client_id,
        code,
        encoded_uri,
        client->code_verifier);

    free(encoded_uri);

    /* Add client_secret if we have one */
    if (client->config.client_secret) {
        size_t body_len = strlen(body);
        size_t remaining = sizeof(body) - body_len;
        snprintf(body + body_len, remaining, "&client_secret=%s",
                 client->config.client_secret);
    }

    /* Make token request */
    /* Note: Token endpoint uses form-urlencoded, not JSON */
    char url[256];
    snprintf(url, sizeof(url), "%s", OAUTH_TOKEN_URL);

    /* We need to send with Content-Type: application/x-www-form-urlencoded */
    /* Our http module sends JSON, so we need a custom approach */
    /* For now, let's add a simple form POST capability */

    http_response *resp = http_post_form(client->http, url, body);

    if (!resp) {
        set_error(client, "Token request failed");
        return false;
    }

    if (!http_response_ok(resp)) {
        char err[MAX_ERROR_MSG];
        snprintf(err, sizeof(err), "Token request failed with status %d: %s",
                 resp->status_code, resp->body ? resp->body : "no body");
        set_error(client, err);
        http_response_free(resp);
        return false;
    }

    /* Parse token response */
    json_value *json = json_parse(resp->body);
    http_response_free(resp);

    if (!json) {
        set_error(client, "Failed to parse token response");
        return false;
    }

    /* Extract tokens */
    const char *access = json_get_string(json_object_get(json, "access_token"));
    const char *refresh = json_get_string(json_object_get(json, "refresh_token"));
    const char *type = json_get_string(json_object_get(json, "token_type"));
    int expires_in = json_get_int(json_object_get(json, "expires_in"));
    int refresh_expires_in = json_get_int(json_object_get(json, "refresh_token_expires_in"));

    if (!access) {
        set_error(client, "No access_token in response");
        json_free(json);
        return false;
    }

    /* Store tokens */
    free(client->tokens.access_token);
    free(client->tokens.refresh_token);
    free(client->tokens.token_type);

    client->tokens.access_token = strdup(access);
    client->tokens.refresh_token = refresh ? strdup(refresh) : NULL;
    client->tokens.token_type = type ? strdup(type) : strdup("Bearer");

    time_t now = time(NULL);
    client->tokens.access_expires = now + expires_in;
    client->tokens.refresh_expires = refresh_expires_in > 0 ? now + refresh_expires_in : 0;

    json_free(json);
    return true;
}

/*
 * Lifecycle
 */

oauth_client *oauth_create(const oauth_config *config)
{
    if (!config || !config->client_id) return NULL;

    oauth_client *client = calloc(1, sizeof(oauth_client));
    if (!client) return NULL;

    /* Copy configuration */
    client->config.client_id = strdup(config->client_id);
    client->config.client_secret = config->client_secret ? strdup(config->client_secret) : NULL;
    client->config.redirect_uri = config->redirect_uri
        ? strdup(config->redirect_uri)
        : strdup("http://localhost:8080/callback");
    client->config.callback_port = config->callback_port > 0
        ? config->callback_port
        : OAUTH_DEFAULT_PORT;
    client->config.scope = config->scope
        ? strdup(config->scope)
        : strdup(OAUTH_DEFAULT_SCOPE);

    /* Create HTTP session */
    client->http = http_session_create();
    if (!client->http) {
        oauth_destroy(client);
        return NULL;
    }

    return client;
}

void oauth_destroy(oauth_client *client)
{
    if (!client) return;

    /* Free config */
    free(client->config.client_id);
    free(client->config.client_secret);
    free(client->config.redirect_uri);
    free(client->config.scope);

    /* Free tokens (clear sensitive data first) */
    if (client->tokens.access_token) {
        memset(client->tokens.access_token, 0, strlen(client->tokens.access_token));
        free(client->tokens.access_token);
    }
    if (client->tokens.refresh_token) {
        memset(client->tokens.refresh_token, 0, strlen(client->tokens.refresh_token));
        free(client->tokens.refresh_token);
    }
    free(client->tokens.token_type);
    free(client->tokens.scope);

    /* Free PKCE state */
    if (client->code_verifier) {
        memset(client->code_verifier, 0, strlen(client->code_verifier));
        free(client->code_verifier);
    }
    free(client->code_challenge);
    free(client->state);

    /* Free HTTP session */
    if (client->http) http_session_destroy(client->http);

    free(client);
}

/*
 * Token Management
 */

const char *oauth_get_access_token(oauth_client *client)
{
    if (!client) return NULL;
    return client->tokens.access_token;
}

bool oauth_token_valid(oauth_client *client)
{
    if (!client || !client->tokens.access_token) return false;
    return time(NULL) < client->tokens.access_expires;
}

bool oauth_token_expiring(oauth_client *client, int margin_seconds)
{
    if (!client || !client->tokens.access_token) return true;
    return (client->tokens.access_expires - time(NULL)) < margin_seconds;
}

/*
 * Authentication Flow
 */

bool oauth_authorize(oauth_client *client)
{
    if (!client) return false;

    /* Generate PKCE code verifier */
    free(client->code_verifier);
    client->code_verifier = generate_random_string(CODE_VERIFIER_LEN);
    if (!client->code_verifier) {
        set_error(client, "Failed to generate code verifier");
        return false;
    }

    /* Generate code challenge */
    free(client->code_challenge);
    client->code_challenge = generate_code_challenge(client->code_verifier);
    if (!client->code_challenge) {
        set_error(client, "Failed to generate code challenge");
        return false;
    }

    /* Generate state */
    free(client->state);
    client->state = generate_random_string(STATE_LEN);
    if (!client->state) {
        set_error(client, "Failed to generate state");
        return false;
    }

    /* Build authorization URL */
    char *encoded_redirect = url_encode(client->config.redirect_uri);
    char *encoded_scope = url_encode(client->config.scope);

    if (!encoded_redirect || !encoded_scope) {
        free(encoded_redirect);
        free(encoded_scope);
        set_error(client, "Failed to encode URL parameters");
        return false;
    }

    char auth_url[2048];
    snprintf(auth_url, sizeof(auth_url),
        "%s?client_id=%s"
        "&redirect_uri=%s"
        "&response_type=code"
        "&code_challenge=%s"
        "&code_challenge_method=S256"
        "&state=%s"
        "&scope=%s",
        OAUTH_AUTH_URL,
        client->config.client_id,
        encoded_redirect,
        client->code_challenge,
        client->state,
        encoded_scope);

    free(encoded_redirect);
    free(encoded_scope);

    printf("Opening browser for authorization...\n");

    /* Open browser */
    if (!open_browser(auth_url)) {
        set_error(client, "Failed to open browser. Please open the URL manually.");
        printf("Please open this URL in your browser:\n%s\n", auth_url);
    }

    /* Wait for callback */
    char *auth_code = wait_for_callback(client->config.callback_port,
                                         client->state,
                                         CALLBACK_TIMEOUT_SEC);

    if (!auth_code) {
        set_error(client, "Authorization timed out or was cancelled");
        return false;
    }

    printf("Received authorization code, exchanging for tokens...\n");

    /* Exchange code for tokens */
    bool success = exchange_code_for_tokens(client, auth_code);

    /* Clear auth code */
    memset(auth_code, 0, strlen(auth_code));
    free(auth_code);

    /* Clear PKCE state */
    memset(client->code_verifier, 0, strlen(client->code_verifier));
    free(client->code_verifier);
    client->code_verifier = NULL;
    free(client->code_challenge);
    client->code_challenge = NULL;
    free(client->state);
    client->state = NULL;

    return success;
}

bool oauth_refresh(oauth_client *client)
{
    if (!client || !client->tokens.refresh_token) {
        set_error(client, "No refresh token available");
        return false;
    }

    /* Build refresh request body */
    char body[1024];
    snprintf(body, sizeof(body),
        "grant_type=refresh_token"
        "&client_id=%s"
        "&refresh_token=%s",
        client->config.client_id,
        client->tokens.refresh_token);

    if (client->config.client_secret) {
        size_t body_len = strlen(body);
        size_t remaining = sizeof(body) - body_len;
        snprintf(body + body_len, remaining, "&client_secret=%s",
                 client->config.client_secret);
    }

    http_response *resp = http_post_form(client->http, OAUTH_TOKEN_URL, body);

    /* Clear body (contains refresh token) */
    memset(body, 0, sizeof(body));

    if (!resp) {
        set_error(client, "Refresh request failed");
        return false;
    }

    if (!http_response_ok(resp)) {
        char err[MAX_ERROR_MSG];
        snprintf(err, sizeof(err), "Refresh failed with status %d",
                 resp->status_code);
        set_error(client, err);
        http_response_free(resp);
        return false;
    }

    /* Parse response */
    json_value *json = json_parse(resp->body);
    http_response_free(resp);

    if (!json) {
        set_error(client, "Failed to parse refresh response");
        return false;
    }

    /* Update tokens */
    const char *access = json_get_string(json_object_get(json, "access_token"));
    const char *refresh = json_get_string(json_object_get(json, "refresh_token"));
    int expires_in = json_get_int(json_object_get(json, "expires_in"));
    int refresh_expires_in = json_get_int(json_object_get(json, "refresh_token_expires_in"));

    if (!access) {
        set_error(client, "No access_token in refresh response");
        json_free(json);
        return false;
    }

    /* Clear old tokens */
    if (client->tokens.access_token) {
        memset(client->tokens.access_token, 0, strlen(client->tokens.access_token));
        free(client->tokens.access_token);
    }
    if (client->tokens.refresh_token) {
        memset(client->tokens.refresh_token, 0, strlen(client->tokens.refresh_token));
        free(client->tokens.refresh_token);
    }

    client->tokens.access_token = strdup(access);
    client->tokens.refresh_token = refresh ? strdup(refresh) : NULL;

    time_t now = time(NULL);
    client->tokens.access_expires = now + expires_in;
    client->tokens.refresh_expires = refresh_expires_in > 0 ? now + refresh_expires_in : 0;

    json_free(json);
    return true;
}

/*
 * Token Persistence
 */

bool oauth_save_tokens(oauth_client *client, const char *filename)
{
    if (!client || !filename) return false;
    if (!client->tokens.access_token) return false;

    json_value *root = json_new_object();
    if (!root) return false;

    json_object_set(root, "access_token",
                    json_new_string(client->tokens.access_token));

    if (client->tokens.refresh_token) {
        json_object_set(root, "refresh_token",
                        json_new_string(client->tokens.refresh_token));
    }

    json_object_set(root, "token_type",
                    json_new_string(client->tokens.token_type ?
                                   client->tokens.token_type : "Bearer"));

    json_object_set(root, "access_expires",
                    json_new_number((double)client->tokens.access_expires));
    json_object_set(root, "refresh_expires",
                    json_new_number((double)client->tokens.refresh_expires));

    bool result = json_write_file(root, filename, true);
    json_free(root);
    return result;
}

bool oauth_load_tokens(oauth_client *client, const char *filename)
{
    if (!client || !filename) return false;

    json_value *root = json_parse_file(filename);
    if (!root) return false;

    const char *access = json_get_string(json_object_get(root, "access_token"));
    const char *refresh = json_get_string(json_object_get(root, "refresh_token"));
    const char *type = json_get_string(json_object_get(root, "token_type"));
    double access_exp = json_get_number(json_object_get(root, "access_expires"));
    double refresh_exp = json_get_number(json_object_get(root, "refresh_expires"));

    if (!access) {
        json_free(root);
        return false;
    }

    /* Free existing tokens */
    free(client->tokens.access_token);
    free(client->tokens.refresh_token);
    free(client->tokens.token_type);

    client->tokens.access_token = strdup(access);
    client->tokens.refresh_token = refresh ? strdup(refresh) : NULL;
    client->tokens.token_type = type ? strdup(type) : strdup("Bearer");
    client->tokens.access_expires = (time_t)access_exp;
    client->tokens.refresh_expires = (time_t)refresh_exp;

    json_free(root);
    return true;
}

/*
 * Error Handling
 */

const char *oauth_get_error(oauth_client *client)
{
    if (!client) return "Invalid client";
    if (client->last_error[0]) return client->last_error;
    return "No error";
}
