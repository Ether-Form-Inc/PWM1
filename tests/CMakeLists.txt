set(PROJECT_CLI_APP ${PROJECT_NAME}_CLI)
set(PROJECT_TEST_NAME ${PROJECT_NAME}_tests)
set(PROJECT_LOOPBACK ${PROJECT_NAME}_loopback_test)

set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)

FetchContent_Declare(
  gtest
  GIT_REPOSITORY https://github.com/google/googletest.git
  GIT_TAG "main"
  SOURCE_DIR ${CMAKE_BINARY_DIR}/gtest
  BINARY_DIR ${CMAKE_BINARY_DIR}/gtest_build
)
FetchContent_MakeAvailable(gtest)

#***************** Build UAVCAN target **************
#libuavcan has some cmake warnings here regarding python so we suppress them for this project
set(CMAKE_SUPPRESS_DEVELOPER_WARNINGS ON CACHE BOOL "" FORCE)

target_compile_definitions(uavcan PRIVATE
    UAVCAN_CPP_VERSION=UAVCAN_CPP11
)

set(CMAKE_SUPPRESS_DEVELOPER_WARNINGS OFF CACHE BOOL "" FORCE)

if(TARGET uavcan)
    message(STATUS "uavcan target exists")
else()
    message(FATAL_ERROR "uavcan target does not exist")
endif()

#************* Import source files ****************

file(GLOB_RECURSE bool_test_files 
  ${CMAKE_CURRENT_LIST_DIR}/bool_tests/*.cpp
)

file(GLOB_RECURSE board_files
  ${CMAKE_SOURCE_DIR}/board/*.cpp
)

file(GLOB_RECURSE loopback_test_files 
  ${CMAKE_CURRENT_LIST_DIR}/loopback_test/*.cpp
)

file(GLOB_RECURSE efi_common_files 
  ${EFI_CAN_COMMON_SRC}
)

file(GLOB_RECURSE cli_files 
  ${CMAKE_CURRENT_LIST_DIR}/cli_app/*.cpp
)

file(GLOB_RECURSE cli_resp_files 
  ${CMAKE_CURRENT_LIST_DIR}/cli_responder/*.cpp
)

# ************ Bool Tests ***************
add_executable(${PROJECT_TEST_NAME} ${bool_test_files} ${efi_can_common} ${board_files})

target_compile_features(${PROJECT_TEST_NAME} PRIVATE cxx_std_20)
target_compile_features(${PROJECT_TEST_NAME} PRIVATE cxx_std_20)

if (COVERAGE_ENABLED)
  target_compile_options(${PROJECT_TEST_NAME} PRIVATE
    -fprofile-instr-generate -fcoverage-mapping
  )

  target_link_options(${PROJECT_TEST_NAME} PRIVATE
    -fprofile-instr-generate -fcoverage-mapping
  )
endif()

target_include_directories(${PROJECT_TEST_NAME} PRIVATE
  ${CMAKE_SOURCE_DIR}/board
  ${CMAKE_SOURCE_DIR}/tests
  ${LIBUAVCAN_SRC}/libuavcan/include
  ${LIBUAVCAN_SRC}/libuavcan/include/dsdlc_generated
  ${CMAKE_SOURCE_DIR}/messages/compiled
  ${EFI_CAN_COMMON_SRC}
)

target_link_libraries(${PROJECT_TEST_NAME} PRIVATE
  uavcan
  gtest
  gtest_main
)

target_compile_definitions(${PROJECT_TEST_NAME} PRIVATE
    UAVCAN_CPP_VERSION=UAVCAN_CPP11
)

# Print all linked libraries for a target
get_target_property(LINK_LIBS ${PROJECT_TEST_NAME} LINK_LIBRARIES)
message(STATUS "Linked libraries: ${LINK_LIBS}")

#enable_testing()


# ************ Loopback Tests ***************
add_executable(${PROJECT_LOOPBACK} ${loopback_test_files} ${efi_can_common})

target_include_directories(${PROJECT_LOOPBACK} PRIVATE
  ${CMAKE_SOURCE_DIR}/board
  ${CMAKE_SOURCE_DIR}/tests
  ${CMAKE_SOURCE_DIR}/tests/cli_app
  ${LIBUAVCAN_SRC}/libuavcan/include
  ${LIBUAVCAN_SRC}/libuavcan/include/dsdlc_generated
  ${CMAKE_SOURCE_DIR}/messages/compiled
  ${EFI_CAN_COMMON_SRC}
)

target_compile_definitions(${PROJECT_LOOPBACK} PRIVATE
    UAVCAN_CPP_VERSION=UAVCAN_CPP11
)

target_link_libraries(${PROJECT_LOOPBACK} PRIVATE
  uavcan
)

# ********** CLI APP *****************
add_executable(${PROJECT_CLI_APP} ${cli_files} ${board_files})

target_include_directories(${PROJECT_CLI_APP} PRIVATE
  ${CMAKE_SOURCE_DIR}/board
  ${CMAKE_SOURCE_DIR}/tests
  ${CMAKE_SOURCE_DIR}/tests/cli_app
  ${LIBUAVCAN_SRC}/libuavcan/include
  ${LIBUAVCAN_SRC}/libuavcan/include/dsdlc_generated
  ${CMAKE_SOURCE_DIR}/messages/compiled
  ${EFI_CAN_COMMON_SRC}
)

target_compile_definitions(${PROJECT_CLI_APP} PRIVATE
    UAVCAN_CPP_VERSION=UAVCAN_CPP11
)

target_link_libraries(${PROJECT_CLI_APP} PRIVATE
  uavcan
)

# ********** CLI Responder ************
set(PROJECT_CLI_RESP ${PROJECT_NAME}_CLI_RESP)

add_executable(${PROJECT_CLI_RESP} ${cli_resp_files} ${efi_can_common})

target_include_directories(${PROJECT_CLI_RESP} PRIVATE
  ${CMAKE_SOURCE_DIR}/board
  ${CMAKE_SOURCE_DIR}/tests
  ${CMAKE_SOURCE_DIR}/tests/cli_app
  ${LIBUAVCAN_SRC}/libuavcan/include
  ${LIBUAVCAN_SRC}/libuavcan/include/dsdlc_generated
  ${CMAKE_SOURCE_DIR}/messages/compiled
  ${EFI_CAN_COMMON_SRC}
)


target_compile_definitions(${PROJECT_CLI_RESP} PRIVATE
    UAVCAN_CPP_VERSION=UAVCAN_CPP11
)

target_link_libraries(${PROJECT_CLI_RESP} PRIVATE
  uavcan
)

