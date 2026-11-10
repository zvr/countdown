/*
 * countdown -- minimal getopt_long() for platforms lacking <getopt.h>
 */

/*
 * Copyright (C) 2026 Alexios Zavras
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef COUNTDOWN_GETOPT_H
#define COUNTDOWN_GETOPT_H

#define no_argument             0
#define required_argument       1

struct option {
	const char *name;
	int has_arg;
	int *flag;
	int val;
};

extern char *optarg;
extern int optind, opterr, optopt;

/*
 * Subset of GNU getopt_long(): short option clusters ("-rl2"),
 * attached or separate arguments, "--name" / "--name=value" long options,
 * "--" terminator, and permutation of non-option arguments to the end.
 * Optional arguments are not supported.
 */
int getopt_long(int argc, char **argv, const char *shortopts,
		const struct option *longopts, int *longindex);

#endif
