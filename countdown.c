/*
 * countdown -- an implementation of the classical TV game
 */

/*
 * Copyright (C) 2026 Alexios Zavras
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

/*
This is a command-line utility that "solves" countdown problems.

For number rounds, the program takes a list of numbers and a target number,
and finds a sequence of basic arithmetic operations
(addition, subtraction, multiplication, division)
that combines the numbers to reach the target (or as close as possible to it).
There is no requirement to use use all numbers and
a number may not be used more times than it appears on the input.
Only exact division (no remainder) is permitted.
Fractions and negative numbers are not allowed.

Example usage:
  $ countdown 3 8 7 2 50 75 812
  (50 + 8) * 2 * 7 = 812

For letter rounds, the program takes the revealed letters
(individually or as one string) and lists the words from a dictionary file
(one word per line, default "words.txt") that can be formed from them,
longest first.
Each letter may not be used more times than it appears on the input.

Example usage:
  $ countdown c h a r t e s o p
  chapter (7)
  patches (7)
  ...
*/

#ifdef _MSC_VER
#define _CRT_SECURE_NO_WARNINGS
#endif

#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifndef MAX_NUMBERS
#define MAX_NUMBERS 16
#endif
#ifndef MAX_EXPR
#define MAX_EXPR 1024
#endif
#ifndef DEFAULT_DICT
#define DEFAULT_DICT "words.txt"
#endif
#ifndef DEFAULT_COUNT
#define DEFAULT_COUNT 50
#endif

struct node {
        long value;
        char op;	/* top-level operator, or 0 for a plain number */
        char expr[MAX_EXPR];
};

static long target;
static long best_diff = LONG_MAX;
static long best_value;
static char best_expr[MAX_EXPR];

static long diff(long v)
{
        return v > target ? v - target : target - v;
}

static void consider(const struct node *n)
{
        long d = diff(n->value);

        if (d < best_diff) {
                best_diff = d;
                best_value = n->value;
                snprintf(best_expr, MAX_EXPR, "%s", n->expr);
        }
}

static int is_additive(char op)
{
        return op == '+' || op == '-';
}

/* Build "a op b" into out, adding parentheses only where needed. */
static int combine(struct node *out, const struct node *a,
                   const struct node *b, char op, long value)
{
        int lp = 0, rp = 0;
        int len;

        switch (op) {
        case '-':
                rp = is_additive(b->op);
                break;
        case '*':
                lp = is_additive(a->op);
                rp = is_additive(b->op);
                break;
        case '/':
                lp = is_additive(a->op);
                rp = b->op != 0;
                break;
        }
        len = snprintf(out->expr, MAX_EXPR, "%s%s%s %c %s%s%s",
                       lp ? "(" : "", a->expr, lp ? ")" : "", op,
                       rp ? "(" : "", b->expr, rp ? ")" : "");
        if (len < 0 || len >= MAX_EXPR)
                return 0;
        out->value = value;
        out->op = op;
        return 1;
}

static void solve(struct node *nodes, int n)
{
        struct node save_i, save_j;
        int i, j, k;

        if (best_diff == 0 || n < 2)
                return;

        for (i = 0; i < n; i++) {
                for (j = i + 1; j < n; j++) {
                        struct node *a, *b;

                        save_i = nodes[i];
                        save_j = nodes[j];
                        /* a is the larger operand */
                        if (save_i.value >= save_j.value) {
                                a = &save_i;
                                b = &save_j;
                        } else {
                                a = &save_j;
                                b = &save_i;
                        }

                        for (k = 0; k < 4; k++) {
                                struct node r;
                                long v;
                                char op = "+-*/"[k];

                                switch (op) {
                                case '+':
                                        if (a->value > LONG_MAX - b->value)
                                                continue;
                                        v = a->value + b->value;
                                        break;
                                case '-':
                                        if (a->value == b->value)
                                                continue;
                                        v = a->value - b->value;
                                        break;
                                case '*':
                                        if (a->value == 1 || b->value == 1)
                                                continue;
                                        if (b->value != 0 &&
                                            a->value > LONG_MAX / b->value)
                                                continue;
                                        v = a->value * b->value;
                                        break;
                                default:
                                        if (b->value <= 1 ||
                                            a->value % b->value != 0)
                                                continue;
                                        v = a->value / b->value;
                                        break;
                                }
                                if (!combine(&r, a, b, op, v))
                                        continue;

                                consider(&r);
                                if (best_diff == 0) {
                                        nodes[i] = save_i;
                                        nodes[j] = save_j;
                                        return;
                                }

                                nodes[i] = r;
                                nodes[j] = nodes[n - 1];
                                solve(nodes, n - 1);
                                nodes[n - 1] = nodes[j];
                                nodes[i] = save_i;
                                nodes[j] = save_j;
                                if (best_diff == 0)
                                        return;
                        }
                }
        }
}

static int parse_number(const char *s, long *out)
{
        char *end;
        long v;

        errno = 0;
        v = strtol(s, &end, 10);
        if (errno != 0 || end == s || *end != '\0' || v <= 0)
                return 0;
        *out = v;
        return 1;
}

static void set_number(struct node *nd, long v)
{
        nd->value = v;
        nd->op = 0;
        snprintf(nd->expr, MAX_EXPR, "%ld", v);
        consider(nd);
}

/*
 * Generate a random puzzle following the TV show rules:
 * six numbers, 0-4 of them "large" (25, 50, 75, 100, each at most once;
 * nlarge < 0 picks the count at random),
 * the rest "small" (1-10, each at most twice), and a target in 100-999.
 */
static int random_puzzle(struct node *nodes, int nlarge)
{
        static const long large[] = { 25, 50, 75, 100 };
        long pool[20], t;
        int used, i, k;

        srand((unsigned) time(NULL));
        target = 100 + rand() % 900;
        if (nlarge < 0)
                nlarge = rand() % 5;

        for (i = 0; i < 4; i++)
                pool[i] = large[i];
        for (i = 0; i < 4; i++) {
                k = i + rand() % (4 - i);
                t = pool[i];
                pool[i] = pool[k];
                pool[k] = t;
        }
        for (used = 0; used < nlarge; used++)
                set_number(&nodes[used], pool[used]);

        for (i = 0; i < 20; i++)
                pool[i] = i / 2 + 1;
        for (i = 0; i < 6 - nlarge; i++) {
                k = i + rand() % (20 - i);
                t = pool[i];
                pool[i] = pool[k];
                pool[k] = t;
                set_number(&nodes[used++], pool[i]);
        }

        for (i = 0; i < used; i++)
                printf("%ld ", nodes[i].value);
        printf("-> %ld\n", target);
        return used;
}

/*
 * Letters round: list the dictionary words that can be formed from the
 * given letters (each letter used at most as often as it appears),
 * longest first, up to max_words of them.
 */

struct word_list {
        char **words;
        size_t count, size;
};

static int add_word(struct word_list *wl, const char *w, size_t len)
{
        char *copy;

        if (wl->count == wl->size) {
                size_t size = wl->size ? wl->size * 2 : 256;
                char **p = realloc(wl->words, size * sizeof *p);

                if (p == NULL)
                        return 0;
                wl->words = p;
                wl->size = size;
        }
        copy = malloc(len + 1);
        if (copy == NULL)
                return 0;
        memcpy(copy, w, len + 1);
        wl->words[wl->count++] = copy;
        return 1;
}

static int cmp_words(const void *a, const void *b)
{
        const char *wa = *(const char * const *) a;
        const char *wb = *(const char * const *) b;
        size_t la = strlen(wa), lb = strlen(wb);

        if (la != lb)
                return la < lb ? 1 : -1;
        return strcmp(wa, wb);
}

static int letters_round(const char *prog, const char *letters,
                         const char *dict, long max_words)
{
        int avail[26] = { 0 };
        int need[26];
        size_t nletters = strlen(letters);
        struct word_list wl = { NULL, 0, 0 };
        char line[256];
        FILE *f;
        size_t i, len;
        long shown;
        int ok = 1;

        for (i = 0; i < nletters; i++)
                avail[letters[i] - 'a']++;

        f = fopen(dict, "r");
        if (f == NULL) {
                fprintf(stderr, "%s: cannot open dictionary '%s': %s\n",
                        prog, dict, strerror(errno));
                return EXIT_FAILURE;
        }
        while (fgets(line, sizeof line, f) != NULL) {
                len = strcspn(line, "\r\n");
                line[len] = '\0';
                if (len == 0 || len > nletters)
                        continue;
                memset(need, 0, sizeof need);
                for (i = 0; i < len; i++) {
                        int c = tolower((unsigned char) line[i]);

                        if (c < 'a' || c > 'z' || ++need[c - 'a'] > avail[c - 'a'])
                                break;
                        line[i] = (char) c;
                }
                if (i == len && !add_word(&wl, line, len)) {
                        fprintf(stderr, "%s: out of memory\n", prog);
                        ok = 0;
                        break;
                }
        }
        fclose(f);

        if (ok) {
                qsort(wl.words, wl.count, sizeof *wl.words, cmp_words);
                shown = 0;
                for (i = 0; i < wl.count && shown < max_words; i++) {
                        if (i > 0 && strcmp(wl.words[i], wl.words[i - 1]) == 0)
                                continue;
                        printf("%s (%u)\n", wl.words[i],
                               (unsigned) strlen(wl.words[i]));
                        shown++;
                }
                if (shown == 0)
                        printf("no words found\n");
        }
        for (i = 0; i < wl.count; i++)
                free(wl.words[i]);
        free(wl.words);
        return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}

static int is_word_arg(const char *s)
{
        if (*s == '\0')
                return 0;
        for (; *s; s++)
                if (!isalpha((unsigned char) *s))
                        return 0;
        return 1;
}

static void usage(const char *prog)
{
        fprintf(stderr, "usage: %s number... target\n"
                "       %s -r|--random [-l|--large n]\n"
                "       %s [-d|--dict file] [-n|--count n] letters...\n",
                prog, prog, prog);
}

int main(int argc, char *argv[])
{
        struct node nodes[MAX_NUMBERS];
        const char *dict = DEFAULT_DICT;
        long max_words = DEFAULT_COUNT;
        int n, first;
        int i;
        int random = 0, nlarge = -1;

        for (first = 1; first < argc && argv[first][0] == '-'; first++) {
                const char *opt = argv[first];

                if (strcmp(opt, "-r") == 0 || strcmp(opt, "--random") == 0) {
                        random = 1;
                } else if (strcmp(opt, "-l") == 0 ||
                           strcmp(opt, "--large") == 0) {
                        if (first + 1 >= argc ||
                            strlen(argv[first + 1]) != 1 ||
                            argv[first + 1][0] < '0' ||
                            argv[first + 1][0] > '4') {
                                fprintf(stderr, "%s: %s requires a "
                                        "number between 0 and 4\n",
                                        argv[0], opt);
                                return EXIT_FAILURE;
                        }
                        nlarge = argv[++first][0] - '0';
                } else if (strcmp(opt, "-d") == 0 ||
                           strcmp(opt, "--dict") == 0) {
                        if (first + 1 >= argc) {
                                fprintf(stderr, "%s: %s requires a file "
                                        "name\n", argv[0], opt);
                                return EXIT_FAILURE;
                        }
                        dict = argv[++first];
                } else if (strcmp(opt, "-n") == 0 ||
                           strcmp(opt, "--count") == 0) {
                        if (first + 1 >= argc ||
                            !parse_number(argv[first + 1], &max_words)) {
                                fprintf(stderr, "%s: %s requires a "
                                        "positive number\n", argv[0], opt);
                                return EXIT_FAILURE;
                        }
                        first++;
                } else {
                        usage(argv[0]);
                        return EXIT_FAILURE;
                }
        }

        if (random) {
                if (first < argc) {
                        usage(argv[0]);
                        return EXIT_FAILURE;
                }
                n = random_puzzle(nodes, nlarge);
                solve(nodes, n);
                printf("%s = %ld\n", best_expr, best_value);
                return EXIT_SUCCESS;
        }
        if (nlarge >= 0) {
                fprintf(stderr, "%s: -l/--large is only valid "
                        "with -r/--random\n", argv[0]);
                return EXIT_FAILURE;
        }

        if (first < argc && is_word_arg(argv[first])) {
                char letters[256];
                size_t len = 0, k;

                for (i = first; i < argc; i++) {
                        if (!is_word_arg(argv[i])) {
                                fprintf(stderr, "%s: invalid letters '%s'\n",
                                        argv[0], argv[i]);
                                return EXIT_FAILURE;
                        }
                        for (k = 0; argv[i][k]; k++) {
                                if (len + 1 >= sizeof letters) {
                                        fprintf(stderr, "%s: too many "
                                                "letters\n", argv[0]);
                                        return EXIT_FAILURE;
                                }
                                letters[len++] = (char) tolower(
                                        (unsigned char) argv[i][k]);
                        }
                }
                letters[len] = '\0';
                return letters_round(argv[0], letters, dict, max_words);
        }

        n = argc - first - 1;
        if (n < 1) {
                usage(argv[0]);
                return EXIT_FAILURE;
        }
        if (n > MAX_NUMBERS) {
                fprintf(stderr, "%s: at most %d numbers are supported\n",
                        argv[0], MAX_NUMBERS);
                return EXIT_FAILURE;
        }
        if (!parse_number(argv[argc - 1], &target)) {
                fprintf(stderr, "%s: invalid target '%s'\n",
                        argv[0], argv[argc - 1]);
                return EXIT_FAILURE;
        }
        for (i = 0; i < n; i++) {
                if (!parse_number(argv[first + i], &nodes[i].value)) {
                        fprintf(stderr, "%s: invalid number '%s'\n",
                                argv[0], argv[first + i]);
                        return EXIT_FAILURE;
                }
                nodes[i].op = 0;
                snprintf(nodes[i].expr, MAX_EXPR, "%ld", nodes[i].value);
                consider(&nodes[i]);
        }

        solve(nodes, n);

        printf("%s = %ld\n", best_expr, best_value);
        return EXIT_SUCCESS;
}

