add_library(lms_domain STATIC TenPercentPerDayPenalty.cpp)
target_include_directories(lms_domain PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_options(lms_domain PRIVATE -Wall -Wextra -Wpedantic)

if(LMS_COVERAGE)
  target_compile_options(lms_domain PUBLIC --coverage -O0)
  target_link_options(lms_domain PUBLIC --coverage)
endif()
