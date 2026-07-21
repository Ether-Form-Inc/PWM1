cmake_minimum_required(VERSION 3.15)
include(FetchContent)

set(ARCH "STM" CACHE STRING "Target architecture")

if(${ARCH} STREQUAL "STM")
  set(CMAKE_TOOLCHAIN_FILE ../stm32-toolchain.cmake CACHE STRING "" FORCE)
  #STM Build
  project(can_pwm_firmware LANGUAGES C CXX ASM)
  FetchContent_Declare(
    stm32_g0xx
    GIT_REPOSITORY https://github.com/STMicroelectronics/stm32g0xx-hal-driver.git
    GIT_TAG master
    SOURCE_DIR ${CMAKE_BINARY_DIR}/stm32_g0xx
  )
  FetchContent_Populate(stm32_g0xx)

  add_subdirectory(stm_generated)
else()
  project(can_to_pwm_firmware_test)
  add_subdirectory(messages)
  add_subdirectory(tests)
  add_subdirectory(efi_can_common)
endif()



