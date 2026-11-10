# countdown -- CMake build
#
# Copyright (C) 2026 Alexios Zavras
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Compile-time defaults can be overridden, e.g.:
#   cmake -B build -DDEFAULT_DICT=/usr/share/dict/words -DDEFAULT_COUNT=20

cmake_minimum_required(VERSION 3.10)
project(countdown VERSION 0.1 LANGUAGES C)

add_executable(countdown countdown.c numbers.c letters.c)

if(MSVC)
  string(REGEX REPLACE "/W[0-4]" "" CMAKE_C_FLAGS "${CMAKE_C_FLAGS}")
  target_sources(countdown PRIVATE compat/getopt.c)
  target_compile_options(countdown PRIVATE /W4)
else()
  target_compile_options(countdown PRIVATE -Wall -Wextra)
endif()

target_compile_definitions(countdown PRIVATE
  VERSION="${PROJECT_VERSION}")

foreach(def DEFAULT_COUNT DEFAULT_TIME_LIMIT MAX_NUMBERS MAX_EXPR MAX_LETTERS TT_BITS)
  if(DEFINED ${def})
    target_compile_definitions(countdown PRIVATE ${def}=${${def}})
  endif()
endforeach()
if(DEFINED DEFAULT_DICT)
  target_compile_definitions(countdown PRIVATE DEFAULT_DICT="${DEFAULT_DICT}")
endif()
if(DEFINED INDEX_SUFFIX)
  target_compile_definitions(countdown PRIVATE INDEX_SUFFIX="${INDEX_SUFFIX}")
endif()

include(GNUInstallDirs)
install(TARGETS countdown RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(FILES countdown.1 DESTINATION ${CMAKE_INSTALL_MANDIR}/man1)

# e.g. -DSANITIZE=address,undefined (GCC/Clang) or -DSANITIZE=address (MSVC)
set(SANITIZE "" CACHE STRING "sanitizers to build with")
if(SANITIZE)
  if(MSVC)
    target_compile_options(countdown PRIVATE /fsanitize=${SANITIZE} /Zi)
  else()
    target_compile_options(countdown PRIVATE -fsanitize=${SANITIZE}
      -fno-omit-frame-pointer -fno-sanitize-recover=all -g)
    target_link_options(countdown PRIVATE -fsanitize=${SANITIZE})
  endif()
endif()

enable_testing()
find_program(SH_PROGRAM NAMES sh bash
  HINTS "$ENV{ProgramFiles}/Git/bin" "$ENV{ProgramFiles}/Git/usr/bin")
if(SH_PROGRAM)
  add_test(NAME regression
    COMMAND ${SH_PROGRAM} ${CMAKE_CURRENT_SOURCE_DIR}/tests/run.sh
            $<TARGET_FILE:countdown> ${CMAKE_CURRENT_SOURCE_DIR}/words.txt)
endif()
