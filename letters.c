/*
 * countdown -- letters round
 */

/*
 * Copyright (C) 2026 Alexios Zavras
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifdef _MSC_VER
#define _CRT_SECURE_NO_WARNINGS
#endif

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "letters.h"

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

int letters_solve(const char *prog, const char *letters,
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
