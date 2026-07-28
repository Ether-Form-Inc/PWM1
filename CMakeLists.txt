cmake_minimum_required(VERSION 3.15)
include(FetchContent)


set(LIBUAVCAN_SRC 
    "${CMAKE_BINARY_DIR}/libuavcan_src" 
    CACHE STRING "Path to libuavcan source" FORCE
)

set(LIBUAVCAN_BUILD 
    "${CMAKE_BINARY_DIR}/libuavcan_build" 
    CACHE STRING "Path to libuavcan build dir" FORCE
)

set(EFI_CAN_COMMON_SRC 
    "${CMAKE_BINARY_DIR}/efi_can_common_src" 
    CACHE STRING "Path to efi_can_common source" FORCE
)

set(EFI_CAN_COMMON_BUILD 
    "${CMAKE_BINARY_DIR}/efi_can_common_build" 
    CACHE STRING "Path to efi_can_common build dir" FORCE
)

FetchContent_Declare(
  efi_can_common 
  GIT_REPOSITORY git@server2.internal.etherform.com:hardware/efi_can_common.git 
  GIT_TAG        main 
  GIT_SHALLOW    TRUE
  SOURCE_DIR     ${EFI_CAN_COMMON_SRC}
  BINARY_DIR     ${EFI_CAN_COMMON_BUILD}
)
FetchContent_Populate(efi_can_common)

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
elseif(${ARCH} STREQUAL "POSIX")

  FetchContent_Declare(
    libuavcan
    GIT_REPOSITORY git@server2.internal.etherform.com:hardware/efi_libuavcan.git 
    GIT_TAG        EFI_main
    GIT_SHALLOW    TRUE
    SOURCE_DIR     ${LIBUAVCAN_SRC}
    BINARY_DIR     ${LIBUAVCAN_BUILD}
  )
  FetchContent_MakeAvailable(libuavcan)

  project(can_to_pwm_firmware_test)
  add_subdirectory(messages)
  add_subdirectory(tests)
else()
  message(FATAL_ERROR "Specified architecture type not supported. Use \"-DARCH=<POSIX,STM>\".")
endif()



