set(PROJECT_LOOPBACK ${PROJECT_NAME}_loopback)

add_executable(${PROJECT_LOOPBACK} ${CMAKE_CURRENT_LIST_DIR}/loopback.cpp)

target_link_libraries(${PROJECT_LOOPBACK} PRIVATE
  ${PROJECT_NAME}
)
