#std::execution with ULT backend (X)

cmake_minimum_required(VERSION 3.25)
set(CMAKE_DISABLE_IN_SOURCE_BUILD ON)
set(CMAKE_DISABLE_SOURCE_CHANGES ON)

set(CMAKE_C_STANDARD 23)
set(CMAKE_CXX_STANDARD 26)
set(BUILD_SHARED_LIBS ON CACHE BOOL "Build using shared libraries.")

project(stdexx
  VERSION 0.1
  LANGUAGES C CXX)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/modules")

include("${CMAKE_CURRENT_SOURCE_DIR}/modules/flags.cmake")

option(ENABLE_BUILD_APPS "Build cgsolve example" OFF)
option(ENABLE_BUILD_EXAMPLES "Build fibonacci example" ON)

set(EXEC_BACKEND qthreads CACHE STRING "Backend to use, options are 'reference', 'qthreads', and 'argobots'.")

if ("${EXEC_BACKEND}" STREQUAL "qthreads")
  find_package(qthread REQUIRED)
  set(ULT_NAME "Qthreads")
  set(ULT_LIB "qthread")
  set(ULT_BACKEND_DEFINE "STDEXX_QTHREADS")
  list(APPEND ULTs ${ULT_NAME})
elseif ("${EXEC_BACKEND}" STREQUAL "argobots")
  message(FATAL_ERROR "Argobots support not yet implemented.")
  find_package(Argobots REQUIRED)
  set(ULT_NAME "Argobots")
  set(ULT_LIB "argobots")
  set(ULT_BACKEND_DEFINE "STDEXX_ARGOBOTS")
  list(APPEND ULTs ${ULT_NAME})
elseif ("${EXEC_BACKEND}" STREQUAL "reference")
  set(ULT_NAME "Reference")
  set(ULT_BACKEND_DEFINE "STDEXX_REFERENCE")
  list(APPEND ULTs ${ULT_NAME})
else ()
  message(FATAL_ERROR "EXEC_BACKEND not specified or not recognized.")
endif()

find_package(stdexec REQUIRED)

find_package(Catch2 3 REQUIRED)

add_subdirectory(impl)

if(ENABLE_BUILD_APPS)
    message(STATUS "Including apps")
    add_subdirectory(apps)
endif()

if(ENABLE_BUILD_EXAMPLES)
    message(STATUS "Including examples")
    add_subdirectory(examples)
endif()

