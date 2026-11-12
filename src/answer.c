/*
 * answer.c -- parse the kusyareshka feed of the Radio Anonimus.
 */

#include "answer.h"
#include "util.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* ------------------------------------------------------------------ */
/* JSON parser for the shape of the feed                              */
/* ------------------------------------------------------------------ */

typedef struct {
    const char *cur; /* cursor into the JSON text */
    const char *err; /* parse error, or NULL */
} afr_json;

static void json_ws(afr_json *j)
{
    while (*j->cur == ' ' || *j->cur == '\t' || *j->cur == '\n'
           || *j->cur == '\r') {
        j->cur++;
    }
}

static void json_fail(afr_json *j, const char *reason)
{
    if (j->err == NULL) {
        j->err = reason;
    }
}

static int json_expect(afr_json *j, char c)
{
    json_ws(j);
    if (*j->cur != c) {
        json_fail(j, "unexpected character in the feed");
        return 0;
    }
    j->cur++;
    return 1;
}

/* Encode one Unicode code point as UTF-8 into the buffer. */
static void json_utf8(afr_buf *b, unsigned long cp)
{
    if (cp < 0x80) {
        afr_buf_byte(b, (char)cp);
    } else if (cp < 0x800) {
        afr_buf_byte(b, (char)(0xC0 | (cp >> 6)));
        afr_buf_byte(b, (char)(0x80 | (cp & 0x3F)));
    } else if (cp < 0x10000) {
        afr_buf_byte(b, (char)(0xE0 | (cp >> 12)));
        afr_buf_byte(b, (char)(0x80 | ((cp >> 6) & 0x3F)));
        afr_buf_byte(b, (char)(0x80 | (cp & 0x3F)));
    } else {
        afr_buf_byte(b, (char)(0xF0 | (cp >> 18)));
        afr_buf_byte(b, (char)(0x80 | ((cp >> 12) & 0x3F)));
        afr_buf_byte(b, (char)(0x80 | ((cp >> 6) & 0x3F)));
        afr_buf_byte(b, (char)(0x80 | (cp & 0x3F)));
    }
}

static int json_hex4(const char **pp, unsigned long *out)
{
    unsigned long v;
    int i;

    v = 0;
    for (i = 0; i < 4; i++) {
        char c = **pp;
        unsigned long d;

        if (c >= '0' && c <= '9') {
            d = (unsigned long)(c - '0');
        } else if (c >= 'a' && c <= 'f') {
            d = (unsigned long)(c - 'a') + 10;
        } else if (c >= 'A' && c <= 'F') {
            d = (unsigned long)(c - 'A') + 10;
        } else {
            return 0;
        }
        v = v * 16 + d;
        (*pp)++;
    }
    *out = v;
    return 1;
}

/* Parse one JSON string with escapes and return it as a fresh string. */
static int json_string(afr_json *j, char **out)
{
    afr_buf b;

    json_ws(j);
    if (*j->cur != '"') {
        json_fail(j, "expected a string in the feed");
        return 0;
    }
    j->cur++;
    afr_buf_init(&b);
    while (*j->cur != '"' && *j->cur != '\0') {
        char c = *j->cur++;

        if (c != '\\') {
            afr_buf_byte(&b, c);
            continue;
        }
        c = *j->cur++;
        switch (c) {
        case '"':
            afr_buf_byte(&b, '"');
            break;
        case '\\':
            afr_buf_byte(&b, '\\');
            break;
        case '/':
            afr_buf_byte(&b, '/');
            break;
        case 'b':
            afr_buf_byte(&b, '\b');
            break;
        case 'f':
            afr_buf_byte(&b, '\f');
            break;
        case 'n':
            afr_buf_byte(&b, '\n');
            break;
        case 'r':
            afr_buf_byte(&b, '\r');
            break;
        case 't':
            afr_buf_byte(&b, '\t');
            break;
        case 'u': {
            unsigned long cp;

            if (!json_hex4(&j->cur, &cp)) {
                afr_buf_free(&b);
                json_fail(j, "bad \\u escape in the feed");
                return 0;
            }
            /* Join a surrogate pair when the code point is a high surrogate. */
            if (cp >= 0xD800 && cp <= 0xDBFF
                && j->cur[0] == '\\' && j->cur[1] == 'u') {
                const char *save = j->cur;
                unsigned long lo;

                j->cur += 2;
                if (json_hex4(&j->cur, &lo) && lo >= 0xDC00 && lo <= 0xDFFF) {
                    cp = 0x10000 + ((cp - 0xD800) << 10) + (lo - 0xDC00);
                } else {
                    j->cur = save;
                    cp = 0xFFFD;
                }
            } else if (cp >= 0xD800 && cp <= 0xDFFF) {
                cp = 0xFFFD;
            }
            json_utf8(&b, cp);
            break;
        }
        case '\0':
            afr_buf_free(&b);
            json_fail(j, "unterminated string in the feed");
            return 0;
        default:
            afr_buf_free(&b);
            json_fail(j, "unknown escape in the feed");
            return 0;
        }
    }
    if (*j->cur != '"') {
        afr_buf_free(&b);
        json_fail(j, "unterminated string in the feed");
        return 0;
    }
    j->cur++;
    *out = afr_buf_release(&b, NULL);
    return 1;
}

/* ------------------------------------------------------------------ */
/* Timestamp extraction                                               */
/* ------------------------------------------------------------------ */

/* Extract the HH:MM:SS part from the timestamp HTML field. */
static char *answer_time_from_html(const char *html)
{
    const char *p = html;
    char buf[9];
    int n = 0;

    while (*p != '>' && *p != '\0') {
        p++;
    }
    if (*p == '>') {
        p++;
    }
    while (*p != '\0' && n < 8 && (isdigit((unsigned char)*p) || *p == ':')) {
        buf[n++] = *p++;
    }
    buf[n] = '\0';
    return afr_strdup(buf);
}

/* ------------------------------------------------------------------ */
/* HTML stripping                                                     */
/* ------------------------------------------------------------------ */

static void html_utf8(afr_buf *b, unsigned long cp)
{
    if (cp > 0x10FFFF) {
        cp = 0xFFFD;
    }
    json_utf8(b, cp);
}

/* Decode one entity and advance the cursor past it. */
static void html_entity(afr_buf *b, const char **pp)
{
    const char *p = *pp + 1; /* skip the '&' */
    char name[12];
    int n = 0;

    while (*p != '\0' && *p != ';' && (isalnum((unsigned char)*p) || *p == '#')
           && n < 11) {
        name[n++] = *p++;
    }
    name[n] = '\0';
    if (*p == ';' && n > 0) {
        p++;
        if (name[0] == '#') {
            unsigned long cp = 0;

            if (name[1] == 'x' || name[1] == 'X') {
                const char *hex = name + 2;
                int shift = 0;
                int i;

                for (i = (int)strlen(hex) - 1; i >= 0; i--) {
                    char c = hex[i];
                    unsigned long d;

                    if (c >= '0' && c <= '9') {
                        d = (unsigned long)(c - '0');
                    } else if (c >= 'a' && c <= 'f') {
                        d = (unsigned long)(c - 'a') + 10;
                    } else {
                        d = (unsigned long)(c - 'A') + 10;
                    }
                    cp += d << shift;
                    shift += 4;
                }
            } else {
                const char *dec = name + 1;

                while (*dec != '\0') {
                    cp = cp * 10 + (unsigned long)(*dec - '0');
                    dec++;
                }
            }
            html_utf8(b, cp);
        } else if (strcmp(name, "quot") == 0) {
            afr_buf_byte(b, '"');
        } else if (strcmp(name, "amp") == 0) {
            afr_buf_byte(b, '&');
        } else if (strcmp(name, "apos") == 0) {
            afr_buf_byte(b, '\'');
        } else if (strcmp(name, "lt") == 0) {
            afr_buf_byte(b, '<');
        } else if (strcmp(name, "gt") == 0) {
            afr_buf_byte(b, '>');
        } else if (strcmp(name, "nbsp") == 0) {
            afr_buf_byte(b, ' ');
        } else if (strcmp(name, "ndash") == 0) {
            afr_buf_add(b, "\xE2\x80\x93", 3);
        } else if (strcmp(name, "mdash") == 0) {
            afr_buf_add(b, "\xE2\x80\x94", 3);
        } else if (strcmp(name, "hellip") == 0) {
            afr_buf_add(b, "\xE2\x80\xA6", 3);
        } else {
            /* Unknown entity: keep the raw text. */
            afr_buf_byte(b, '&');
            afr_buf_add(b, name, (size_t)n);
            afr_buf_byte(b, ';');
        }
        *pp = p;
    } else {
        /* Not a valid entity: emit the '&' and keep scanning. */
        afr_buf_byte(b, '&');
        *pp = *pp + 1;
    }
}

/* Convert a text with HTML markup to plain text. Returns a fresh string. */
static char *answer_html_to_text(const char *s);

/* Check whether q starts with the attribute name href. */
static int html_is_href(const char *q)
{
    return (q[0] == 'h' || q[0] == 'H')
           && (q[1] == 'r' || q[1] == 'R')
           && (q[2] == 'e' || q[2] == 'E')
           && (q[3] == 'f' || q[3] == 'F');
}

/* Skip an HTML tag and emit its href value when the tag is a link. */
static void html_tag(afr_buf *b, const char **pp)
{
    const char *p = *pp;
    const char *end;

    if (p[0] == '<' && p[1] == '/') {
        /* A closing tag is dropped. */
        end = strchr(p, '>');
        *pp = end == NULL ? p + strlen(p) : end + 1;
        return;
    }
    if (p[0] == '<' && (p[1] == 'a' || p[1] == 'A')
        && (p[2] == ' ' || p[2] == '>')) {
        /* Print the target of the link instead of the tagged text. */
        const char *q;
        const char *href = NULL;
        const char *href_end = NULL;

        end = strchr(p, '>');
        if (end == NULL) {
            *pp = p + strlen(p);
            return;
        }
        for (q = p + 1; q < end && href == NULL; ) {
            const char *val;

            if (!html_is_href(q)) {
                q = strchr(q, ' ');
                if (q != NULL) {
                    q++;
                }
                continue;
            }
            val = q + 4;
            while (*val == ' ' || *val == '\t') {
                val++;
            }
            if (*val != '=') {
                q = strchr(q, ' ');
                if (q != NULL) {
                    q++;
                }
                continue;
            }
            val++;
            if (*val == '"' || *val == '\'') {
                char quote = *val++;
                const char *close = strchr(val, quote);

                if (close != NULL && close <= end) {
                    href = val;
                    href_end = close;
                }
            }
        }
        if (href != NULL) {
            /* Decode the entities inside the target of the link too. */
            size_t len = (size_t)(href_end - href);
            char *copy = (char *)afr_malloc(len + 1);
            char *plain;

            memcpy(copy, href, len);
            copy[len] = '\0';
            plain = answer_html_to_text(copy);
            afr_buf_add(b, plain, strlen(plain));
            free(plain);
            free(copy);
        }
        /* Skip the tagged text and stop at the closing anchor tag. */
        {
            const char *body = end + 1;
            const char *closing = body;

            while (*closing != '\0') {
                if (closing[0] == '<' && closing[1] == '/'
                    && (closing[2] == 'a' || closing[2] == 'A')) {
                    break;
                }
                closing++;
            }
            if (*closing == '\0') {
                *pp = end + 1; /* No closing tag: print the inner text. */
                return;
            }
            end = strchr(closing, '>');
            *pp = end == NULL ? closing + strlen(closing) : end + 1;
        }
        return;
    }
    /* Drop every other tag, but turn <br> and <br/> into a line break. */
    if ((p[1] == 'b' || p[1] == 'B') && (p[2] == 'r' || p[2] == 'R')) {
        afr_buf_byte(b, '\n');
    }
    end = strchr(p, '>');
    *pp = end == NULL ? p + strlen(p) : end + 1;
}

/* Convert a text with HTML markup to plain text. Returns a fresh string. */
static char *answer_html_to_text(const char *s)
{
    afr_buf b;
    const char *p = s;

    afr_buf_init(&b);
    while (*p != '\0') {
        if (*p == '<') {
            html_tag(&b, &p);
        } else if (*p == '&') {
            html_entity(&b, &p);
        } else {
            afr_buf_byte(&b, *p++);
        }
    }
    return afr_buf_release(&b, NULL);
}

/* ------------------------------------------------------------------ */
/* Feed parsing                                                       */
/* ------------------------------------------------------------------ */

#define AFR_FIELDS 7

int afr_answers_parse(const char *body, afr_answer **out, const char **err)
{
    afr_json j;
    afr_answer *arr = NULL;
    int n = 0;
    int cap = 0;

    j.cur = body;
    j.err = NULL;
    if (!json_expect(&j, '[')) {
        goto fail;
    }
    json_ws(&j);
    if (*j.cur == ']') {
        j.cur++;
        *out = NULL;
        *err = NULL;
        return 0;
    }
    for (;;) {
        char *fields[AFR_FIELDS];
        int i;
        afr_answer a;
        int ok = 1;

        for (i = 0; i < AFR_FIELDS; i++) {
            fields[i] = NULL;
        }
        if (!json_expect(&j, '[')) {
            ok = 0;
        }
        for (i = 0; ok && i < AFR_FIELDS; i++) {
            if (!json_string(&j, &fields[i])) {
                ok = 0;
                break;
            }
            json_ws(&j);
            if (*j.cur == ',') {
                j.cur++;
            }
        }
        if (!ok) {
            int k;

            for (k = 0; k < AFR_FIELDS; k++) {
                free(fields[k]);
            }
            goto fail;
        }
        /* Tolerate extra fields of a future feed format. */
        json_ws(&j);
        while (*j.cur == ',') {
            char *extra = NULL;

            j.cur++;
            if (!json_string(&j, &extra)) {
                free(extra);
                ok = 0;
                break;
            }
            free(extra);
            json_ws(&j);
        }
        if (!json_expect(&j, ']')) {
            ok = 0;
        }
        if (!ok) {
            int k;

            for (k = 0; k < AFR_FIELDS; k++) {
                free(fields[k]);
            }
            goto fail;
        }

        a.is_dj = fields[0][0] == '1';
        a.listener = fields[1];
        a.message = answer_html_to_text(fields[2] != NULL ? fields[2] : "");
        a.time = answer_time_from_html(fields[3] != NULL ? fields[3] : "");
        a.dj = fields[4];
        a.response = answer_html_to_text(fields[5] != NULL ? fields[5] : "");
        a.persona = fields[6];

        free(fields[0]);
        free(fields[2]);
        free(fields[3]);

        if (n == cap) {
            cap = cap == 0 ? 16 : cap * 2;
            arr = (afr_answer *)afr_realloc(arr, (size_t)cap * sizeof(arr[0]));
        }
        arr[n++] = a;

        json_ws(&j);
        if (*j.cur == ',') {
            j.cur++;
            continue;
        }
        if (*j.cur == ']') {
            j.cur++;
            break;
        }
        json_fail(&j, "expected ',' or ']' in the feed");
        goto fail;
    }
    *out = arr;
    *err = NULL;
    return n;

fail:
    if (arr != NULL) {
        afr_answers_free(arr, n);
    }
    *out = NULL;
    *err = j.err != NULL ? j.err : "bad feed body";
    return -1;
}

void afr_answers_free(afr_answer *arr, int n)
{
    int i;

    for (i = 0; i < n; i++) {
        free(arr[i].listener);
        free(arr[i].message);
        free(arr[i].time);
        free(arr[i].dj);
        free(arr[i].response);
        free(arr[i].persona);
    }
    free(arr);
}

unsigned long afr_answer_hash(const afr_answer *a)
{
    unsigned long h = 2166136261UL;
    const char *parts[5];
    int i;

    parts[0] = a->listener != NULL ? a->listener : "";
    parts[1] = a->message != NULL ? a->message : "";
    parts[2] = a->time != NULL ? a->time : "";
    parts[3] = a->response != NULL ? a->response : "";
    parts[4] = a->is_dj ? "1" : "0";
    for (i = 0; i < 5; i++) {
        const char *p = parts[i];

        while (*p != '\0') {
            h ^= (unsigned char)*p++;
            h *= 16777619UL;
        }
        h ^= (unsigned char)0x1F;
        h *= 16777619UL;
    }
    return h;
}
