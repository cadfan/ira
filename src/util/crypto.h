/*
 * ira - iRacing Application
 * Crypto Utilities (Windows BCrypt)
 *
 * Copyright (c) 2026 Christopher Griffiths
 */

#ifndef IRA_CRYPTO_H
#define IRA_CRYPTO_H

#include <stddef.h>

/*
 * Compute SHA256 hash of data.
 *
 * Returns a newly allocated 32-byte buffer containing the hash.
 * Caller must free the returned pointer.
 * Returns NULL on error.
 */
unsigned char *crypto_sha256(const void *data, size_t len);

/*
 * Base64 encode binary data.
 *
 * Returns a newly allocated null-terminated string.
 * Caller must free the returned pointer.
 * Returns NULL on error.
 */
char *crypto_base64_encode(const unsigned char *data, size_t len);

/*
 * Compute iRacing password hash.
 *
 * The hash is: Base64(SHA256(password + lowercase(email)))
 *
 * Returns a newly allocated null-terminated string.
 * Caller must free the returned pointer.
 * Returns NULL on error.
 */
char *crypto_iracing_password_hash(const char *password, const char *email);

#endif /* IRA_CRYPTO_H */
