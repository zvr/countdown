/*
 * countdown -- numbers round
 */

/*
 * Copyright (C) 2026 Alexios Zavras
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "numbers.h"

struct node {
	long value;
	char op;	/* top-level operator, or 0 for a plain number */
	char expr[MAX_EXPR];
};

static long target;
static long best_diff;
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

void numbers_solve(const long *numbers, int n, long tgt,
		   char *expr, size_t size, long *value)
{
	struct node nodes[MAX_NUMBERS];
	int i;

	target = tgt;
	best_diff = LONG_MAX;
	best_value = 0;
	best_expr[0] = '\0';

	for (i = 0; i < n && i < MAX_NUMBERS; i++) {
		nodes[i].value = numbers[i];
		nodes[i].op = 0;
		snprintf(nodes[i].expr, MAX_EXPR, "%ld", numbers[i]);
		consider(&nodes[i]);
	}
	solve(nodes, i);

	snprintf(expr, size, "%s", best_expr);
	*value = best_value;
}

static void shuffle(long *pool, int size, int count)
{
	long t;
	int i, k;

	for (i = 0; i < count; i++) {
		k = i + rand() % (size - i);
		t = pool[i];
		pool[i] = pool[k];
		pool[k] = t;
	}
}

int numbers_random(long *numbers, long *tgt, int nlarge)
{
	static const long large[] = { 25, 50, 75, 100 };
	long pool[20];
	int used, i;

	srand((unsigned) time(NULL));
	*tgt = 100 + rand() % 900;
	if (nlarge < 0 || nlarge > 4)
		nlarge = rand() % 5;

	for (i = 0; i < 4; i++)
		pool[i] = large[i];
	shuffle(pool, 4, nlarge);
	for (used = 0; used < nlarge; used++)
		numbers[used] = pool[used];

	for (i = 0; i < 20; i++)
		pool[i] = i / 2 + 1;
	shuffle(pool, 20, 6 - nlarge);
	for (i = 0; i < 6 - nlarge; i++)
		numbers[used++] = pool[i];

	return used;
}
