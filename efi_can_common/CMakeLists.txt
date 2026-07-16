set(PROJECT_EFI_COMMON ${PROJECT_NAME}_common)
set(PROJECT_EFI_COMMON_TEST ${PROJECT_NAME}_common_test)


#file(GLOB_RECURSE efi_common_files_usb
#  ${CMAKE_SOURCE_DIR}/efi_can_common/usb_interface/*.cpp
#)

file(GLOB_RECURSE efi_common_files_test
  ${CMAKE_SOURCE_DIR}/efi_can_common/tests/*.cpp
)

#add_library(${PROJECT_EFI_COMMON} ${efi_common_files_usb})

add_executable(${PROJECT_EFI_COMMON_TEST} ${efi_common_files_test})

#target_include_directories(${PROJECT_EFI_COMMON_TEST} PUBLIC
#  ${CMAKE_SOURCE_DIR}/efi_can_common
#)

#target_include_directories(${PROJECT_EFI_COMMON} PUBLIC
#  ${CMAKE_SOURCE_DIR}/efi_can_common
#)
#
#
#TODO fetch contnent and build once by linking this through to the other stuff?
target_include_directories(${PROJECT_EFI_COMMON_TEST} PRIVATE
  ${CMAKE_SOURCE_DIR}/efi_can_common

  ${CMAKE_SOURCE_DIR}/board
  ${CMAKE_SOURCE_DIR}/tests
  ${CMAKE_SOURCE_DIR}/tests/cli_app
  ${CMAKE_BINARY_DIR}/libuavcan/libuavcan/include
  ${CMAKE_BINARY_DIR}/libuavcan/libuavcan/include/dsdlc_generated
  ${CMAKE_SOURCE_DIR}/messages/compiled
  ${CMAKE_SOURCE_DIR}/efi_can_common
#  ${PROJECT_EFI_COMMON}
)

target_compile_definitions(${PROJECT_EFI_COMMON_TEST} PRIVATE
    UAVCAN_CPP_VERSION=UAVCAN_CPP11
)

target_link_libraries(${PROJECT_EFI_COMMON_TEST} PUBLIC
  uavcan
  gtest
  gtest_main
)
