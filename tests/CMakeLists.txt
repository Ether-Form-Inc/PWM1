# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Ether Form Inc.
# Unit tests that run on the PC: the pulse width limits, the command to pulse width arithmetic and the parameter table.
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_Declare(
  gtest
  GIT_REPOSITORY https://github.com/google/googletest.git
  GIT_TAG        v1.15.2
  SOURCE_DIR     ${CMAKE_BINARY_DIR}/gtest
  BINARY_DIR     ${CMAKE_BINARY_DIR}/gtest_build
)
FetchContent_MakeAvailable(gtest)

add_executable(pwm_tests ${CMAKE_CURRENT_LIST_DIR}/pwm_tests/pwm_config_tests.cpp)
target_compile_features(pwm_tests PRIVATE cxx_std_17)
target_include_directories(pwm_tests PRIVATE ${CMAKE_SOURCE_DIR}/board)
target_link_libraries(pwm_tests PRIVATE gtest gtest_main)
