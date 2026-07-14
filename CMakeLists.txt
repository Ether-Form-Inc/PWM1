cmake_minimum_required(VERSION 3.15)

project(can_to_pwm_firmware)

include(FetchContent)

#add_subdirectory(stm_generated)
#add_subdirectory(efi_can_translator)
#add_subdirectory(board)


add_subdirectory(messages)
add_subdirectory(tests)
