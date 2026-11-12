/*
 * util.c -- small helpers used by all modules of the reader.
 */

#include "util.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

static void afr_buf_grow(afr_buf *b, size_t need)
{
    size_t cap;

    if (need <= b->cap) {
        return;
    }
    cap = b->cap == 0 ? 64 : b->cap;
    while (cap < need + 1) {
        cap *= 2;
    }
    b->data = (char *)afr_realloc(b->data, cap);
    b->cap = cap;
}

void afr_buf_init(afr_buf *b)
{
    b->data = NULL;
    b->len = 0;
    b->cap = 0;
}

void afr_buf_add(afr_buf *b, const char *s, size_t n)
{
    if (n == 0) {
        return;
    }
    afr_buf_grow(b, b->len + n);
    memcpy(b->data + b->len, s, n);
    b->len += n;
    b->data[b->len] = '\0';
}

void afr_buf_byte(afr_buf *b, char c)
{
    afr_buf_add(b, &c, 1);
}

char *afr_buf_release(afr_buf *b, size_t *out_len)
{
    char *s;

    if (b->data == NULL) {
        s = afr_strdup("");
    } else {
        s = b->data;
    }
    if (out_len != NULL) {
        *out_len = b->len;
    }
    b->data = NULL;
    b->len = 0;
    b->cap = 0;
    return s;
}

void afr_buf_free(afr_buf *b)
{
    if (b->data != NULL) {
        free(b->data);
    }
    b->data = NULL;
    b->len = 0;
    b->cap = 0;
}

void *afr_malloc(size_t n)
{
    void *p;

    p = malloc(n);
    if (p == NULL && n != 0) {
        fprintf(stderr, "anonfm-reader: no memory for %lu bytes\n",
                (unsigned long)n);
        exit(1);
    }
    return p;
}

void *afr_realloc(void *p, size_t n)
{
    void *q;

    q = realloc(p, n);
    if (q == NULL && n != 0) {
        fprintf(stderr, "anonfm-reader: no memory for %lu bytes\n",
                (unsigned long)n);
        exit(1);
    }
    return q;
}

char *afr_strdup(const char *s)
{
    size_t n;
    char *copy;

    n = strlen(s);
    copy = (char *)afr_malloc(n + 1);
    memcpy(copy, s, n + 1);
    return copy;
}

int afr_stricmp(const char *a, const char *b)
{
    unsigned char ca;
    unsigned char cb;

    do {
        ca = (unsigned char)*a++;
        cb = (unsigned char)*b++;
        ca = (unsigned char)tolower(ca);
        cb = (unsigned char)tolower(cb);
    } while (ca == cb && ca != '\0');
    return ca - cb;
}

long afr_parse_long(const char *s, const char **end)
{
    long v;
    int sign;

    v = 0;
    sign = 1;
    while (*s == ' ' || *s == '\t') {
        s++;
    }
    if (*s == '-') {
        sign = -1;
        s++;
    }
    while (isdigit((unsigned char)*s)) {
        v = v * 10 + (*s - '0');
        s++;
    }
    if (end != NULL) {
        *end = s;
    }
    return v * sign;
}

void afr_sleep(int seconds)
{
#ifdef _WIN32
    Sleep((DWORD)seconds * 1000);
#else
    sleep((unsigned int)seconds);
#endif
}
