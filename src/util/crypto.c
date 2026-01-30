/*
 * ira - iRacing Application
 * Crypto Utilities Implementation (Windows BCrypt)
 *
 * Copyright (c) 2026 Christopher Griffiths
 */

#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <bcrypt.h>
#include <wincrypt.h>

#include "crypto.h"

#define SHA256_HASH_SIZE 32

/*
 * Compute SHA256 hash using BCrypt
 */
unsigned char *crypto_sha256(const void *data, size_t len)
{
    if (!data || len == 0) return NULL;

    BCRYPT_ALG_HANDLE alg = NULL;
    BCRYPT_HASH_HANDLE hash = NULL;
    unsigned char *result = NULL;
    NTSTATUS status;

    /* Open algorithm provider */
    status = BCryptOpenAlgorithmProvider(&alg, BCRYPT_SHA256_ALGORITHM, NULL, 0);
    if (!BCRYPT_SUCCESS(status)) {
        goto cleanup;
    }

    /* Create hash object */
    status = BCryptCreateHash(alg, &hash, NULL, 0, NULL, 0, 0);
    if (!BCRYPT_SUCCESS(status)) {
        goto cleanup;
    }

    /* Hash the data */
    status = BCryptHashData(hash, (PUCHAR)data, (ULONG)len, 0);
    if (!BCRYPT_SUCCESS(status)) {
        goto cleanup;
    }

    /* Allocate result buffer */
    result = malloc(SHA256_HASH_SIZE);
    if (!result) {
        goto cleanup;
    }

    /* Get the hash */
    status = BCryptFinishHash(hash, result, SHA256_HASH_SIZE, 0);
    if (!BCRYPT_SUCCESS(status)) {
        free(result);
        result = NULL;
        goto cleanup;
    }

cleanup:
    if (hash) BCryptDestroyHash(hash);
    if (alg) BCryptCloseAlgorithmProvider(alg, 0);

    return result;
}

/*
 * Base64 encode using Windows CryptBinaryToStringA
 */
char *crypto_base64_encode(const unsigned char *data, size_t len)
{
    if (!data || len == 0) return NULL;

    DWORD encoded_size = 0;

    /* Get required size */
    if (!CryptBinaryToStringA(data, (DWORD)len,
                              CRYPT_STRING_BASE64 | CRYPT_STRING_NOCRLF,
                              NULL, &encoded_size)) {
        return NULL;
    }

    /* Allocate buffer */
    char *encoded = malloc(encoded_size);
    if (!encoded) return NULL;

    /* Encode */
    if (!CryptBinaryToStringA(data, (DWORD)len,
                              CRYPT_STRING_BASE64 | CRYPT_STRING_NOCRLF,
                              encoded, &encoded_size)) {
        free(encoded);
        return NULL;
    }

    return encoded;
}

/*
 * Compute iRacing password hash
 *
 * Algorithm:
 * 1. Normalize email (trim whitespace, convert to lowercase)
 * 2. Concatenate: password + normalized_email
 * 3. SHA256 hash the concatenation
 * 4. Base64 encode the hash
 */
char *crypto_iracing_password_hash(const char *password, const char *email)
{
    if (!password || !email) return NULL;

    size_t pass_len = strlen(password);
    size_t email_len = strlen(email);

    /* Allocate buffer for: password + lowercase(email) */
    char *concat = malloc(pass_len + email_len + 1);
    if (!concat) return NULL;

    /* Copy password */
    memcpy(concat, password, pass_len);

    /* Copy email and convert to lowercase */
    for (size_t i = 0; i < email_len; i++) {
        concat[pass_len + i] = (char)tolower((unsigned char)email[i]);
    }
    concat[pass_len + email_len] = '\0';

    /* Compute SHA256 */
    unsigned char *hash = crypto_sha256(concat, pass_len + email_len);

    /* Clear and free the concatenation (contains password) */
    memset(concat, 0, pass_len + email_len);
    free(concat);

    if (!hash) return NULL;

    /* Base64 encode the hash */
    char *encoded = crypto_base64_encode(hash, SHA256_HASH_SIZE);

    /* Clear and free the hash */
    memset(hash, 0, SHA256_HASH_SIZE);
    free(hash);

    return encoded;
}
