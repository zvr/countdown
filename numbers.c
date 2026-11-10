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

/* Time limit (CPU seconds, 0 = none) and search state. */
static double time_limit;
static clock_t deadline;
static unsigned long node_count;
static int timed_out;
static int cur_limit;	/* operations allowed in the current pass */

/* When listing all solutions, the exact ones found so far. */
static int all_mode;
static int all_failed;
static struct solution {
	char *expr;
	int ops;
} *solutions;
static size_t nsolutions, solutions_size;

/*
 * Canonical form of a solution, so that solutions differing only in the
 * order of operands (e.g. "a * b * c" and "a * c * b", or "a - b + c" and
 * "a + c - b") are listed once. The expression is parsed into a tree, and
 * chains of + and - (resp. * and /) are flattened and sorted: complex
 * operands first, then numbers in decreasing order; subtracted (divided)
 * operands come last, so all intermediate results remain valid.
 */

struct tree {
	char op;		/* 0 for a number */
	long value;
	struct tree *l, *r;
};

struct item {
	char *s;
	int inverse;		/* subtracted or divided */
};

static struct tree tree_pool[4 * MAX_NUMBERS];
static int tree_used;
static const char *parse_pos;

static struct tree *parse_sum(void);

static struct tree *new_tree(char op, long value, struct tree *l,
			     struct tree *r)
{
	struct tree *t;

	if (tree_used >= (int) (sizeof tree_pool / sizeof *tree_pool))
		return NULL;
	t = &tree_pool[tree_used++];
	t->op = op;
	t->value = value;
	t->l = l;
	t->r = r;
	return t;
}

static void skip_spaces(void)
{
	while (*parse_pos == ' ')
		parse_pos++;
}

static struct tree *parse_factor(void)
{
	struct tree *t;

	skip_spaces();
	if (*parse_pos == '(') {
		parse_pos++;
		t = parse_sum();
		skip_spaces();
		if (t == NULL || *parse_pos != ')')
			return NULL;
		parse_pos++;
		return t;
	}
	if (*parse_pos < '0' || *parse_pos > '9')
		return NULL;
	t = new_tree(0, strtol(parse_pos, (char **) &parse_pos, 10),
		     NULL, NULL);
	return t;
}

static struct tree *parse_chain(struct tree *(*sub)(void), char op1,
				char op2)
{
	struct tree *t = sub();

	for (;;) {
		char op;

		skip_spaces();
		op = *parse_pos;
		if (t == NULL || (op != op1 && op != op2))
			return t;
		parse_pos++;
		t = new_tree(op, 0, t, sub());
		if (t == NULL || t->r == NULL)
			return NULL;
	}
}

static struct tree *parse_product(void)
{
	return parse_chain(parse_factor, '*', '/');
}

static struct tree *parse_sum(void)
{
	return parse_chain(parse_product, '+', '-');
}

static int is_plain_number(const char *s)
{
	return *s >= '0' && *s <= '9' && strchr(s, ' ') == NULL;
}

static int cmp_items(const void *a, const void *b)
{
	const struct item *ia = a, *ib = b;
	int na = is_plain_number(ia->s), nb = is_plain_number(ib->s);
	long va, vb;

	if (ia->inverse != ib->inverse)
		return ia->inverse - ib->inverse;
	if (na != nb)
		return na - nb;
	if (!na)
		return strcmp(ia->s, ib->s);
	va = strtol(ia->s, NULL, 10);
	vb = strtol(ib->s, NULL, 10);
	return va < vb ? 1 : va > vb ? -1 : 0;
}

static char *canon(const struct tree *t);

/* Collect the operands of a +/- (or * and /) chain. */
static int collect(const struct tree *t, int additive, int inverse,
		   struct item *items, int *count)
{
	char plus = additive ? '+' : '*', minus = additive ? '-' : '/';

	if (t->op == plus || t->op == minus) {
		return collect(t->l, additive, inverse, items, count) &&
		       collect(t->r, additive, t->op == minus ? !inverse :
			       inverse, items, count);
	}
	if (*count >= 2 * MAX_NUMBERS)
		return 0;
	items[*count].inverse = inverse;
	items[*count].s = canon(t);
	return items[(*count)++].s != NULL;
}

static char *canon(const struct tree *t)
{
	struct item items[2 * MAX_NUMBERS];
	int count = 0, additive, i, ok;
	char *out;
	size_t len = 0;

	out = malloc(MAX_EXPR);
	if (out == NULL)
		return NULL;
	if (t->op == 0) {
		snprintf(out, MAX_EXPR, "%ld", t->value);
		return out;
	}

	additive = t->op == '+' || t->op == '-';
	ok = collect(t, additive, 0, items, &count);
	if (ok)
		qsort(items, count, sizeof *items, cmp_items);
	out[0] = '\0';
	for (i = 0; ok && i < count; i++) {
		/* operands of a product that are sums need parentheses */
		int paren = !additive && !is_plain_number(items[i].s);
		const char *sep = i == 0 ? "" :
			additive ? (items[i].inverse ? " - " : " + ") :
				   (items[i].inverse ? " / " : " * ");
		int n;

		n = snprintf(out + len, MAX_EXPR - len, "%s%s%s%s", sep,
			     paren ? "(" : "", items[i].s, paren ? ")" : "");
		if (n < 0 || (size_t) n >= MAX_EXPR - len)
			ok = 0;
		else
			len += (size_t) n;
	}
	for (i = 0; i < count; i++)
		free(items[i].s);
	if (!ok) {
		free(out);
		return NULL;
	}
	return out;
}

/* Canonical form of expr into out; falls back to expr on failure. */
static void canonicalize(const char *expr, char *out)
{
	struct tree *t;
	char *c = NULL;

	tree_used = 0;
	parse_pos = expr;
	t = parse_sum();
	if (t != NULL && *parse_pos == '\0')
		c = canon(t);
	snprintf(out, MAX_EXPR, "%s", c != NULL ? c : expr);
	free(c);
}

static void add_solution(const struct node *r)
{
	char expr[MAX_EXPR];
	size_t len;
	char *copy;

	canonicalize(r->expr, expr);
	len = strlen(expr) + 1;
	if (nsolutions == solutions_size) {
		size_t size = solutions_size ? solutions_size * 2 : 256;
		struct solution *p = realloc(solutions, size * sizeof *p);

		if (p == NULL) {
			all_failed = 1;
			return;
		}
		solutions = p;
		solutions_size = size;
	}
	copy = malloc(len);
	if (copy == NULL) {
		all_failed = 1;
		return;
	}
	memcpy(copy, expr, len);
	solutions[nsolutions].expr = copy;
	solutions[nsolutions].ops = r->ops;
	nsolutions++;
}

static int search_done(void)
{
	if (!timed_out && time_limit > 0 && (++node_count & 1023) == 0 &&
	    clock() >= deadline)
		timed_out = 1;
	return timed_out || all_failed ||
	       (!all_mode && best_diff == 0 && best_ops <= cur_limit);
}

static void start_timer(void)
{
	timed_out = 0;
	node_count = 0;
	if (time_limit > 0)
		deadline = clock() + (clock_t) (time_limit * CLOCKS_PER_SEC);
}

void numbers_set_time_limit(double seconds)
{
	time_limit = seconds;
}

int numbers_timed_out(void)
{
	return timed_out;
}

/*
 * Depth-limited search: combine pairs of the n current values, using at
 * most `remaining` further operations. hash identifies the multiset.
 */
static void solve(struct node *nodes, int n, uint64_t hash, int remaining)
{
	struct node save_i, save_j;
	int i, j, k;

	if (search_done() || remaining == 0 || n < 2)
		return;
	/* small states are cheaper to expand than to look up */
	if (remaining >= 2 && n >= 3 && already_seen(hash))
		return;

	for (i = 0; i < n; i++) {
		for (j = i + 1; j < n; j++) {
			struct node *a, *b;
			uint64_t rest;

			/*
			 * skip pairs equal by value to an earlier pair
			 * (not when listing all solutions: their expressions differ)
			 */
			int dup = 0, p, q;

			for (p = 0; p <= i && !dup && !all_mode; p++)
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
				if (all_mode && v == target)
					add_solution(&r);
				if (search_done())
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
				if (search_done())
					return;
			}
		}
	}
}

static int init_search(const long *numbers, int n, long tgt,
		       struct node *nodes, uint64_t *hash)
{
	int i;

	target = tgt;
	best_diff = LONG_MAX;
	best_value = 0;
	best_ops = INT_MAX;
	best_expr[0] = '\0';
	*hash = 0;

	if (n > MAX_NUMBERS)
		n = MAX_NUMBERS;
	for (i = 0; i < n; i++) {
		nodes[i].value = numbers[i];
		nodes[i].ops = 0;
		nodes[i].op = 0;
		snprintf(nodes[i].expr, MAX_EXPR, "%ld", numbers[i]);
		*hash += value_hash(numbers[i]);
		consider(&nodes[i]);
		if (all_mode && numbers[i] == tgt)
			add_solution(&nodes[i]);
	}
	start_timer();
	return n;
}

void numbers_solve(const long *numbers, int n, long tgt,
		   char *expr, size_t size, long *value)
{
	struct node nodes[MAX_NUMBERS];
	uint64_t hash;
	int limit;

	all_mode = 0;
	n = init_search(numbers, n, tgt, nodes, &hash);
	seen = calloc(seen_mask + 1, sizeof *seen);

	/*
	 * With a time limit, first spend half of it on a plain depth-first
	 * search, which quickly finds good (if not the shortest) results.
	 */
	if (time_limit > 0 && n > 2) {
		clock_t start = clock();

		deadline = start + (clock_t) (time_limit / 2 * CLOCKS_PER_SEC);
		cur_limit = n - 1;
		solve(nodes, n, hash, n - 1);
		timed_out = 0;
		deadline = start + (clock_t) (time_limit * CLOCKS_PER_SEC);
		if (seen != NULL)
			memset(seen, 0, (seen_mask + 1) * sizeof *seen);
	}

	/*
	 * Iterative deepening on the number of operations: once a pass is
	 * complete, no exact solution shorter than its limit exists.
	 */
	for (limit = 1; limit < n && !timed_out; limit++) {
		if (best_diff == 0 && best_ops <= limit)
			break;
		if (seen != NULL)
			memset(seen, 0, (seen_mask + 1) * sizeof *seen);
		cur_limit = limit;
		solve(nodes, n, hash, limit);
	}
	free(seen);
	seen = NULL;

	snprintf(expr, size, "%s", best_expr);
	*value = best_value;
}

static int cmp_solutions(const void *a, const void *b)
{
	const struct solution *sa = a, *sb = b;

	if (sa->ops != sb->ops)
		return sa->ops < sb->ops ? -1 : 1;
	return strcmp(sa->expr, sb->expr);
}

long numbers_solve_all(const long *numbers, int n, long tgt,
		       long max_solutions)
{
	struct node nodes[MAX_NUMBERS];
	uint64_t hash;
	size_t i;
	long shown = 0;

	all_mode = 1;
	all_failed = 0;
	nsolutions = 0;
	n = init_search(numbers, n, tgt, nodes, &hash);

	/* a single full-depth pass, without state caching, finds them all */
	solve(nodes, n, hash, n - 1);
	all_mode = 0;

	if (all_failed)
		fprintf(stderr, "warning: out of memory, "
			"not all solutions were collected\n");
	if (nsolutions > 0)
		qsort(solutions, nsolutions, sizeof *solutions, cmp_solutions);
	for (i = 0; i < nsolutions; i++) {
		if (i > 0 && strcmp(solutions[i].expr,
				    solutions[i - 1].expr) == 0)
			continue;
		if (shown < max_solutions)
			printf("%s = %ld\n", solutions[i].expr, tgt);
		shown++;
	}
	if (shown == 0)
		printf("%s = %ld\n", best_expr, best_value);

	for (i = 0; i < nsolutions; i++)
		free(solutions[i].expr);
	free(solutions);
	solutions = NULL;
	nsolutions = solutions_size = 0;
	return shown;
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
