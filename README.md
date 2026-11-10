# countdown
Basic implementation of the TV game

A command-line utility that solves Countdown number rounds.
Given a list of numbers and a target, it finds a combination of
addition, subtraction, multiplication and division that reaches the
target, or gets as close to it as possible.

Rules:
- not all numbers need to be used;
- a number may not be used more times than it appears in the input;
- only exact division is allowed;
- fractions and negative numbers are not allowed.

## Building

With GCC or Clang:

    cc -Wall -Wextra -O2 -o countdown countdown.c

With MSVC (from a Developer Command Prompt):

    cl /nologo /W4 /O2 countdown.c

## Usage

    countdown number... target
    countdown -r|--random [-l|--large n]

All arguments must be positive integers; the last one is the target
(at most 16 numbers are supported).

With `-r` / `--random`, a puzzle is generated following the TV show rules
(six numbers: 0-4 large ones from 25, 50, 75, 100 and the rest small ones
from 1-10, each at most twice; target between 100 and 999), printed, and solved:

    $ countdown -r
    100 2 8 5 1 7 -> 760
    (100 + 2 - 7) * 8 = 760

Use `-l n` / `--large n` (0-4) together with `-r` to choose how many
large numbers the random puzzle contains (random by default):

    $ countdown -r -l 0
    4 6 9 1 10 5 -> 455
    (10 * 9 + 1) * (6 + 4 - 5) = 455

Examples:

    $ countdown 3 8 7 2 50 75 812
    (75 - (8 + 3) + 50 + 2) * 7 = 812

    $ countdown 25 50 75 100 3 6 952
    (75 * 3 * (100 + 6) - 50) / 25 = 952

If the target cannot be reached exactly, the closest result is shown:

    $ countdown 1 2 3 1000
    (2 + 1) * 3 = 9

## License

GPL-3.0-or-later
