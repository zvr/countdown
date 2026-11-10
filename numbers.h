/*
 * countdown -- numbers round
 */

/*
 * Copyright (C) 2026 Alexios Zavras
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef COUNTDOWN_NUMBERS_H
#define COUNTDOWN_NUMBERS_H

#include <stddef.h>

#ifndef MAX_NUMBERS
  #define MAX_NUMBERS 16
#endif
#ifndef MAX_EXPR
  #define MAX_EXPR 1024
#endif
#ifndef TT_BITS
  #define TT_BITS 18 /* log2 of the solver's transposition table size */
#endif

/*
 * Find an expression combining (a subset of) the n numbers that is as close
 * as possible to target, using as few operations as possible.
 * The expression is stored into expr (of the given size) and its value
 * into *value.
 */
void numbers_solve(const long *numbers,
                   int         n,
                   long        target,
                   char       *expr,
                   size_t      size,
                   long       *value);

/*
 * Print the distinct exact solutions, shortest first, at most
 * max_solutions of them (or the closest result if there are none).
 * Returns the total number of distinct solutions found.
 */
long
numbers_solve_all(const long *numbers, int n, long target, long max_solutions);

/*
 * Limit each search to the given time in seconds (0: no limit), measured
 * as CPU time, or as elapsed (wall-clock) time if wall is nonzero.
 * When the limit is hit, the best result found so far is reported and
 * numbers_timed_out() returns nonzero.
 */
void               numbers_set_time_limit(double seconds, int wall);
int                numbers_timed_out(void);

/* A seed that differs between runs, even within the same second. */
unsigned long long numbers_auto_seed(void);

/*
 * Generate a random puzzle following the TV show rules:
 * six numbers, 0-4 of them "large" (25, 50, 75, 100, each at most once;
 * nlarge < 0 picks the count at random),
 * the rest "small" (1-10, each at most twice), and a target in 100-999.
 * The same seed always gives the same puzzle.
 * numbers must have room for 6 entries; returns the number of entries.
 */
int                numbers_random(long              *numbers,
                                  long              *target,
                                  int                nlarge,
                                  unsigned long long seed);

#endif
