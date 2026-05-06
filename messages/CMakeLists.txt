FetchContent_Declare(
  dsdlc
  GIT_REPOSITORY https://github.com/dronecan/dronecan_dsdlc.git
  GIT_TAG "master"
  SOURCE_DIR ${CMAKE_BINARY_DIR}/dsdlc
)
FetchContent_Populate(dsdlc)

find_package(Python REQUIRED)

set(VENV_DIR ${CMAKE_BINARY_DIR}/venv)
set(PYTHON_VENV ${VENV_DIR}/bin/python)
  
message(STATUS "Creating virtualenv: ${VENV_DIR}")

execute_process(
  COMMAND python3 -m venv venv
  WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
  RESULT_VARIABLE venv_status
  ERROR_VARIABLE venv_error
)

if(NOT venv_status EQUAL 0)
  message(FATAL_ERROR
    "Failed to create venv\n"
    "stderr:\n${venv_error}"
  )
endif()
  
message(STATUS "Installing pip requirement: ${VENV_DIR}")

execute_process(
  COMMAND ${CMAKE_BINARY_DIR}/venv/bin/pip install -r ${CMAKE_BINARY_DIR}/dsdlc/requirements.txt
  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
  RESULT_VARIABLE pip_install_result
  ERROR_VARIABLE pip_install_error
)

if(NOT pip_install_result EQUAL 0)
  message(FATAL_ERROR
    "Failed to install requirements\n"
    "stderr:\n${pip_install_error}"
  )
endif()

message(STATUS "Installing drone can: ${VENV_DIR}")

execute_process(
  COMMAND ${CMAKE_BINARY_DIR}/venv/bin/pip install dronecan
  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
  RESULT_VARIABLE dronecan_install_result
  ERROR_VARIABLE dronecan_install_error
)

if(NOT dronecan_install_result EQUAL 0)
  message(FATAL_ERROR
    "Failed to install dronecan\n"
    "stderr:\n${dronecan_install_error}"
  )
endif()

FetchContent_Declare(
  dsdl
  GIT_REPOSITORY https://github.com/dronecan/DSDL.git
  GIT_TAG "master"
  SOURCE_DIR ${CMAKE_BINARY_DIR}/dsdl
)
FetchContent_Populate(dsdl)

FetchContent_Declare(
  libcanard
  GIT_REPOSITORY https://github.com/dronecan/libcanard.git
  GIT_TAG "master"
  SOURCE_DIR ${CMAKE_BINARY_DIR}/libcanard
)
FetchContent_Populate(libcanard)

execute_process(
  COMMAND ../venv/bin/python3 dronecan_dsdlc.py -O ../../messages/compiled ../../messages/etherform
  WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/dsdlc
  RESULT_VARIABLE compile_messages_result
  ERROR_VARIABLE compile_messages_error
)

if(NOT compile_messages_result EQUAL 0)
  message(FATAL_ERROR
    "Failed to compile messages\n"
    "stderr:\n${compile_messages_error}"
  )
endif()
