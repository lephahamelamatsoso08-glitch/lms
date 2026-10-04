find_package(GTest QUIET)

if(NOT GTest_FOUND)
  include(FetchContent)
  FetchContent_Declare(googletest
    URL https://github.com/google/googletest/archive/refs/tags/v1.17.0.tar.gz
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
  set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
  FetchContent_MakeAvailable(googletest)
endif()

add_executable(domain_tests PenaltyTests.cpp UserTests.cpp SubmissionTests.cpp LateDaysTests.cpp AssignmentTests.cpp SubmissionMarkTests.cpp TimeFormatTests.cpp)
target_link_libraries(domain_tests PRIVATE lms_domain GTest::gtest_main)

include(GoogleTest)
gtest_discover_tests(domain_tests)

add_executable(data_tests Argon2Tests.cpp DatabaseTests.cpp SqliteUserRepositoryTests.cpp AssignmentRepositoryTests.cpp SubmissionRepositoryTests.cpp CourseRepositoryTests.cpp)
target_link_libraries(data_tests PRIVATE lms_data GTest::gtest_main)
gtest_discover_tests(data_tests)

target_compile_definitions(data_tests PRIVATE LMS_SCHEMA_PATH="${CMAKE_SOURCE_DIR}/db/schema.sql")

add_executable(app_tests AuthServiceTests.cpp AuditTests.cpp SubmissionServiceTests.cpp SubmissionMarkingTests.cpp ModerationTests.cpp NotificationTests.cpp StudentViewTests.cpp AdminTests.cpp AssignmentServiceTests.cpp)
target_link_libraries(app_tests PRIVATE lms_application lms_data GTest::gtest_main)
target_compile_definitions(app_tests PRIVATE LMS_SCHEMA_PATH="${CMAKE_SOURCE_DIR}/db/schema.sql")
gtest_discover_tests(app_tests)

add_executable(context_tests AppContextTests.cpp)
target_link_libraries(context_tests PRIVATE lms_app GTest::gtest_main)
gtest_discover_tests(context_tests)
