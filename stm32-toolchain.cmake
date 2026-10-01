set(CMAKE_SYSTEM_NAME Generic)

# Find the toolchain on PATH, or in ARM_TOOLCHAIN_DIR (env or cache var), e.g. STM32CubeIDE's tools/bin
find_program(ARM_GCC arm-none-eabi-gcc HINTS $ENV{ARM_TOOLCHAIN_DIR} ${ARM_TOOLCHAIN_DIR} REQUIRED)
find_program(ARM_GXX arm-none-eabi-g++ HINTS $ENV{ARM_TOOLCHAIN_DIR} ${ARM_TOOLCHAIN_DIR} REQUIRED)
set(CMAKE_C_COMPILER ${ARM_GCC})
set(CMAKE_CXX_COMPILER ${ARM_GXX})
set(CMAKE_ASM_COMPILER ${ARM_GCC})

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -mcpu=cortex-m0plus -mthumb")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -mcpu=cortex-m0plus -mthumb")

set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)
