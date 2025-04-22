#std::execution with ULT backend (X)

cmake_minimum_required(VERSION 3.17)
set(CMAKE_DISABLE_IN_SOURCE_BUILD ON)
set(CMAKE_DISABLE_SOURCE_CHANGES ON)

project(stdexx)

list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/modules)

include(${CMAKE_CURRENT_SOURCE_DIR}/modules/flags.cmake)

option(ENABLE_BUILD_APPS "Build cgsolve example" OFF)
option(ENABLE_BUILD_EXAMPLES "Build fibonacci example" OFF)

option(ENABLE_QTHREADS "Build with Qthreads" OFF)
option(ENABLE_ARGOBOTS "Build with Argobots" OFF)
option(ENABLE_REF "Build with stdexec reference implementation" OFF)

if(ENABLE_QTHREADS)
  find_package(qthread REQUIRED)
  set(ULT_NAME "qthread")
  set(ULT_BACKEND_DEFINE "STDEXX_ENABLE_QTHREADS")
  list(APPEND ULTs ${ULT_NAME})
endif()

if(ENABLE_ARGOBOTS)
   find_package(Argobots REQUIRED)
   set(ULT_NAME "Argobots")
   set(ULT_BACKEND_DEFINE "STDEXX_ENABLE_ARGOBOTS")
   list(APPEND ULTs ${ULT_NAME})
endif()

if(ENABLE_REF)
   set(ULT_NAME "REF")
   set(ULT_BACKEND_DEFINE "STDEXX_ENABLE_REF")
   list(APPEND ULTs ${ULT_NAME})
endif()

list(LENGTH ULTs nULTs)
if (NOT ${nULTs} EQUAL "1")
  message(FATAL_ERROR "Must give a single valid ULT backend. ${nULTs} given.")
endif()

add_compile_options(-std=c++20)

find_package(stdexec REQUIRED)

add_subdirectory(impl)

if(ENABLE_BUILD_APPS)
    message(STATUS "Including apps")
    add_subdirectory(apps)
endif()

if(ENABLE_BUILD_EXAMPLES)
    message(STATUS "Including examples")
    add_subdirectory(examples)
endif()




