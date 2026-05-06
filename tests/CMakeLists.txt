set(PROJECT_TEST_NAME ${PROJECT_NAME}_tests)

file(GLOB_RECURSE test_files 
  ${CMAKE_CURRENT_LIST_DIR}/*.cpp
  ${CMAKE_CURRENT_LIST_DIR}/*.h
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

target_include_directories(${PROJECT_TEST_NAME} PUBLIC
  ${CMAKE_CURRENT_LIST_DIR}
  ${CMAKE_BINARY_DIR}/libcanard
  ${CMAKE_SOURCE_DIR}/messages/compiled/include
)

target_link_libraries(${PROJECT_TEST_NAME} PRIVATE
  gtest
  gtest_main
)

enable_testing()
