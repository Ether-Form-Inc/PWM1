set(PROJECT_TEST_NAME ${PROJECT_NAME}_tests)

set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)

FetchContent_Declare(
  gtest
  GIT_REPOSITORY https://github.com/google/googletest.git
  GIT_TAG "main"
  SOURCE_DIR ${CMAKE_BINARY_DIR}/gtest
  BINARY_DIR ${CMAKE_BINARY_DIR}/gtest_build
)
FetchContent_MakeAvailable(gtest)

file(GLOB_RECURSE test_files 
  ${CMAKE_CURRENT_LIST_DIR}/*.cpp
  ${CMAKE_SOURCE_DIR}/messages/compiled/src
)

add_executable(${PROJECT_TEST_NAME} ${test_files})

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
  ${CMAKE_SOURCE_DIR}/lib
  ${CMAKE_SOURCE_DIR}/tests
  ${CMAKE_BINARY_DIR}/libuavcan/libuavcan/include
  ${CMAKE_SOURCE_DIR}/messages/compiled
)

target_link_libraries(${PROJECT_TEST_NAME} PRIVATE
  ${PROJECT_NAME}
  ${CMAKE_BINARY_DIR}/libuavcan/build/libuavcan/libuavcan.a
  gtest
  gtest_main
)

# Print all linked libraries for a target
get_target_property(LINK_LIBS ${PROJECT_TEST_NAME} LINK_LIBRARIES)
message(STATUS "Linked libraries: ${LINK_LIBS}")

#enable_testing()
