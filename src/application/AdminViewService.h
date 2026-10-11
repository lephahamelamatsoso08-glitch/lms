#pragma once

#include <optional>
#include <string>
#include <vector>

#include "AccessDeniedError.h"
#include "AuditService.h"
#include "DepartmentInfo.h"
#include "ICourseRepository.h"
#include "IUserRepository.h"
#include "LateDays.h"
#include "PersonInfo.h"
#include "Role.h"
#include "User.h"

namespace lms::application {

struct AdminUserRow {
    int id = 0;
    std::string username;
    std::string fullName;
    lms::domain::Role role = lms::domain::Role::Student;
    bool active = true;
    std::string detail;  // the student number, or the department of a head of department
};

struct AdminCourseRow {
    int id = 0;
    std::string code;
    std::string title;
    std::string department;
    std::vector<lms::domain::PersonInfo> lecturers;
    int studentCount = 0;
};

struct AdminAuditRow {
    lms::domain::TimePoint changedAt{};
    int submissionId = 0;
    std::string changedBy;
    std::optional<double> oldValue;
    double newValue = 0.0;
    std::string reason;
};

// The lists the administrator works with (FR3, FR4, FR5, FR19).
// Everything here throws AccessDeniedError unless the caller is an active administrator.
class AdminViewService {
public:
    AdminViewService(const lms::domain::IUserRepository& users,
                     const lms::domain::ICourseRepository& courses, const AuditService& audit);

    std::vector<AdminUserRow> users(const lms::domain::User& admin) const;  // by username
    std::vector<lms::domain::DepartmentInfo> departments(const lms::domain::User& admin) const;
    std::vector<AdminCourseRow> courses(const lms::domain::User& admin) const;  // by code
    std::vector<AdminAuditRow> auditLog(const lms::domain::User& admin) const;  // oldest first

private:
    static void requireAdmin(const lms::domain::User& admin);

    const lms::domain::IUserRepository& users_;
    const lms::domain::ICourseRepository& courses_;
    const AuditService& audit_;
};

}  // namespace lms::application
