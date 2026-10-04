find_package(Qt6 REQUIRED COMPONENTS Widgets)
set(CMAKE_AUTOMOC ON)

add_library(lms_ui STATIC
  LoginWindow.cpp LoginWindow.h
  FirstRunDialog.cpp FirstRunDialog.h
  RoleWindow.cpp RoleWindow.h)
target_include_directories(lms_ui PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(lms_ui PUBLIC lms_app Qt6::Widgets)
target_compile_options(lms_ui PRIVATE -Wall -Wextra -Wpedantic)

add_executable(lms main.cpp)
target_link_libraries(lms PRIVATE lms_ui)
