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

#include "letters.h"

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

#ifdef _WIN32
typedef struct _stat64 file_stat;
  #define get_stat _stat64
#else
typedef struct stat file_stat;
  #define get_stat stat
#endif

/* Longest word considered: dictionary lines are read into 256 bytes. */
#define MAX_WORD 254

#define INDEX_VERSION 1

struct word_list {
    char **words;
    size_t count, size;
};

static
int
add_word(struct word_list *wl, const char *w, size_t len)
{
    char *copy;

    if (wl->count == wl->size) {
	size_t size = wl->size ? wl->size * 2 : 256;
	char **p    = realloc(wl->words, size * sizeof *p);

	if (p == NULL) return 0;
	wl->words = p;
	wl->size  = size;
    }
    copy = malloc(len + 1);
    if (copy == NULL) return 0;
    memcpy(copy, w, len);
    copy[len]              = '\0';
    wl->words[wl->count++] = copy;
    return 1;
}

static
void
free_words(struct word_list *wl)
{
    size_t i;

    for (i = 0; i < wl->count; i++) free(wl->words[i]);
    free(wl->words);
    wl->words = NULL;
    wl->count = wl->size = 0;
}

/* Lowercase w in place; returns 0 if it contains anything but letters. */
static
int
lower_word(char *w, size_t len)
{
    size_t i;

    for (i = 0; i < len; i++) {
	int c = tolower((unsigned char) w[i]);

	if (c < 'a' || c > 'z') return 0;
	w[i] = (char) c;
    }
    return 1;
}

/* The anagram key of w: its letters in alphabetical order. */
static
void
make_key(const char *w, size_t len, char *key)
{
    int    count[26] = {0};
    size_t i;
    int    c;

    for (i = 0; i < len; i++) count[w[i] - 'a']++;
    for (i = 0, c = 0; c < 26; c++)
	while (count[c]-- > 0) key[i++] = (char) ('a' + c);
    key[i] = '\0';
}

/* Can w be formed from the available letters? */
static
int
fits(const int *avail, const char *w, size_t len)
{
    int    need[26] = {0};
    size_t i;

    for (i = 0; i < len; i++) {
	int c = w[i] - 'a';

	if (c < 0 || c >= 26 || ++need[c] > avail[c]) return 0;
    }
    return 1;
}

static
int
cmp_words(const void *a, const void *b)
{
    const char *wa = *(const char * const *) a;
    const char *wb = *(const char * const *) b;
    size_t      la = strlen(wa), lb = strlen(wb);

    if (la != lb) return la < lb ? 1 : -1;
    return strcmp(wa, wb);
}

static
char *
index_path(const char *dict)
{
    size_t len  = strlen(dict);
    char  *path = malloc(len + sizeof INDEX_SUFFIX);

    if (path != NULL) {
	memcpy(path, dict, len);
	memcpy(path + len, INDEX_SUFFIX, sizeof INDEX_SUFFIX);
    }
    return path;
}

/*
 * Index file format (text, '\n' line endings):
 *
 *	countdown-index 1
 *	source SIZE MTIME
 *	maxlen M
 *	offsets O1 O2 ... O(M+1)
 *	KEY WORD
 *	...
 *
 * SIZE and MTIME identify the version of the word list the index was built
 * from. The entries are the valid words of the list, lowercased, without
 * duplicates, sorted by length, then anagram key, then word; OL is the
 * offset (from the first entry) of the first word with at least L letters.
 * Thus words longer than the given letters are never read, all words of a
 * given length can be found directly, and anagrams are adjacent.
 */
struct index {
    FILE *f;
    long  body; /* file offset of the first entry */
    int   maxlen;
    long  off[MAX_WORD + 2];
};

/*
 * Open the index of a word list, if present and up to date.
 * Returns 1 if usable, 0 if there is none, -1 if it is stale or invalid.
 */
static
int
open_index(const char      *prog,
           const char      *path,
           const file_stat *st,
           struct index    *ix)
{
    long long size, mtime;
    int       version, i;

    ix->f = fopen(path, "rb");
    if (ix->f == NULL) return 0;
    if (fscanf(ix->f,
               "countdown-index %d source %lld %lld maxlen %d "
               "offsets",
               &version, &size, &mtime, &ix->maxlen)
            != 4
        || version != INDEX_VERSION
        || ix->maxlen < 0
        || ix->maxlen > MAX_WORD)
	goto invalid;
    for (i = 1; i <= ix->maxlen + 1; i++)
	if (fscanf(ix->f, "%ld", &ix->off[i]) != 1 || ix->off[i] < 0)
	    goto invalid;
    if (fgetc(ix->f) != '\n') goto invalid;
    ix->body = ftell(ix->f);
    if (ix->body < 0) goto invalid;
    if (size != (long long) st->st_size || mtime != (long long) st->st_mtime) {
	fprintf(stderr,
	        "%s: index '%s' is out of date, ignoring it "
	        "(rebuild it with --index)\n",
	        prog, path);
	fclose(ix->f);
	return -1;
    }
    return 1;

  invalid:
    fprintf(stderr, "%s: index '%s' is not valid, ignoring it\n", prog, path);
    fclose(ix->f);
    return -1;
}

/* Collect matching words from the index; returns 0 if out of memory. */
static
int
query_index(struct index     *ix,
            const int        *avail,
            const char       *letters,
            size_t            nletters,
            int               conundrum,
            struct word_list *wl)
{
    char   line[2 * MAX_WORD + 4];
    char   sorted[MAX_WORD + 1], last_key[MAX_WORD + 1] = "";
    size_t hi = nletters < (size_t) ix->maxlen ? nletters : (size_t) ix->maxlen;
    int    last_ok = 0;
    long   pos, end;

    if (conundrum) {
	if (nletters > (size_t) ix->maxlen) return 1;
	make_key(letters, nletters, sorted);
	pos = ix->off[nletters];
    } else {
	pos = 0;
    }
    end = ix->off[hi + 1];
    if (pos >= end || fseek(ix->f, ix->body + pos, SEEK_SET) != 0) return 1;

    while (pos < end && fgets(line, sizeof line, ix->f) != NULL) {
	char  *key = line, *word;
	size_t len;

	pos                         += (long) strlen(line);
	line[strcspn(line, "\r\n")]  = '\0';
	word                         = strchr(line, ' ');
	if (word == NULL) continue;
	*word++ = '\0';
	len     = strlen(word);
	if (len == 0 || len > nletters || strlen(key) != len) continue;

	if (conundrum) {
	    int c = strcmp(key, sorted);

	    if (c < 0) continue;
	    if (c > 0) break;
	} else {
	    /* anagrams share their key: check each key once */
	    if (strcmp(key, last_key) != 0) {
		memcpy(last_key, key, len + 1);
		last_ok = fits(avail, key, len);
	    }
	    if (!last_ok) continue;
	}
	if (!add_word(wl, word, len)) return 0;
    }
    return 1;
}

/* Collect matching words by reading the whole word list. */
static
int
scan_dict(FILE             *f,
          const int        *avail,
          size_t            nletters,
          int               conundrum,
          struct word_list *wl)
{
    char   line[MAX_WORD + 2];
    size_t len;

    while (fgets(line, sizeof line, f) != NULL) {
	len       = strcspn(line, "\r\n");
	line[len] = '\0';
	/* words longer than the letters cannot match: skip early */
	if (len == 0 || len > nletters || (conundrum && len != nletters))
	    continue;
	if (lower_word(line, len)
	    && fits(avail, line, len)
	    && !add_word(wl, line, len))
	    return 0;
    }
    return 1;
}

int
letters_solve(const char *prog,
              const char *letters,
              const char *dict,
              long        max_words,
              int         conundrum)
{
    int              avail[26] = {0};
    size_t           nletters  = strlen(letters);
    struct word_list wl        = {NULL, 0, 0};
    struct index     ix;
    file_stat        st;
    char            *path;
    FILE            *f;
    size_t           i;
    long             shown;
    int              ok = 1, have_index = 0;

    for (i = 0; i < nletters; i++) avail[letters[i] - 'a']++;

    if (get_stat(dict, &st) != 0) {
	fprintf(stderr, "%s: cannot open dictionary '%s': %s\n", prog, dict,
	        strerror(errno));
	return EXIT_FAILURE;
    }
    path = index_path(dict);
    if (path != NULL) {
	have_index = open_index(prog, path, &st, &ix) == 1;
	free(path);
    }

    if (have_index) {
	ok = query_index(&ix, avail, letters, nletters, conundrum, &wl);
	fclose(ix.f);
    } else {
	f = fopen(dict, "r");
	if (f == NULL) {
	    fprintf(stderr, "%s: cannot open dictionary '%s': %s\n", prog, dict,
	            strerror(errno));
	    return EXIT_FAILURE;
	}
	ok = scan_dict(f, avail, nletters, conundrum, &wl);
	fclose(f);
    }
    if (!ok) fprintf(stderr, "%s: out of memory\n", prog);

    if (ok) {
	qsort(wl.words, wl.count, sizeof *wl.words, cmp_words);
	shown = 0;
	for (i = 0; i < wl.count && shown < max_words; i++) {
	    if (i > 0 && strcmp(wl.words[i], wl.words[i - 1]) == 0) continue;
	    printf("%s (%u)\n", wl.words[i], (unsigned) strlen(wl.words[i]));
	    shown++;
	}
	if (shown == 0) printf("no words found\n");
    }
    free_words(&wl);
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}

/* Index entries ("KEY WORD") sort by length, then key, then word. */
static
int
cmp_entries(const void *a, const void *b)
{
    const char *ea = *(const char * const *) a;
    const char *eb = *(const char * const *) b;
    size_t      la = strcspn(ea, " "), lb = strcspn(eb, " ");

    if (la != lb) return la < lb ? -1 : 1;
    return strcmp(ea, eb);
}

int
letters_build_index(const char *prog, const char *dict)
{
    struct word_list entries = {NULL, 0, 0};
    char             line[MAX_WORD + 2], entry[2 * MAX_WORD + 2];
    long             off[MAX_WORD + 2];
    file_stat        st;
    char            *path;
    FILE            *f;
    size_t           i, n, len;
    int              maxlen = 0, l, ok = 1;
    long             pos;

    f = fopen(dict, "r");
    if (f == NULL || get_stat(dict, &st) != 0) {
	fprintf(stderr, "%s: cannot open dictionary '%s': %s\n", prog, dict,
	        strerror(errno));
	if (f != NULL) fclose(f);
	return EXIT_FAILURE;
    }
    while (ok && fgets(line, sizeof line, f) != NULL) {
	len       = strcspn(line, "\r\n");
	line[len] = '\0';
	if (len == 0 || !lower_word(line, len)) continue;
	make_key(line, len, entry);
	entry[len] = ' ';
	memcpy(entry + len + 1, line, len + 1);
	ok = add_word(&entries, entry, 2 * len + 1);
	if ((int) len > maxlen) maxlen = (int) len;
    }
    fclose(f);
    if (!ok) {
	fprintf(stderr, "%s: out of memory\n", prog);
	free_words(&entries);
	return EXIT_FAILURE;
    }

    /* sort, drop duplicates, and find where each length starts */
    qsort(entries.words, entries.count, sizeof *entries.words, cmp_entries);
    for (i = n = 0; i < entries.count; i++) {
	if (n > 0 && strcmp(entries.words[i], entries.words[n - 1]) == 0)
	    free(entries.words[i]);
	else
	    entries.words[n++] = entries.words[i];
    }
    entries.count = n;
    for (l = 1, pos = 0, i = 0; l <= maxlen + 1; l++) {
	while (i < n && (int) strcspn(entries.words[i], " ") < l)
	    pos += (long) strlen(entries.words[i++]) + 1;
	off[l] = pos;
    }

    path = index_path(dict);
    f    = path != NULL ? fopen(path, "wb") : NULL;
    if (f == NULL) {
	fprintf(stderr, "%s: cannot create index '%s': %s\n", prog,
	        path != NULL ? path : dict, strerror(errno));
	free(path);
	free_words(&entries);
	return EXIT_FAILURE;
    }
    fprintf(f, "countdown-index %d\nsource %lld %lld\nmaxlen %d\noffsets",
            INDEX_VERSION, (long long) st.st_size, (long long) st.st_mtime,
            maxlen);
    for (l = 1; l <= maxlen + 1; l++) fprintf(f, " %ld", off[l]);
    fputc('\n', f);
    for (i = 0; i < n; i++) fprintf(f, "%s\n", entries.words[i]);
    if (ferror(f) | fclose(f)) {
	fprintf(stderr, "%s: cannot write index '%s'\n", prog, path);
	remove(path);
	ok = 0;
    } else {
	printf("%lu words indexed into '%s'\n", (unsigned long) n, path);
    }
    free(path);
    free_words(&entries);
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
