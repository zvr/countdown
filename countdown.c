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
*/

