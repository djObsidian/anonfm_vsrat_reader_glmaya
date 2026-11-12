/*
 * color.h -- unique colors for the nicks of the kusyareshka.
 */

#ifndef AFR_COLOR_H
#define AFR_COLOR_H

/* Set to 1 to print every nick without ANSI color codes. */
extern int afr_no_color;

/* Compute the RGB color of one nick. The same nick always gets the same
 * color. */
void afr_nick_rgb(const char *nick, int *r, int *g, int *b);

/* Print a nick with its unique color. */
void afr_print_nick(const char *nick);

/* Print a plain nick. */
void afr_print_nick_plain(const char *nick);

/* Print a string in dim gray, for example a timestamp. */
void afr_print_dim(const char *s);

/* Print a string in the color of one nick, followed by a reset. */
void afr_print_person(const char *nick, const char *persona);

#endif
