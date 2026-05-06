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
  
if(EXISTS ${PYTHON_VENV})
  message(STATUS "Creating virtualenv: ${VENV_DIR}")

  execute_process(
      COMMAND ${Python_EXECUTABLE} -m venv ${VENV_DIR}
      WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
      RESULT_VARIABLE pip_status
  )

  if(NOT pip_status EQUAL 0)
    message(FATAL_ERROR "Failed to setup virtual env for dsdlc")
  else()
    message(STATUS "Entering virtualenv")
    execute_process(
      COMMAND source venv/bin/activate
      WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
      RESULT_VARIABLE venv_status
    )

    if(NOT venv_status EQUAL 0)
      message(FATAL_ERROR "Failed to enter venv")
    endif()
  endif()

  execute_process(
    COMMAND ${PYTHON_VENV} -m pip install -r ${CMAKE_BINARY_DIR}/dsdlc/requirements.txt
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    RESULT_VARIABLE pip_status
  )

  if(NOT pip_status EQUAL 0)
    message(FATAL_ERROR "Failed to install python requirements for dsdlc")
  endif()
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
