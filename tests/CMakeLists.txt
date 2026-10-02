find_package(GTest QUIET)

if(NOT GTest_FOUND)
  include(FetchContent)
  FetchContent_Declare(googletest
    URL https://github.com/google/googletest/archive/refs/tags/v1.17.0.tar.gz
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
  set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
  FetchContent_MakeAvailable(googletest)
endif()

add_executable(domain_tests PenaltyTests.cpp UserTests.cpp SubmissionTests.cpp)
target_link_libraries(domain_tests PRIVATE lms_domain GTest::gtest_main)

include(GoogleTest)
gtest_discover_tests(domain_tests)
