/*
 * test_answer.c -- check the feed parser against known inputs.
 */

#include "../src/answer.c"

static void check_html(const char *in, const char *want)
{
    char *got = answer_html_to_text(in);
    int ok = strcmp(got, want) == 0;

    printf("html: %s [%s] want [%s]\n", ok ? "PASS" : "FAIL", got, want);
    free(got);
}

static void check_parse(const char *body, int want)
{
    afr_answer *arr = NULL;
    const char *err = NULL;
    int n = afr_answers_parse(body, &arr, &err);
    int ok = n == want;

    printf("parse: %s n=%d want=%d err=%s\n", ok ? "PASS" : "FAIL", n, want,
           err != NULL ? err : "-");
    if (ok && arr != NULL) {
        afr_answers_free(arr, n);
    }
}

int main(void)
{
    check_html("plain text", "plain text");
    check_html("<br>", "\n");
    check_html("a<br/>b", "a\nb");
    check_html("<a rel=\"nofollow noopener noreferrer\" "
               "href=\"https://x/y.png\" target=\"_blank\">https://x/y.png</a>",
               "https://x/y.png");
    check_html("a &lt; b &amp; c &quot;d&quot; &apos;e&apos;",
               "a < b & c \"d\" 'e'");
    check_html("&#39;q&#x21;", "'q!");
    check_html("<span class=\"timestamp\">12:34:56.7890</span>x",
               "12:34:56.7890x");
    check_html("<a href=\"http://a/b\">text</a>", "http://a/b");
    check_html("<a href='http://a/b'>t</a>", "http://a/b");

    check_parse("[]", 0);
    check_parse("[[\"0\",\"u\",\"m\",\"<span>x</span>12:00:00\",\"K\","
                "\"r\",\"Persona\"]]", 1);
    check_parse("[[\"1\",\"!\",\"\",\"<span>y</span>01:02:03\",\"Kriesh\","
                "\"dj text\",\"Name\"],"
                "[\"0\",\"nick\",\"msg <br>2\",\"<span>z</span>09:59:59\","
                "\"vito\",\"answer\",\"P\"]]", 2);
    check_parse("[\"bad\",1]", -1);
    check_parse("not json", -1);
    return 0;
}
