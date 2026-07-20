cmake_minimum_required(VERSION 3.15)

project(can_to_pwm_firmware)

include(FetchContent)

add_subdirectory(messages)
add_subdirectory(tests)
add_subdirectory(efi_can_common)


#STM Build
project(can_pwm LANGUAGES C CXX ASM)

include(FetchContent)

FetchContent_Declare(
  stm32_g0xx
  GIT_REPOSITORY https://github.com/STMicroelectronics/stm32g0xx-hal-driver.git
  GIT_TAG master
  SOURCE_DIR ${CMAKE_BINARY_DIR}/stm32_g0xx
)
FetchContent_Populate(stm32_g0xx)

add_subdirectory(stm_generated)
