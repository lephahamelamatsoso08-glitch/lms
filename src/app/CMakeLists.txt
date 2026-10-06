file(READ ${CMAKE_SOURCE_DIR}/db/schema.sql LMS_SCHEMA_SQL)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${CMAKE_SOURCE_DIR}/db/schema.sql)
configure_file(Schema.h.in ${CMAKE_CURRENT_BINARY_DIR}/Schema.h @ONLY)

add_library(lms_app STATIC AppContext.cpp DemoData.cpp)
target_include_directories(lms_app PUBLIC ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR})
target_link_libraries(lms_app PUBLIC lms_application lms_data)
target_compile_options(lms_app PRIVATE -Wall -Wextra -Wpedantic)
