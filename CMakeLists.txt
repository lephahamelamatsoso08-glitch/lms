cmake_minimum_required(VERSION 3.20)
project(lms VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(LMS_BUILD_TESTS "Build the tests" ON)
option(LMS_COVERAGE "Build with coverage flags" OFF)

add_subdirectory(src/domain)

if(LMS_BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()
