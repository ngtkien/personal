# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(zephyr_esp32)

include_directories(include)
FILE(GLOB app_sources src/*.c)
FILE(GLOB ra8875 src/ra8875/*.c)
message("app_sources: ${app_sources}")
target_sources(app PRIVATE ${app_sources} ${ra8875})
target_include_directories(app PRIVATE ${CMAKE_CURRENT_LIST_DIR}/src/ra8875)