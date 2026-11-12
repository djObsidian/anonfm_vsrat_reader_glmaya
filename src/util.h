/*
 * util.h -- small helpers used by all modules of the reader.
 */

#ifndef AFR_UTIL_H
#define AFR_UTIL_H

#include <stddef.h>

/* A growable byte buffer. Always keeps room for one extra NUL byte. */
typedef struct {
    char *data;
    size_t len;
    size_t cap;
} afr_buf;

/* Set the buffer to the empty state. */
void afr_buf_init(afr_buf *b);

/* Append n bytes from s to the buffer. */
void afr_buf_add(afr_buf *b, const char *s, size_t n);

/* Append one byte to the buffer. */
void afr_buf_byte(afr_buf *b, char c);

/* Return the buffer content as a NUL terminated string and empty the buffer. */
char *afr_buf_release(afr_buf *b, size_t *out_len);

/* Release all memory of a buffer without printing its content. */
void afr_buf_free(afr_buf *b);

/* Allocate n bytes or terminate the program. */
void *afr_malloc(size_t n);

/* Resize a block or terminate the program. Accepts a NULL block. */
void *afr_realloc(void *p, size_t n);

/* Copy a NUL terminated string into a fresh block or terminate the program. */
char *afr_strdup(const char *s);

/* Compare two strings and ignore case. Return 0 when the strings are equal. */
int afr_stricmp(const char *a, const char *b);

/* Read one decimal number from a string and return its end pointer. */
long afr_parse_long(const char *s, const char **end);

/* Stop the thread of the caller for n seconds. */
void afr_sleep(int seconds);

/* A bounded printf for a string result. C89 has no snprintf, but POSIX
 * and every compiler of today declare it. */
#if defined(_MSC_VER) && _MSC_VER < 1900
#define afr_snprintf _snprintf
#elif defined(_WIN32)
/* MinGW declares snprintf itself, so no own declaration is needed. */
#define afr_snprintf snprintf
#else
extern int snprintf(char *s, size_t n, const char *fmt, ...);
#define afr_snprintf snprintf
#endif

#endif
