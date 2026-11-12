/*
 * answer.h -- parse the kusyareshka feed of the Radio Anonimus.
 *
 * The feed lives at https://anon.fm/answers.js. It is a JSON array of
 * rows. Each row is an array with seven fields:
 *
 *   0  kind flag, "0" for a listener message, "1" for a message of the DJ
 *   1  listener nick, or "!" when the row belongs to the DJ
 *   2  text of the listener message, with HTML markup
 *   3  time of the row, wrapped in a <span class="timestamp"> tag
 *   4  nick of the DJ
 *   5  text of the DJ answer or of the DJ message
 *   6  full name of the DJ persona
 *
 * New rows appear at the start of the array, so the array is ordered
 * from the newest row to the oldest row.
 */

#ifndef AFR_ANSWER_H
#define AFR_ANSWER_H

/* One exchange of the feed. Every text field is HTML stripped. */
typedef struct {
    int is_dj;      /* flag "1" means a message of the DJ to the listeners */
    char *listener; /* nick of the listener, or "!" for DJ rows */
    char *message;  /* text of the listener message, or an empty string */
    char *time;     /* time of day, like 12:34:56 */
    char *dj;       /* nick of the DJ */
    char *response; /* text of the DJ answer or message */
    char *persona;  /* full name of the DJ persona */
} afr_answer;

/*
 * Parse the feed body into a fresh array of answers. Return the number
 * of parsed rows and set *out to the array. On a parse error return -1
 * and set *err to the reason. The caller frees the array with
 * afr_answers_free.
 */
int afr_answers_parse(const char *body, afr_answer **out, const char **err);

/* Free n answers and their fields. */
void afr_answers_free(afr_answer *arr, int n);

/* FNV-1a hash over all fields of one answer. */
unsigned long afr_answer_hash(const afr_answer *a);

#endif
