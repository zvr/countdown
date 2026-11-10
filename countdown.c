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

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _MSC_VER
#include "compat/getopt.h"
#else
#include <getopt.h>
#endif

#include "letters.h"
#include "numbers.h"

#ifndef VERSION
#define VERSION "0.1"
#endif
#ifndef MAX_LETTERS
#define MAX_LETTERS 255
#endif

#ifndef DEFAULT_TIME_LIMIT
#define DEFAULT_TIME_LIMIT 0	/* seconds; 0 means no limit */
#endif

static const char *prog = "countdown";
static int all_solutions;
static long max_count = DEFAULT_COUNT;

static void usage(FILE *f)
{
	fprintf(f,
		"usage: %s [options] number... target\n"
		"       %s [options] -r|--random [-l|--large n] [-s|--seed n]\n"
		"       %s [options] letters...\n"
		"\n"
		"options:\n"
		"  -r, --random       generate and solve a random numbers round\n"
		"  -l, --large n      number of large numbers (0-4) with --random\n"
		"  -s, --seed n       seed for --random, to reproduce a puzzle\n"
		"  -a, --all          list all exact solutions of a numbers round\n"
		"  -t, --time secs    time limit for solving a numbers round\n"
		"  -w, --wall         measure --time as elapsed, not CPU, time\n"
		"  -C, --conundrum    only words using all the letters\n"
		"  -d, --dict file    word list for letters rounds "
		"(default \"%s\")\n"
		"  -I, --index        build an index of the word list, "
		"to speed up letters rounds\n"
		"  -n, --count n      maximum number of words or solutions "
		"to show (default %d)\n"
		"  -h, --help         show this help and exit\n"
		"  -V, --version      show version and exit\n",
		prog, prog, prog, DEFAULT_DICT, DEFAULT_COUNT);
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

static int is_word_arg(const char *s)
{
	if (*s == '\0')
		return 0;
	for (; *s; s++)
		if (!isalpha((unsigned char) *s))
			return 0;
	return 1;
}

static void print_numbers(const long *numbers, int n, long target)
{
	char expr[MAX_EXPR];
	long value;

	if (all_solutions) {
		long found = numbers_solve_all(numbers, n, target, max_count);

		if (found > max_count)
			printf("(%ld of %ld solutions shown)\n",
			       max_count, found);
	} else {
		numbers_solve(numbers, n, target, expr, sizeof expr, &value);
		printf("%s = %ld\n", expr, value);
	}
	if (numbers_timed_out())
		fprintf(stderr, "%s: time limit reached, "
			"results may be incomplete\n", prog);
}

static int run_random(int nlarge, unsigned long long seed)
{
	long numbers[6], target;
	int n, i;

	n = numbers_random(numbers, &target, nlarge, seed);
	for (i = 0; i < n; i++)
		printf("%ld ", numbers[i]);
	printf("-> %ld   (seed %llu)\n", target, seed);
	print_numbers(numbers, n, target);
	return EXIT_SUCCESS;
}

static int run_letters(int argc, char **argv, const char *dict,
		       int conundrum)
{
	char letters[MAX_LETTERS + 1];
	size_t len = 0, k;
	int i;

	for (i = 0; i < argc; i++) {
		if (!is_word_arg(argv[i])) {
			fprintf(stderr, "%s: invalid letters '%s'\n",
				prog, argv[i]);
			return EXIT_FAILURE;
		}
		for (k = 0; argv[i][k]; k++) {
			if (len >= MAX_LETTERS) {
				fprintf(stderr, "%s: too many letters\n",
					prog);
				return EXIT_FAILURE;
			}
			letters[len++] = (char) tolower(
				(unsigned char) argv[i][k]);
		}
	}
	letters[len] = '\0';
	return letters_solve(prog, letters, dict, max_count, conundrum);
}

static int run_numbers(int argc, char **argv)
{
	long numbers[MAX_NUMBERS], target;
	int n = argc - 1;
	int i;

	if (n < 1) {
		usage(stderr);
		return EXIT_FAILURE;
	}
	if (n > MAX_NUMBERS) {
		fprintf(stderr, "%s: at most %d numbers are supported\n",
			prog, MAX_NUMBERS);
		return EXIT_FAILURE;
	}
	if (!parse_number(argv[n], &target)) {
		fprintf(stderr, "%s: invalid target '%s'\n", prog, argv[n]);
		return EXIT_FAILURE;
	}
	for (i = 0; i < n; i++) {
		if (!parse_number(argv[i], &numbers[i])) {
			fprintf(stderr, "%s: invalid number '%s'\n",
				prog, argv[i]);
			return EXIT_FAILURE;
		}
	}
	print_numbers(numbers, n, target);
	return EXIT_SUCCESS;
}

int main(int argc, char *argv[])
{
	static const struct option longopts[] = {
		{ "random",  no_argument,       NULL, 'r' },
		{ "large",   required_argument, NULL, 'l' },
		{ "seed",    required_argument, NULL, 's' },
		{ "all",     no_argument,       NULL, 'a' },
		{ "time",    required_argument, NULL, 't' },
		{ "wall",    no_argument,       NULL, 'w' },
		{ "conundrum", no_argument,     NULL, 'C' },
		{ "dict",    required_argument, NULL, 'd' },
		{ "index",   no_argument,       NULL, 'I' },
		{ "count",   required_argument, NULL, 'n' },
		{ "help",    no_argument,       NULL, 'h' },
		{ "version", no_argument,       NULL, 'V' },
		{ NULL,      0,                 NULL, 0 }
	};
	const char *dict = DEFAULT_DICT;
	double time_limit = DEFAULT_TIME_LIMIT;
	int random = 0, nlarge = -1, conundrum = 0, wall = 0, build_index = 0;
	int have_seed = 0;
	unsigned long long seed = 0;
	char *end;
	int c;

	if (argc > 0 && argv[0] != NULL && argv[0][0] != '\0')
		prog = argv[0];

	while ((c = getopt_long(argc, argv, "rl:s:at:wCd:In:hV",
				longopts, NULL)) != -1) {
		switch (c) {
		case 'r':
			random = 1;
			break;
		case 'a':
			all_solutions = 1;
			break;
		case 'w':
			wall = 1;
			break;
		case 'I':
			build_index = 1;
			break;
		case 'C':
			conundrum = 1;
			break;
		case 't':
			errno = 0;
			time_limit = strtod(optarg, &end);
			if (errno != 0 || end == optarg || *end != '\0' ||
			    !(time_limit > 0)) {
				fprintf(stderr, "%s: --time requires a "
					"positive number of seconds\n", prog);
				return EXIT_FAILURE;
			}
			break;
		case 's':
			errno = 0;
			seed = strtoull(optarg, &end, 10);
			if (errno != 0 || end == optarg || *end != '\0' ||
			    optarg[0] == '-') {
				fprintf(stderr, "%s: --seed requires a "
					"non-negative number\n", prog);
				return EXIT_FAILURE;
			}
			have_seed = 1;
			break;
		case 'l':
			if (strlen(optarg) != 1 ||
			    optarg[0] < '0' || optarg[0] > '4') {
				fprintf(stderr, "%s: --large requires a "
					"number between 0 and 4\n", prog);
				return EXIT_FAILURE;
			}
			nlarge = optarg[0] - '0';
			break;
		case 'd':
			dict = optarg;
			break;
		case 'n':
			if (!parse_number(optarg, &max_count)) {
				fprintf(stderr, "%s: --count requires a "
					"positive number\n", prog);
				return EXIT_FAILURE;
			}
			break;
		case 'h':
			usage(stdout);
			return EXIT_SUCCESS;
		case 'V':
			printf("countdown %s\n", VERSION);
			return EXIT_SUCCESS;
		default:
			usage(stderr);
			return EXIT_FAILURE;
		}
	}
	argc -= optind;
	argv += optind;
	numbers_set_time_limit(time_limit, wall);

	if (build_index) {
		if (argc > 0 || random) {
			usage(stderr);
			return EXIT_FAILURE;
		}
		return letters_build_index(prog, dict);
	}

	if (random) {
		if (argc > 0 || conundrum) {
			usage(stderr);
			return EXIT_FAILURE;
		}
		return run_random(nlarge,
				  have_seed ? seed : numbers_auto_seed());
	}
	if (nlarge >= 0 || have_seed) {
		fprintf(stderr, "%s: -l/--large and -s/--seed are only "
			"valid with -r/--random\n", prog);
		return EXIT_FAILURE;
	}
	if (argc > 0 && is_word_arg(argv[0])) {
		if (all_solutions) {
			fprintf(stderr, "%s: -a/--all is only valid for "
				"numbers rounds\n", prog);
			return EXIT_FAILURE;
		}
		return run_letters(argc, argv, dict, conundrum);
	}
	if (conundrum) {
		fprintf(stderr, "%s: -C/--conundrum is only valid for "
			"letters rounds\n", prog);
		return EXIT_FAILURE;
	}
	return run_numbers(argc, argv);
}
