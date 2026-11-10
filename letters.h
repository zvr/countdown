/*
 * countdown -- letters round
 */

/*
 * Copyright (C) 2026 Alexios Zavras
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef COUNTDOWN_LETTERS_H
#define COUNTDOWN_LETTERS_H

#ifndef DEFAULT_DICT
  #define DEFAULT_DICT "words.txt"
#endif
#ifndef DEFAULT_COUNT
  #define DEFAULT_COUNT 50
#endif
#ifndef INDEX_SUFFIX
  #define INDEX_SUFFIX ".idx" /* index of a word list: its name + this */
#endif

/*
 * List the words of the dictionary file that can be formed from the given
 * (lowercase) letters, each letter used at most as often as it appears,
 * longest first, up to max_words of them.
 * If conundrum is nonzero, only words using all the letters are listed.
 * If an up-to-date index of the dictionary exists, it is used instead.
 * Returns EXIT_SUCCESS or EXIT_FAILURE; prog is used in error messages.
 */
int letters_solve(const char *prog,
                  const char *letters,
                  const char *dict,
                  long        max_words,
                  int         conundrum);

/*
 * Build the index of the dictionary file (dict + INDEX_SUFFIX), recording
 * the dictionary's size and modification time to detect when it is stale.
 * Returns EXIT_SUCCESS or EXIT_FAILURE.
 */
int letters_build_index(const char *prog, const char *dict);

#endif
