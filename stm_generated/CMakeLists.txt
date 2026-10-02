file(GLOB hal_source ${stm32_g0xx_SOURCE_DIR}/Src/*.c)

list(FILTER hal_source EXCLUDE REGEX "template\\.c$")

add_library(stm32cubeg0 STATIC ${hal_source})

target_include_directories(stm32cubeg0 PUBLIC
    ${CMAKE_CURRENT_LIST_DIR}/Core/Inc
    ${CMAKE_BINARY_DIR}/stm32_g0xx/Inc/
    # Remove these for fetch content
    ${CMAKE_CURRENT_LIST_DIR}/Drivers/CMSIS/Device/ST/STM32G0xx/Include
    ${CMAKE_CURRENT_LIST_DIR}/Drivers/CMSIS/Include
)

target_compile_definitions(stm32cubeg0 PUBLIC STM32G0B1xx)

file(GLOB source 
  ${CMAKE_CURRENT_LIST_DIR}/Core/Src/*.c
  ${CMAKE_CURRENT_LIST_DIR}/Core/Src/*.cpp
  ${CMAKE_CURRENT_LIST_DIR}/Core/Startup/*.s
)

set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)


include(${CMAKE_SOURCE_DIR}/cmake/dronecan_c.cmake)

# fw_version.h holds the version and the git commit; it is regenerated at every build
set(FW_VERSION_HEADER ${CMAKE_BINARY_DIR}/generated/fw_version.h)
add_custom_target(fw_version ALL
    COMMAND ${CMAKE_COMMAND} -DSRC_DIR=${CMAKE_SOURCE_DIR} -DOUT=${FW_VERSION_HEADER}
            -DMAJOR=${FIRMWARE_VERSION_MAJOR} -DMINOR=${FIRMWARE_VERSION_MINOR} -DPATCH=${FIRMWARE_VERSION_PATCH}
            -P ${CMAKE_SOURCE_DIR}/cmake/gen_version.cmake
    BYPRODUCTS ${FW_VERSION_HEADER}
    VERBATIM)

add_executable(${PROJECT_NAME} ${source} ${CANARD_SRC_DIR}/canard.c ${DSDLC_SOURCES})
add_dependencies(${PROJECT_NAME} fw_version)

set_target_properties(${PROJECT_NAME} PROPERTIES OUTPUT_NAME ${PROJECT_NAME}.elf)

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_SOURCE_DIR}/board
    ${CMAKE_BINARY_DIR}/generated
    ${CANARD_SRC_DIR}
    ${DSDLC_GEN_DIR}/include
    ${CMAKE_CURRENT_LIST_DIR}/Core/Inc
)

# Minimum delay between transmitted CAN frames in ms (0 = none). The serial USB-CAN adapters used for bring-up and
# testing lose the end of a burst of frames (a 7 frame parameter reply came back cut short every time), so frames are
# spaced 1 ms apart. NodeStatus is a single frame and parameter or GetNodeInfo replies are rare, so this costs nothing
# on a real bus.
set(CAN_TX_MIN_SPACING_MS 1 CACHE STRING "Minimum ms between transmitted CAN frames")
if(CAN_TX_MIN_SPACING_MS GREATER 0)
  target_compile_definitions(${PROJECT_NAME} PRIVATE CAN_TX_MIN_SPACING_MS=${CAN_TX_MIN_SPACING_MS})
endif()

target_compile_options(${PROJECT_NAME} PRIVATE
    --specs=nano.specs
    -ffunction-sections
    -fdata-sections
    -Os
)

target_link_options(${PROJECT_NAME} PRIVATE
    -T ${CMAKE_SOURCE_DIR}/stm_generated/STM32G0B1KBTX_FLASH.ld
    -Wl,--gc-sections
    -Wl,-Map=firmware.map
)

target_link_libraries(${PROJECT_NAME} PRIVATE
  stm32cubeg0
)

# "make flash" programs the board over SWD and checks that it works (see tools/flash_and_check.py). It needs Python 3
# and OpenOCD (the copy in STM32CubeIDE is found automatically); the CAN check also needs pyserial and a USB-CAN
# adapter. Extra script arguments can be given with -DFLASH_ARGS, e.g. -DFLASH_ARGS="--can-device none".
find_program(FLASH_PYTHON NAMES python3 python)
set(FLASH_ARGS "" CACHE STRING "Extra arguments for tools/flash_and_check.py")
separate_arguments(FLASH_ARGS_LIST UNIX_COMMAND "${FLASH_ARGS}")

add_custom_target(flash
    COMMAND ${FLASH_PYTHON} ${CMAKE_SOURCE_DIR}/tools/flash_and_check.py --elf $<TARGET_FILE:${PROJECT_NAME}> ${FLASH_ARGS_LIST}
    DEPENDS ${PROJECT_NAME}
    USES_TERMINAL
    VERBATIM)
