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

/*
 * Find an expression combining (a subset of) the n numbers that is as close
 * as possible to target. The expression is stored into expr (of the given
 * size) and its value into *value.
 */
void numbers_solve(const long *numbers, int n, long target,
		   char *expr, size_t size, long *value);

/*
 * Generate a random puzzle following the TV show rules:
 * six numbers, 0-4 of them "large" (25, 50, 75, 100, each at most once;
 * nlarge < 0 picks the count at random),
 * the rest "small" (1-10, each at most twice), and a target in 100-999.
 * numbers must have room for 6 entries; returns the number of entries.
 */
int numbers_random(long *numbers, long *target, int nlarge);

#endif
