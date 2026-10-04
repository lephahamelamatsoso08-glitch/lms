cmake_minimum_required(VERSION 3.20)
project(lms VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(LMS_BUILD_TESTS "Build the tests" ON)
option(LMS_COVERAGE "Build with coverage flags" OFF)

find_package(PkgConfig REQUIRED)
pkg_check_modules(SODIUM REQUIRED IMPORTED_TARGET libsodium)

find_package(SQLite3 REQUIRED)
if(NOT TARGET SQLite3::SQLite3)
  add_library(SQLite3::SQLite3 ALIAS SQLite::SQLite3)
endif()

add_subdirectory(src/domain)
add_subdirectory(src/data)
add_subdirectory(src/application)
add_subdirectory(src/app)

option(LMS_BUILD_UI "Build the Qt interface" ON)
if(LMS_BUILD_UI)
  add_subdirectory(src/ui)
endif()

if(LMS_BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()
