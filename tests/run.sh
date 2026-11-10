#!/bin/sh
# countdown -- regression tests
#
# Copyright (C) 2026 Alexios Zavras
# SPDX-License-Identifier: GPL-3.0-or-later
#
# usage: tests/run.sh path/to/countdown [path/to/words.txt]
#
# Optionally, RUNNER is prefixed to each invocation (e.g. "valgrind -q
# --error-exitcode=99").

prog=$1
dict=${2:-words.txt}
fail=0
count=0

if [ -z "$prog" ]; then
	echo "usage: $0 countdown [words.txt]" >&2
	exit 2
fi

# expect DESCRIPTION EXPECTED-OUTPUT ARGS...: stdout must match exactly
expect() {
	desc=$1
	want=$2
	shift 2
	count=$((count + 1))
	got=$($RUNNER "$prog" "$@" 2>/dev/null)
	status=$?
	got=$(printf '%s' "$got" | tr -d '\r')	# Windows line endings
	if [ $status -ne 0 ] || [ "$got" != "$want" ]; then
		printf 'FAIL: %s\n  args: %s\n  want: %s\n  got:  %s (exit %d)\n' \
			"$desc" "$*" "$want" "$got" $status
		fail=$((fail + 1))
	fi
}

# expect_status DESCRIPTION STATUS ARGS...: only the exit status is checked
expect_status() {
	desc=$1
	want=$2
	shift 2
	count=$((count + 1))
	$RUNNER "$prog" "$@" >/dev/null 2>&1
	status=$?
	if [ $status -ne "$want" ]; then
		printf 'FAIL: %s\n  args: %s\n  want exit %d, got %d\n' \
			"$desc" "$*" "$want" $status
		fail=$((fail + 1))
	fi
}

# expect_lines DESCRIPTION N ARGS...: stdout must have N lines
expect_lines() {
	desc=$1
	want=$2
	shift 2
	count=$((count + 1))
	got=$($RUNNER "$prog" "$@" 2>/dev/null | wc -l | tr -d ' ')
	if [ "$got" != "$want" ]; then
		printf 'FAIL: %s\n  args: %s\n  want %s lines, got %s\n' \
			"$desc" "$*" "$want" "$got"
		fail=$((fail + 1))
	fi
}

# numbers round
expect "shortest 812" "(50 + 8) * 7 * 2 = 812" 3 8 7 2 50 75 812
expect "simplest 952" "(100 + 3) * 75 * 6 / 50 + 25 = 952" 25 50 75 100 3 6 952
expect "765" "(50 + 1) * (25 - 10) = 765" 1 3 7 10 25 50 765
expect "closest" "(2 + 1) * 3 = 9" 1 2 3 1000
expect "single number" "3 = 3" 9 3 3
expect "one step" "4 * 2 = 8" 4 2 8

# all solutions
expect_lines "all 812" 9 -a 3 8 7 2 50 75 812
expect_lines "all 18 limited" 6 -a -n 5 1 1 2 2 3 3 18
expect "all 8" "5 + 3 = 8
10 + 3 - 5 = 8" -a 10 5 3 8

# random puzzles are reproducible
expect "random seed" "75 3 9 5 9 2 -> 713   (seed 42)
(75 + 3) * 9 + 2 + 9 = 713" -r -s 42
expect "random large" "100 50 75 25 2 6 -> 713   (seed 42)
(100 - 75 + 6) * (25 - 2) = 713" -r -l 4 -s 42

# time limit always yields an answer
expect_lines "time limit" 1 -t 0.2 1 2 3 4 5 6 7 8 9 10 11 12 9973
expect_lines "wall time limit" 1 -w -t 0.2 1 2 3 4 5 6 7 8 9 10 11 12 9973

# letters round
expect "letters" "chapters (8)
poachers (8)" -d "$dict" -n 2 chartesop
expect "conundrum" "chapters (8)" -d "$dict" -C c h a p t e r s
expect "conundrum none" "no words found" -d "$dict" -C abcdefghi

# command line
expect_status "help" 0 --help
expect_status "version" 0 -V
expect_status "invalid option" 1 --bogus
expect_status "invalid target" 1 1 2 x3
expect_status "zero" 1 0 5 5
expect_status "large without random" 1 -l 2 1 2 3
expect_status "bad large" 1 -r -l 5
expect_status "all with letters" 1 -a -d "$dict" abc
expect_status "conundrum with numbers" 1 -C 1 2 3
expect_status "bad time" 1 -t 0 1 2 3
expect_status "missing dict" 1 -d does-not-exist.txt abc

echo "$((count - fail)) of $count tests passed"
[ $fail -eq 0 ]
