/*
 * color.c -- unique colors for the nicks of the kusyareshka.
 */

#include "color.h"

#include <stdio.h>
#include <string.h>
#include <math.h>

int afr_no_color = 0;

#define AFR_NICK_SATURATION 0.55
#define AFR_NICK_VALUE 0.80

/* FNV-1a hash of a NUL terminated string. */
static unsigned long nick_hash(const char *s)
{
    unsigned long h = 2166136261UL;

    while (*s != '\0') {
        h ^= (unsigned char)*s++;
        h *= 16777619UL;
    }
    return h;
}

void afr_nick_rgb(const char *nick, int *r, int *g, int *b)
{
    double hue = (double)(nick_hash(nick) % 360);
    double s = AFR_NICK_SATURATION;
    double v = AFR_NICK_VALUE;
    double c;
    double x;
    double m;
    double rr;
    double rg;
    double rb;
    int sector;
    double hp;
    double f;

    c = v * s;
    hp = hue / 60.0;
    sector = (int)hp;
    f = hp - (double)sector;
    x = c * (1.0 - fabs(2.0 * f - 1.0));
    m = v - c;
    switch (sector) {
    case 0:
        rr = c; rg = x; rb = 0.0;
        break;
    case 1:
        rr = x; rg = c; rb = 0.0;
        break;
    case 2:
        rr = 0.0; rg = c; rb = x;
        break;
    case 3:
        rr = 0.0; rg = x; rb = c;
        break;
    case 4:
        rr = x; rg = 0.0; rb = c;
        break;
    default:
        rr = c; rg = 0.0; rb = x;
        break;
    }
    *r = (int)((rr + m) * 255.0 + 0.5);
    *g = (int)((rg + m) * 255.0 + 0.5);
    *b = (int)((rb + m) * 255.0 + 0.5);
}

void afr_print_nick(const char *nick)
{
    int r;
    int g;
    int b;

    if (afr_no_color) {
        fputs(nick, stdout);
        return;
    }
    afr_nick_rgb(nick, &r, &g, &b);
    printf("\x1b[38;2;%d;%d;%dm%s\x1b[0m", r, g, b, nick);
}

void afr_print_nick_plain(const char *nick)
{
    fputs(nick, stdout);
}

void afr_print_dim(const char *s)
{
    if (afr_no_color) {
        fputs(s, stdout);
        return;
    }
    printf("\x1b[90m%s\x1b[0m", s);
}

void afr_print_person(const char *nick, const char *persona)
{
    afr_print_nick(nick);
    if (persona != NULL && persona[0] != '\0' && persona[0] != '!') {
        printf(" (");
        afr_print_dim(persona);
        printf(")");
    }
}
