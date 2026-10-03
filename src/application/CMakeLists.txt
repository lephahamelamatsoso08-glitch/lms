add_library(lms_application STATIC
  AuthService.cpp
  AuditService.cpp
  SubmissionService.cpp
  ModerationService.cpp
  NotificationService.cpp
  StudentViewService.cpp
  AdminService.cpp)
target_include_directories(lms_application PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(lms_application PUBLIC lms_domain)
target_compile_options(lms_application PRIVATE -Wall -Wextra -Wpedantic)
