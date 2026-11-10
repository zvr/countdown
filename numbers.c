/*
 * countdown -- numbers round
 */

/*
 * Copyright (C) 2026 Alexios Zavras
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef _WIN32
#include <process.h>
#define get_pid() ((unsigned long long) _getpid())
#else
#include <unistd.h>
#define get_pid() ((unsigned long long) getpid())
#endif

#include "numbers.h"

struct node {
	long value;
	int ops;	/* number of operations in expr */
	char op;	/* top-level operator, or 0 for a plain number */
	char expr[MAX_EXPR];
};

static long target;
static long best_diff;
static long best_value;
static int best_ops;
static char best_expr[MAX_EXPR];

/*
 * Transposition table: hashes of the multisets of values already explored
 * in the current search pass. Two states with the same values can reach
 * exactly the same results, so only the first one needs to be expanded.
 */
static uint64_t *seen;
static const size_t seen_mask = ((size_t) 1 << TT_BITS) - 1;

static uint64_t splitmix64(uint64_t x)
{
	x += 0x9e3779b97f4a7c15ULL;
	x = (x ^ (x >> 30)) * 0xbf58476d1ce4e5b9ULL;
	x = (x ^ (x >> 27)) * 0x94d049bb133111ebULL;
	return x ^ (x >> 31);
}

/* Order-independent: the hash of a multiset is the sum of its elements'. */
static uint64_t value_hash(long v)
{
	return splitmix64((uint64_t) v);
}

/* Returns 1 if the state was already seen, otherwise records it. */
static int already_seen(uint64_t key)
{
	size_t i, probe;

	if (seen == NULL)
		return 0;
	if (key == 0)
		key = 1;
	for (probe = 0, i = (size_t) key & seen_mask; probe < 16;
	     probe++, i = (i + 1) & seen_mask) {
		if (seen[i] == key)
			return 1;
		if (seen[i] == 0) {
			seen[i] = key;
			return 0;
		}
	}
	return 0;       /* table crowded: just explore again */
}

static long diff(long v)
{
	return v > target ? v - target : target - v;
}

static void consider(const struct node *n)
{
	long d = diff(n->value);

	if (d < best_diff || (d == best_diff && n->ops < best_ops)) {
		best_diff = d;
		best_value = n->value;
		best_ops = n->ops;
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
	out->ops = a->ops + b->ops + 1;
	out->op = op;
	return 1;
}

/* Compute a op b; returns 0 if not allowed or not useful. */
static int apply(char op, long a, long b, long *v)
{
	switch (op) {
	case '+':
		if (a > LONG_MAX - b)
			return 0;
		*v = a + b;
		return 1;
	case '-':
		if (a == b)
			return 0;
		*v = a - b;
		return 1;
	case '*':
		if (a == 1 || b == 1 || a > LONG_MAX / b)
			return 0;
		*v = a * b;
		return 1;
	default:
		if (b <= 1 || a % b != 0)
			return 0;
		*v = a / b;
		return 1;
	}
}

/*
 * Depth-limited search: combine pairs of the n current values, using at
 * most `remaining` further operations. hash identifies the multiset.
 */
static void solve(struct node *nodes, int n, uint64_t hash, int remaining)
{
	struct node save_i, save_j;
	int i, j, k;

	if (best_diff == 0 || remaining == 0 || n < 2)
		return;
	/* small states are cheaper to expand than to look up */
	if (remaining >= 2 && n >= 3 && already_seen(hash))
		return;

	for (i = 0; i < n; i++) {
		for (j = i + 1; j < n; j++) {
			struct node *a, *b;
			uint64_t rest;

			/* skip pairs equal by value to an earlier pair */
			int dup = 0, p, q;

			for (p = 0; p <= i && !dup; p++)
				for (q = p + 1; q < n && !dup; q++) {
					if (p == i && q >= j)
						break;
					dup = (nodes[p].value == nodes[i].value &&
					       nodes[q].value == nodes[j].value) ||
					      (nodes[p].value == nodes[j].value &&
					       nodes[q].value == nodes[i].value);
				}
			if (dup)
				continue;

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
			rest = hash - value_hash(a->value) -
			       value_hash(b->value);

			for (k = 0; k < 4; k++) {
				struct node r;
				long v;
				char op = "+-*/"[k];

				if (!apply(op, a->value, b->value, &v) ||
				    !combine(&r, a, b, op, v))
					continue;

				consider(&r);
				if (best_diff == 0)
					return;
				if (remaining == 1 || n == 2)
					continue;

				nodes[i] = r;
				nodes[j] = nodes[n - 1];
				solve(nodes, n - 1, rest + value_hash(v),
				      remaining - 1);
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
	uint64_t hash = 0;
	int i, limit;

	target = tgt;
	best_diff = LONG_MAX;
	best_value = 0;
	best_ops = INT_MAX;
	best_expr[0] = '\0';

	if (n > MAX_NUMBERS)
		n = MAX_NUMBERS;
	for (i = 0; i < n; i++) {
		nodes[i].value = numbers[i];
		nodes[i].ops = 0;
		nodes[i].op = 0;
		snprintf(nodes[i].expr, MAX_EXPR, "%ld", numbers[i]);
		hash += value_hash(numbers[i]);
		consider(&nodes[i]);
	}

	/*
	 * Iterative deepening on the number of operations, so the first
	 * exact solution found is one of the shortest.
	 */
	seen = calloc(seen_mask + 1, sizeof *seen);
	for (limit = 1; limit < n && best_diff != 0; limit++) {
		if (seen != NULL)
			memset(seen, 0, (seen_mask + 1) * sizeof *seen);
		solve(nodes, n, hash, limit);
	}
	free(seen);
	seen = NULL;

	snprintf(expr, size, "%s", best_expr);
	*value = best_value;
}

/* Random numbers: splitmix64 generator with unbiased range reduction. */

static uint64_t rng_state;

static uint64_t rng_next(void)
{
	uint64_t x = rng_state;

	rng_state += 0x9e3779b97f4a7c15ULL;
	return splitmix64(x);
}

/* Uniform integer in [0, n), without modulo bias. */
static long rng_below(long n)
{
	uint64_t limit = UINT64_MAX - UINT64_MAX % (uint64_t) n;
	uint64_t x;

	do
		x = rng_next();
	while (x >= limit);
	return (long) (x % (uint64_t) n);
}

unsigned long long numbers_auto_seed(void)
{
	unsigned long long s = (unsigned long long) time(NULL);
	int local;

	s ^= splitmix64((uint64_t) clock());
	s ^= splitmix64(get_pid() << 32);
	s ^= splitmix64((uint64_t) (uintptr_t) &local);
	return s;
}

static void shuffle(long *pool, int size, int count)
{
	long t;
	int i, k;

	for (i = 0; i < count; i++) {
		k = i + (int) rng_below(size - i);
		t = pool[i];
		pool[i] = pool[k];
		pool[k] = t;
	}
}

int numbers_random(long *numbers, long *tgt, int nlarge,
		   unsigned long long seed)
{
	static const long large[] = { 25, 50, 75, 100 };
	long pool[20];
	int used, i;

	rng_state = seed;
	*tgt = 100 + rng_below(900);
	if (nlarge < 0 || nlarge > 4)
		nlarge = (int) rng_below(5);

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
