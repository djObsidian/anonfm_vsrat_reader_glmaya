/*
 * main.c -- console reader of the kusyareshka of the Radio Anonimus.
 *
 * The reader gets https://anon.fm/answers.js and prints every exchange
 * of the listeners and the DJ. The reader supports two modes:
 *
 *   auto  poll the feed and print new rows (the default)
 *   once  print the last n rows and exit
 *
 * Every nick gets one unique color. The color comes from a hash of the
 * nick, so the color stays stable between the runs.
 */

#include "answer.h"
#include "color.h"
#include "net.h"
#include "util.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define AFR_HOST "anon.fm"
#define AFR_PATH "/answers.js"
#define AFR_ERR_SIZE 256

/* State of the seen marks of the auto mode. */
typedef struct {
    unsigned long *items;
    int n;
    int cap;
} afr_seen;

static const char *afr_prog_name = "anonfm";

/* ------------------------------------------------------------------ */
/* Output of one answer                                               */
/* ------------------------------------------------------------------ */

/* Print a multi line text with two leading spaces on every line. */
static void print_body(const char *text)
{
    const char *p = text;
    const char *line = text;

    while (*p != '\0') {
        if (*p == '\n') {
            fputs("  ", stdout);
            fwrite(line, 1, (size_t)(p - line), stdout);
            fputc('\n', stdout);
            p++;
            line = p;
        } else {
            p++;
        }
    }
    if (line != p && *line != '\0') {
        fputs("  ", stdout);
        fputs(line, stdout);
        fputc('\n', stdout);
    }
}

/*
 * Print the header "nick => target:". When target is NULL the row is a
 * message of the DJ to the whole chat, so print the group name instead.
 */
static void print_header(const char *time, const char *who, const char *persona,
                         const char *target, const char *target_persona)
{
    afr_print_dim(time);
    fputs("  ", stdout);
    if (afr_no_color) {
        fputs(who, stdout);
    } else {
        afr_print_nick(who);
    }
    if (persona != NULL && persona[0] != '\0') {
        fputs(" (", stdout);
        afr_print_dim(persona);
        fputs(")", stdout);
    }
    fputs(" => ", stdout);
    if (target == NULL) {
        if (!afr_no_color) {
            fputs("\x1b[1m", stdout);
        }
        fputs("\xD1\x81\xD0\xBB\xD1\x83\xD1\x88\xD0\xB0\xD1\x82\xD0\xB5\xD0\xBB"
              "\xD0\xB8", stdout);
        if (!afr_no_color) {
            fputs("\x1b[0m", stdout);
        }
    } else {
        afr_print_nick(target);
    }
    if (target_persona != NULL && target_persona[0] != '\0') {
        fputs(" (", stdout);
        afr_print_dim(target_persona);
        fputs(")", stdout);
    }
    fputs(":\n", stdout);
}

/* Print one exchange of the feed. */
static void print_answer(const afr_answer *a)
{
    const char *dj = a->dj != NULL ? a->dj : "";
    const char *persona = a->persona != NULL ? a->persona : "";
    const char *listener = a->listener != NULL ? a->listener : "";

    if (!a->is_dj && a->message != NULL && a->message[0] != '\0') {
        print_header(a->time, listener, NULL, dj, persona);
        print_body(a->message);
        fputs("\n", stdout);
        print_header(a->time, dj, persona, listener, NULL);
        print_body(a->response != NULL ? a->response : "");
        fputc('\n', stdout);
    } else if (a->is_dj && a->response != NULL && a->response[0] != '\0') {
        print_header(a->time, dj, persona, NULL, NULL);
        print_body(a->response);
        fputc('\n', stdout);
    }
}

/* ------------------------------------------------------------------ */
/* The seen marks of the auto mode                                    */
/* ------------------------------------------------------------------ */

static void seen_init(afr_seen *s)
{
    s->cap = 1024;
    s->n = 0;
    s->items = (unsigned long *)afr_malloc((size_t)s->cap
                                           * sizeof(s->items[0]));
}

static int seen_has(const afr_seen *s, unsigned long h)
{
    int i;

    for (i = 0; i < s->n; i++) {
        if (s->items[i] == h) {
            return 1;
        }
    }
    return 0;
}

static void seen_add(afr_seen *s, unsigned long h)
{
    if (seen_has(s, h)) {
        return;
    }
    if (s->n == s->cap) {
        /* Drop the oldest marks when the set is full. */
        int keep = s->cap / 2;

        memmove(s->items, s->items + (s->cap - keep),
                (size_t)keep * sizeof(s->items[0]));
        s->n = keep;
    }
    s->items[s->n++] = h;
}

static void seen_free(afr_seen *s)
{
    free(s->items);
    s->items = NULL;
    s->n = 0;
    s->cap = 0;
}

/* ------------------------------------------------------------------ */
/* The modes of the reader                                            */
/* ------------------------------------------------------------------ */

static int feed_get(char *err, size_t errsz, const char *ca_file,
                    afr_answer **arr, int *n)
{
    char *body = NULL;
    const char *why = NULL;
    int count;

    *arr = NULL;
    *n = 0;
    if (afr_net_fetch(AFR_HOST, AFR_PATH, ca_file, &body, err, errsz) != 0) {
        return -1;
    }
    count = afr_answers_parse(body, arr, &why);
    free(body);
    if (count < 0) {
        afr_snprintf(err, errsz, "cannot parse the feed: %s",
                     why != NULL ? why : "unknown error");
        return -1;
    }
    *n = count;
    return 0;
}

static int run_once(int count, const char *ca_file)
{
    char err[AFR_ERR_SIZE];
    afr_answer *arr = NULL;
    int n = 0;
    int i;

    if (feed_get(err, sizeof(err), ca_file, &arr, &n) != 0) {
        fprintf(stderr, "anonfm-reader: %s\n", err);
        return 1;
    }
    if (count > n) {
        count = n;
    }
    /* The array holds the newest row first, so print it backwards. */
    for (i = count - 1; i >= 0; i--) {
        print_answer(&arr[i]);
    }
    afr_answers_free(arr, n);
    return 0;
}

static int run_auto(int interval, int history, const char *ca_file)
{
    char err[AFR_ERR_SIZE];
    afr_seen seen;
    afr_answer *arr = NULL;
    int n = 0;
    int i;
    int first = 1;

    seen_init(&seen);
    for (;;) {
        if (feed_get(err, sizeof(err), ca_file, &arr, &n) != 0) {
            fprintf(stderr, "anonfm-reader: %s\n", err);
            arr = NULL;
            n = 0;
            if (first) {
                seen_free(&seen);
                return 1;
            }
        } else if (first) {
            /* Show the last rows and mark all rows as seen. */
            int shown = history < n ? history : n;

            for (i = 0; i < n; i++) {
                seen_add(&seen, afr_answer_hash(&arr[i]));
            }
            for (i = shown - 1; i >= 0; i--) {
                print_answer(&arr[i]);
            }
            first = 0;
        } else {
            /* Print every new row in the order from old to new. */
            for (i = n - 1; i >= 0; i--) {
                unsigned long h = afr_answer_hash(&arr[i]);

                if (!seen_has(&seen, h)) {
                    seen_add(&seen, h);
                    print_answer(&arr[i]);
                }
            }
        }
        if (arr != NULL) {
            afr_answers_free(arr, n);
            arr = NULL;
            n = 0;
        }
        fflush(stdout);
        afr_sleep(interval);
    }
}

/* ------------------------------------------------------------------ */
/* Command line                                                       */
/* ------------------------------------------------------------------ */

static void usage(FILE *f)
{
    fprintf(f, "usage: %s [auto] [options]\n", afr_prog_name);
    fprintf(f, "       %s once [count] [options]\n", afr_prog_name);
    fprintf(f, "\n");
    fprintf(f, "Console reader of the kusyareshka of the Radio Anonimus.\n");
    fprintf(f, "\n");
    fprintf(f, "modes:\n");
    fprintf(f, "  auto          poll the feed and print new rows"
                " (the default)\n");
    fprintf(f, "  once          print the last rows and exit\n");
    fprintf(f, "\n");
    fprintf(f, "options:\n");
    fprintf(f, "  -n N          row count for the once mode and the history"
                " of the auto mode\n");
    fprintf(f, "                (default 20 for once, 10 for auto)\n");
    fprintf(f, "  -i SEC        seconds between the polls of the auto mode"
                " (default 10)\n");
    fprintf(f, "  -c            print without colors\n");
    fprintf(f, "  -C FILE       PEM bundle to verify the server certificate\n");
    fprintf(f, "  -h            print this help\n");
}

static int is_number(const char *s)
{
    if (*s == '\0') {
        return 0;
    }
    while (*s != '\0') {
        if (*s < '0' || *s > '9') {
            return 0;
        }
        s++;
    }
    return 1;
}

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

/* Switch the console of Windows to UTF-8 and ANSI colors. */
static void console_setup(void)
{
    HANDLE h;
    DWORD mode;

    SetConsoleOutputCP(65001);
    h = GetStdHandle(STD_OUTPUT_HANDLE);
    if (h != INVALID_HANDLE_VALUE && GetConsoleMode(h, &mode)) {
        mode |= 0x0004; /* ENABLE_VIRTUAL_TERMINAL_PROCESSING */
        if (!SetConsoleMode(h, mode)) {
            afr_no_color = 1;
        }
    } else {
        afr_no_color = 1;
    }
}
#endif

int main(int argc, char **argv)
{
    int i;
    int mode_once = 0;
    int count = 0;
    int interval = 10;
    const char *ca_file = NULL;
    int status;

    if (argc > 0 && argv[0] != NULL && argv[0][0] != '\0') {
        afr_prog_name = argv[0];
    }
#ifdef _WIN32
    console_setup();
#endif
    for (i = 1; i < argc; i++) {
        const char *a = argv[i];

        if (a[0] == '-' && a[1] != '\0') {
            switch (a[1]) {
            case 'n':
                if (i + 1 >= argc || !is_number(argv[i + 1])) {
                    fprintf(stderr, "anonfm-reader: -n needs a number\n");
                    usage(stderr);
                    return 2;
                }
                count = atoi(argv[++i]);
                break;
            case 'i':
                if (i + 1 >= argc || !is_number(argv[i + 1])) {
                    fprintf(stderr, "anonfm-reader: -i needs a number\n");
                    usage(stderr);
                    return 2;
                }
                interval = atoi(argv[++i]);
                break;
            case 'c':
                afr_no_color = 1;
                break;
            case 'C':
                if (i + 1 >= argc) {
                    fprintf(stderr, "anonfm-reader: -C needs a file path\n");
                    usage(stderr);
                    return 2;
                }
                ca_file = argv[++i];
                break;
            case 'h':
                usage(stdout);
                return 0;
            default:
                fprintf(stderr, "anonfm-reader: unknown option %s\n", a);
                usage(stderr);
                return 2;
            }
        } else if (strcmp(a, "once") == 0) {
            mode_once = 1;
        } else if (strcmp(a, "auto") == 0) {
            mode_once = 0;
        } else if (is_number(a)) {
            /* A bare number sets the row count of the chosen mode. */
            count = atoi(a);
        } else {
            fprintf(stderr, "anonfm-reader: unknown argument %s\n", a);
            usage(stderr);
            return 2;
        }
    }

    if (mode_once) {
        status = run_once(count > 0 ? count : 20, ca_file);
    } else {
        status = run_auto(interval > 0 ? interval : 10,
                          count > 0 ? count : 10, ca_file);
    }
    return status;
}
