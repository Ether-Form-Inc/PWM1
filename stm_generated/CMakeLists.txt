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

FetchContent_Declare(
  libuavcan_stm
  GIT_REPOSITORY git@server2.internal.etherform.com:hardware/efi_libuavcan.git 
  GIT_TAG        EFI_main 
  GIT_SHALLOW    FALSE
  SOURCE_DIR     ${LIBUAVCAN_SRC}
  BINARY_DIR     ${LIBUAVCAN_BUILD}
)
FetchContent_MakeAvailable(libuavcan_stm)

add_executable(${PROJECT_NAME} ${source})

set_target_properties(${PROJECT_NAME} PROPERTIES OUTPUT_NAME ${PROJECT_NAME}.elf)

target_include_directories(${PROJECT_NAME} PRIVATE 
    ${CMAKE_SOURCE_DIR}/board
    ${LIBUAVCAN_SRC}/libuavcan/include
    ${LIBUAVCAN_SRC}/libuavcan/include/dsdlc_generated
    ${CMAKE_SOURCE_DIR}/messages/compiled
    ${CMAKE_SOURCE_DIR}/efi_can_common
    ${CMAKE_CURRENT_LIST_DIR}/Core/Inc
)

target_compile_definitions(${PROJECT_NAME} PRIVATE
    UAVCAN_CPP_VERSION=UAVCAN_CPP11
)

target_compile_options(${PROJECT_NAME} PRIVATE
    --specs=nano.specs
    -ffunction-sections
    -fdata-sections
    -Os
)

target_link_options(${PROJECT_NAME} PRIVATE
    -T ${CMAKE_SOURCE_DIR}/stm_generated/STM32G0B1KBTX_FLASH.ld
    -u _printf_float
    -Wl,--gc-sections
    -Wl,-Map=firmware.map
)

target_link_libraries(${PROJECT_NAME} PRIVATE
  stm32cubeg0
  uavcan
)

set(PROGRAMMER openocd)
set(PROGRAMMER_FLAGS -f interface/stlink.cfg -f target/stm32g0x.cfg)

add_custom_target(flash
    COMMAND ${PROGRAMMER} ${PROGRAMMER_FLAGS} -c "program $<TARGET_FILE:${PROJECT_NAME}> verify reset exit"
    DEPENDS ${PROJECT_NAME}
    VERBATIM)
  
