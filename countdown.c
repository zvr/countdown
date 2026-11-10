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
*/

#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define MAX_NUMBERS 16
#define MAX_EXPR 1024

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

static void usage(const char *prog)
{
        fprintf(stderr, "usage: %s number... target\n"
                "       %s -r|--random [-l|--large n]\n", prog, prog);
}

int main(int argc, char *argv[])
{
        struct node nodes[MAX_NUMBERS];
        int n = argc - 2;
        int i;
        int random = 0, nlarge = -1;

        if (argc > 1 && argv[1][0] == '-') {
                for (i = 1; i < argc; i++) {
                        if (strcmp(argv[i], "-r") == 0 ||
                            strcmp(argv[i], "--random") == 0) {
                                random = 1;
                        } else if (strcmp(argv[i], "-l") == 0 ||
                                   strcmp(argv[i], "--large") == 0) {
                                if (i + 1 >= argc ||
                                    strlen(argv[i + 1]) != 1 ||
                                    argv[i + 1][0] < '0' ||
                                    argv[i + 1][0] > '4') {
                                        fprintf(stderr, "%s: %s requires a "
                                                "number between 0 and 4\n",
                                                argv[0], argv[i]);
                                        return EXIT_FAILURE;
                                }
                                nlarge = argv[++i][0] - '0';
                        } else {
                                usage(argv[0]);
                                return EXIT_FAILURE;
                        }
                }
                if (!random) {
                        fprintf(stderr, "%s: -l/--large is only valid "
                                "with -r/--random\n", argv[0]);
                        return EXIT_FAILURE;
                }
                n = random_puzzle(nodes, nlarge);
                solve(nodes, n);
                printf("%s = %ld\n", best_expr, best_value);
                return EXIT_SUCCESS;
        }
        if (argc < 3) {
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
                if (!parse_number(argv[i + 1], &nodes[i].value)) {
                        fprintf(stderr, "%s: invalid number '%s'\n",
                                argv[0], argv[i + 1]);
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

