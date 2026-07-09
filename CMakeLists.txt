cmake_minimum_required(VERSION 3.15)

project(can_to_pwm_firmware)

include(FetchContent)

#add_subdirectory(loopback_test)
#add_subdirectory(stm_generated)
#add_subdirectory(tests)
#add_subdirectory(efi_can_translator)
#add_subdirectory(board)


add_subdirectory(lib)
add_subdirectory(loopback)
add_subdirectory(messages)
add_subdirectory(tests)
