#pragma once

#include <memory>
#include <string>

#include "AdminService.h"
#include "Argon2Hasher.h"
#include "AssignmentService.h"
#include "AuditService.h"
#include "AuthService.h"
#include "DatabaseConnection.h"
#include "HodViewService.h"
#include "LecturerViewService.h"
#include "ModerationService.h"
#include "NotificationService.h"
#include "SqliteAssignmentRepository.h"
#include "SqliteAuditRepository.h"
#include "SqliteCourseRepository.h"
#include "SqliteNotificationRepository.h"
#include "SqliteSubmissionRepository.h"
#include "SqliteTransactionRunner.h"
#include "SqliteUserRepository.h"
#include "StudentViewService.h"
#include "SubmissionService.h"
#include "TenPercentPerDayPenalty.h"

namespace lms::app {

// The composition root: it builds every part of the system and connects them
// (dependency injection). The windows only talk to the services it returns.
// It has no Qt code, so it can be tested without a window.
class AppContext {
public:
    // Opens the database file (use ":memory:" for a temporary one) and creates the
    // tables if the database is new. Throws lms::data::DatabaseError on failure.
    explicit AppContext(const std::string& databasePath);

    AppContext(const AppContext&) = delete;
    AppContext& operator=(const AppContext&) = delete;

    // First run: the system has no users, so nobody could log in.
    bool needsFirstAdmin() const;

    // Creates the first administrator. Returns the new user id.
    //  - std::logic_error: the system already has users.
    //  - std::invalid_argument: bad user data or a weak password.
    int createFirstAdmin(const std::string& username, const std::string& fullName,
                         const std::string& password);

    lms::application::AuthService& auth() { return authService_; }
    lms::application::AdminService& admin() { return adminService_; }
    lms::application::AssignmentService& assignments() { return assignmentService_; }
    lms::application::SubmissionService& submissions() { return submissionService_; }
    lms::application::ModerationService& moderation() { return moderationService_; }
    lms::application::NotificationService& notifications() { return notificationService_; }
    lms::application::StudentViewService& studentView() { return studentViewService_; }
    lms::application::LecturerViewService& lecturerView() { return lecturerViewService_; }
    lms::application::HodViewService& hodView() { return hodViewService_; }
    lms::application::AuditService& audit() { return auditService_; }

private:
    static std::unique_ptr<lms::data::DatabaseConnection> openDatabase(const std::string& path);

    // The order of these members is the order of construction. Keep it.
    std::unique_ptr<lms::data::DatabaseConnection> db_;
    lms::data::SqliteUserRepository users_;
    lms::data::SqliteCourseRepository courses_;
    lms::data::SqliteAssignmentRepository assignmentRepo_;
    lms::data::SqliteSubmissionRepository submissionRepo_;
    lms::data::SqliteAuditRepository auditRepo_;
    lms::data::SqliteNotificationRepository notificationRepo_;
    lms::data::SqliteTransactionRunner transactions_;
    lms::data::Argon2Hasher hasher_;
    lms::domain::TenPercentPerDayPenalty penalty_;

    lms::application::AuditService auditService_;
    lms::application::AuthService authService_;
    lms::application::AdminService adminService_;
    lms::application::AssignmentService assignmentService_;
    lms::application::SubmissionService submissionService_;
    lms::application::ModerationService moderationService_;
    lms::application::NotificationService notificationService_;
    lms::application::StudentViewService studentViewService_;
    lms::application::LecturerViewService lecturerViewService_;
    lms::application::HodViewService hodViewService_;
};

}  // namespace lms::app
