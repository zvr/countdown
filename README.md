# countdown
Basic implementation of the TV game

A command-line utility that solves Countdown number and letter rounds.
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

Defaults can be changed at compile time with `-D` (`/D` for MSVC):

| Macro           | Default       | Meaning                                  |
|-----------------|---------------|------------------------------------------|
| `DEFAULT_DICT`  | `"words.txt"` | dictionary file for letters rounds       |
| `DEFAULT_COUNT` | `50`          | maximum number of words shown            |
| `MAX_NUMBERS`   | `16`          | maximum numbers accepted in number rounds |
| `MAX_EXPR`      | `1024`        | maximum length of a solution expression  |

For example:

    cc -O2 -DDEFAULT_DICT='"/usr/share/dict/words"' -DDEFAULT_COUNT=20 -o countdown countdown.c

## Usage

    countdown number... target
    countdown -r|--random [-l|--large n]
    countdown [-d|--dict file] [-n|--count n] letters...

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

### Letters round

If the arguments are letters, a letters round is solved instead.
Letters can be given individually (`c h a`) or as strings (`cha`), in any case.
The program lists the words from a dictionary file that can be formed
from the letters, each letter used at most as many times as it appears,
longest first (ties sorted alphabetically), with their length:

    $ countdown c h a r t e s o p
    chapter (7)
    patches (7)
    charts (6)
    ...

- `-d file` / `--dict file`: word list, one word per line
  (default `words.txt` in the current directory); case is ignored and
  words containing non-letters are skipped.
- `-n n` / `--count n`: maximum number of words to show (default 50).

## License

GPL-3.0-or-later
