# countdown
Basic implementation of the TV game

A command-line utility that solves Countdown number and letter rounds.
Given a list of numbers and a target, it finds a combination of
addition, subtraction, multiplication and division that reaches the
target, or gets as close to it as possible.
Among equally good results, one using the fewest operations is shown.

Rules:
- not all numbers need to be used;
- a number may not be used more times than it appears in the input;
- only exact division is allowed;
- fractions and negative numbers are not allowed.

## Building

Source layout:

- `countdown.c` -- command-line handling
- `numbers.c`, `numbers.h` -- numbers round solver and random puzzles
- `letters.c`, `letters.h` -- letters round solver
- `compat/getopt.c`, `compat/getopt.h` -- `getopt_long()` for MSVC,
  which lacks `<getopt.h>`

With make (GCC or Clang):

    make
    make check
    make install PREFIX=/usr/local

With CMake (any platform, including Visual Studio):

    cmake -B build
    cmake --build build --config Release

With MSVC directly (from a Developer Command Prompt):

    cl /nologo /W4 /O2 countdown.c numbers.c letters.c compat\getopt.c

Defaults can be changed at compile time:

| Macro           | Default       | Meaning                                   |
|-----------------|---------------|-------------------------------------------|
| `DEFAULT_DICT`  | `"words.txt"` | dictionary file for letters rounds        |
| `DEFAULT_COUNT` | `50`          | maximum number of words shown             |
| `MAX_NUMBERS`   | `16`          | maximum numbers accepted in number rounds |
| `MAX_EXPR`      | `1024`        | maximum length of a solution expression   |
| `MAX_LETTERS`   | `255`         | maximum letters in a letters round        |
| `TT_BITS`       | `18`          | log2 of the solver's state cache entries  |
| `VERSION`       | `"0.1"`       | version reported by `--version`           |

For example:

    make CFLAGS='-O2 -DDEFAULT_DICT=\"/usr/share/dict/words\" -DDEFAULT_COUNT=20'
    cmake -B build -DDEFAULT_DICT=/usr/share/dict/words -DDEFAULT_COUNT=20

## Usage

    countdown [options] number... target
    countdown [options] -r|--random [-l|--large n] [-s|--seed n]
    countdown [options] letters...

Options may appear anywhere on the command line, short options can be
combined (`-rl2`), and long options accept `--name=value`.
`-h` / `--help` shows a summary; `-V` / `--version` prints the version.

All arguments must be positive integers; the last one is the target
(at most 16 numbers are supported).

With `-r` / `--random`, a puzzle is generated following the TV show rules
(six numbers: 0-4 large ones from 25, 50, 75, 100 and the rest small ones
from 1-10, each at most twice; target between 100 and 999), printed, and solved:

    $ countdown -r
    100 2 2 5 10 9 -> 716   (seed 1147596281817677428)
    (100 + 2) * (9 - 2) + 10 / 5 = 716

Each run uses a different seed; pass it back with `-s n` / `--seed n`
to get the same puzzle again:

    $ countdown -r -s 42
    75 3 9 5 9 2 -> 713   (seed 42)
    (75 + 3) * 9 + 2 + 9 = 713

Use `-l n` / `--large n` (0-4) together with `-r` to choose how many
large numbers the random puzzle contains (random by default):

    $ countdown -r -l 4 -s 42
    100 50 75 25 2 6 -> 713   (seed 42)
    (100 - 75 + 6) * (25 - 2) = 713

Examples:

    $ countdown 3 8 7 2 50 75 812
    (50 + 8) * 7 * 2 = 812

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

The bundled `words.txt` is only a small sample for testing.
For real use, point `-d` at a full word list, e.g. `/usr/share/dict/words`
on most Unix systems, or a list such as
[SCOWL](http://wordlist.aspell.net/) or
[dwyl/english-words](https://github.com/dwyl/english-words).

## License

GPL-3.0-or-later
