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
  target_sources(countdown PRIVATE compat/getopt.c)
  target_compile_options(countdown PRIVATE /W4)
else()
  target_compile_options(countdown PRIVATE -Wall -Wextra)
endif()

target_compile_definitions(countdown PRIVATE
  VERSION="${PROJECT_VERSION}")

foreach(def DEFAULT_COUNT MAX_NUMBERS MAX_EXPR MAX_LETTERS)
  if(DEFINED ${def})
    target_compile_definitions(countdown PRIVATE ${def}=${${def}})
  endif()
endforeach()
if(DEFINED DEFAULT_DICT)
  target_compile_definitions(countdown PRIVATE DEFAULT_DICT="${DEFAULT_DICT}")
endif()

include(GNUInstallDirs)
install(TARGETS countdown RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
