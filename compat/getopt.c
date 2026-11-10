/*
 * countdown -- minimal getopt_long() for platforms lacking <getopt.h>
 */

/*
 * Copyright (C) 2026 Alexios Zavras
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <stdio.h>
#include <string.h>

#include "getopt.h"

char *optarg;
int optind = 1, opterr = 1, optopt;

static char *nextchar;
static int gap;         /* non-options skipped before the current element */

static int is_option(const char *s)
{
	return s[0] == '-' && s[1] != '\0';
}

/* Move argv[from] to argv[to] (to <= from), shifting the rest up. */
static void rotate(char **argv, int from, int to)
{
	char *t = argv[from];

	for (; from > to; from--)
		argv[from] = argv[from - 1];
	argv[to] = t;
}

/* Consume the next element (after any skipped non-options) as an argument. */
static char *take_next(int argc, char **argv)
{
	int j = optind + gap;

	if (j >= argc)
		return NULL;
	rotate(argv, j, optind);
	return argv[optind++];
}

static int long_option(int argc, char **argv, const char *name,
		       const struct option *longopts, int *longindex)
{
	size_t n = strcspn(name, "=");
	int i;

	for (i = 0; longopts != NULL && longopts[i].name != NULL; i++)
		if (strlen(longopts[i].name) == n &&
		    strncmp(longopts[i].name, name, n) == 0)
			break;
	if (longopts == NULL || longopts[i].name == NULL) {
		if (opterr)
			fprintf(stderr, "%s: unrecognized option '--%s'\n",
				argv[0], name);
		optopt = 0;
		return '?';
	}
	if (name[n] == '=') {
		if (longopts[i].has_arg == no_argument) {
			if (opterr)
				fprintf(stderr, "%s: option '--%s' doesn't "
					"allow an argument\n",
					argv[0], longopts[i].name);
			optopt = longopts[i].val;
			return '?';
		}
		optarg = (char *) name + n + 1;
	} else if (longopts[i].has_arg == required_argument) {
		optarg = take_next(argc, argv);
		if (optarg == NULL) {
			if (opterr)
				fprintf(stderr, "%s: option '--%s' requires "
					"an argument\n",
					argv[0], longopts[i].name);
			optopt = longopts[i].val;
			return '?';
		}
	}
	if (longindex != NULL)
		*longindex = i;
	if (longopts[i].flag != NULL) {
		*longopts[i].flag = longopts[i].val;
		return 0;
	}
	return longopts[i].val;
}

int getopt_long(int argc, char **argv, const char *shortopts,
		const struct option *longopts, int *longindex)
{
	const char *p;
	int c;

	optarg = NULL;
	if (nextchar == NULL || *nextchar == '\0') {
		int j = optind;

		nextchar = NULL;
		while (j < argc && !is_option(argv[j]))
			j++;
		if (j >= argc)
			return -1;
		gap = j - optind;
		rotate(argv, j, optind);
		optind++;
		if (strcmp(argv[optind - 1], "--") == 0)
			return -1;
		if (argv[optind - 1][1] == '-')
			return long_option(argc, argv, argv[optind - 1] + 2,
					   longopts, longindex);
		nextchar = argv[optind - 1] + 1;
	}

	c = (unsigned char) *nextchar++;
	p = strchr(shortopts, c);
	if (c == ':' || p == NULL) {
		if (opterr)
			fprintf(stderr, "%s: invalid option -- '%c'\n",
				argv[0], c);
		optopt = c;
		return '?';
	}
	if (p[1] == ':') {
		if (*nextchar != '\0') {
			optarg = nextchar;
		} else {
			optarg = take_next(argc, argv);
			if (optarg == NULL) {
				if (opterr)
					fprintf(stderr, "%s: option requires "
						"an argument -- '%c'\n",
						argv[0], c);
				optopt = c;
				nextchar = NULL;
				return '?';
			}
		}
		nextchar = NULL;
	}
	return c;
}
