set(CMAKE_SYSTEM_NAME Generic)

set(CMAKE_C_COMPILER /usr/bin/arm-none-eabi-gcc)
set(CMAKE_CXX_COMPILER /usr/bin/arm-none-eabi-g++)
set(CMAKE_ASM_COMPILER /usr/bin/arm-none-eabi-gcc) 

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -mcpu=cortex-m0plus -mthumb")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -mcpu=cortex-m0plus -mthumb")

set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)
