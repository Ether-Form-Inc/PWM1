file(GLOB hal_source ${stm32_g0xx_SOURCE_DIR}/Src/*.c)

list(FILTER hal_source EXCLUDE REGEX "template\\.c$")

# file(RENAME ${CMAKE_BINARY_DIR}/stm32_g0xx/Inc/stm32g0xx_hal_conf_template.h ${CMAKE_BINARY_DIR}/stm32_g0xx/Inc/stm32g0xx_hal_conf.h)

set(CMAKE_C_COMPILER arm-none-eabi-gcc)
set(CMAKE_CXX_COMPILER arm-none-eabi-g++)
set(CMAKE_ASM_COMPILER arm-none-eabi-gcc) 

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -mcpu=cortex-m0plus -mthumb")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -mcpu=cortex-m0plus -mthumb")

add_library(stm32cubeg0 STATIC ${hal_source})

target_include_directories(stm32cubeg0 PUBLIC
    ${CMAKE_CURRENT_LIST_DIR}/Core/Inc
    ${CMAKE_BINARY_DIR}/stm32_g0xx/Inc/
    # Remove these for fetch content
    ${CMAKE_CURRENT_LIST_DIR}/Drivers/CMSIS/Device/ST/STM32G0xx/Include
    ${CMAKE_CURRENT_LIST_DIR}/Drivers/CMSIS/Include
)

target_compile_definitions(stm32cubeg0 PUBLIC STM32G0B1xx)

# add_compile_definitions(USE_HAL_DRIVER)

file(GLOB source 
  ${CMAKE_CURRENT_LIST_DIR}/Core/Src/*.c
  ${CMAKE_CURRENT_LIST_DIR}/Core/Startup/*.s
)

add_executable(${PROJECT_NAME} ${source})

set_target_properties(${PROJECT_NAME} PROPERTIES OUTPUT_NAME ${PROJECT_NAME}.elf)

target_include_directories(${PROJECT_NAME} PRIVATE 
    ${CMAKE_CURRENT_LIST_DIR}/Core/Inc
)

target_compile_options(${PROJECT_NAME} PRIVATE
    --specs=nano.specs
)

target_link_options(${PROJECT_NAME} PRIVATE
    -T ${CMAKE_SOURCE_DIR}/stm_generated/STM32G0B1KBTX_FLASH.ld
    -u _printf_float
)

target_link_libraries(${PROJECT_NAME} stm32cubeg0)

set(PROGRAMMER openocd)
set(PROGRAMMER_FLAGS -f interface/stlink.cfg -f target/stm32g0x.cfg)

add_custom_target(flash
    COMMAND ${PROGRAMMER} ${PROGRAMMER_FLAGS} -c "program $<TARGET_FILE:${PROJECT_NAME}> verify reset exit"
    DEPENDS ${PROJECT_NAME}
    VERBATIM)
  
