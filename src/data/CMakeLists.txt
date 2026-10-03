add_library(lms_data STATIC
  Argon2Hasher.cpp
  DatabaseConnection.cpp
  Statement.cpp
  SqliteUserRepository.cpp
  SqliteAuditRepository.cpp
  SqliteAssignmentRepository.cpp
  SqliteSubmissionRepository.cpp
  SqliteCourseRepository.cpp)
target_include_directories(lms_data PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(lms_data PUBLIC lms_domain PkgConfig::SODIUM SQLite3::SQLite3)
target_compile_options(lms_data PRIVATE -Wall -Wextra -Wpedantic)
