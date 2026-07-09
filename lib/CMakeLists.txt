file(GLOB_RECURSE source ${CMAKE_CURRENT_LIST_DIR}/*.cpp)

add_library(${PROJECT_NAME} STATIC ${source})

target_include_directories(${PROJECT_NAME} PUBLIC
  ${CMAKE_CURRENT_LIST_DIR}
  ${CMAKE_BINARY_DIR}/libuavcan/libuavcan/include
  ${CMAKE_BINARY_DIR}/libuavcan/libuavcan/include/dsdlc_generated
  ${CMAKE_SOURCE_DIR}/messages/compiled
)
