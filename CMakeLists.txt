# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Ether Form Inc.
cmake_minimum_required(VERSION 3.15)
include(FetchContent)

# STM  builds the firmware for the STM32G0B1 (needs arm-none-eabi-gcc).
# HOST builds the unit tests for the PC.
set(ARCH "STM" CACHE STRING "Target architecture: STM or HOST")

# The firmware version: reported in GetNodeInfo (major and minor, with the git commit) and stored in flash (see
# board/Fw_info.h). Change it here.
set(FIRMWARE_VERSION_MAJOR 0)
set(FIRMWARE_VERSION_MINOR 3)
set(FIRMWARE_VERSION_PATCH 0)

if(${ARCH} STREQUAL "STM")
  # The default build types do not fit in the 122 KB flash region, so default to the smallest
  if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE MinSizeRel CACHE STRING "Build type" FORCE)
  endif()
  set(CMAKE_TOOLCHAIN_FILE ../stm32-toolchain.cmake CACHE STRING "" FORCE)
  project(can_pwm_firmware LANGUAGES C CXX ASM)

  FetchContent_Declare(
    stm32_g0xx
    GIT_REPOSITORY https://github.com/STMicroelectronics/stm32g0xx-hal-driver.git
    GIT_TAG 289ef6c8ff0922878e99d253f29be76851f8f28e
    SOURCE_DIR ${CMAKE_BINARY_DIR}/stm32_g0xx
  )
  FetchContent_Populate(stm32_g0xx)

  add_subdirectory(stm_generated)
elseif(${ARCH} STREQUAL "HOST")
  project(can_pwm_host_tests LANGUAGES C CXX)
  add_subdirectory(tests)
else()
  message(FATAL_ERROR "Specified architecture type not supported. Use \"-DARCH=<STM,HOST>\".")
endif()
