#include "AdminViewService.h"

#include <unordered_map>

#include "Action.h"
#include "Course.h"
#include "MarkAuditEntry.h"
#include "UserRoles.h"

namespace lms::application {

using lms::domain::Action;
using lms::domain::Course;
using lms::domain::DepartmentInfo;
using lms::domain::HoD;
using lms::domain::Student;
using lms::domain::User;

AdminViewService::AdminViewService(const lms::domain::IUserRepository& users,
                                   const lms::domain::ICourseRepository& courses,
                                   const AuditService& audit)
    : users_(users), courses_(courses), audit_(audit) {}

void AdminViewService::requireAdmin(const User& admin) {
    if (!admin.canPerform(Action::ManageUsers)) {
        throw AccessDeniedError("Only an active administrator can use this page.");
    }
}

std::vector<AdminUserRow> AdminViewService::users(const User& admin) const {
    requireAdmin(admin);
    std::unordered_map<int, std::string> departmentNames;
    for (const DepartmentInfo& department : courses_.findDepartments()) {
        departmentNames[department.id] = department.name;
    }

    std::vector<AdminUserRow> result;
    for (const std::unique_ptr<User>& user : users_.findAll()) {
        AdminUserRow row;
        row.id = user->id();
        row.username = user->username();
        row.fullName = user->fullName();
        row.role = user->getRole();
        row.active = user->isActive();
        if (const auto* student = dynamic_cast<const Student*>(user.get())) {
            row.detail = student->studentNumber();
        } else if (const auto* head = dynamic_cast<const HoD*>(user.get())) {
            const auto found = departmentNames.find(head->departmentId());
            if (found != departmentNames.end()) {
                row.detail = found->second;
            }
        }
        result.push_back(row);
    }
    return result;
}

std::vector<DepartmentInfo> AdminViewService::departments(const User& admin) const {
    requireAdmin(admin);
    return courses_.findDepartments();
}

std::vector<AdminCourseRow> AdminViewService::courses(const User& admin) const {
    requireAdmin(admin);
    std::unordered_map<int, std::string> departmentNames;
    for (const DepartmentInfo& department : courses_.findDepartments()) {
        departmentNames[department.id] = department.name;
    }

    std::vector<AdminCourseRow> result;
    for (const Course& course : courses_.findAll()) {
        AdminCourseRow row;
        row.id = course.id();
        row.code = course.code();
        row.title = course.title();
        const auto found = departmentNames.find(course.departmentId());
        row.department = found != departmentNames.end() ? found->second : "";
        row.lecturers = courses_.findLecturers(course.id());
        row.studentCount = static_cast<int>(courses_.findStudents(course.id()).size());
        result.push_back(row);
    }
    return result;
}

std::vector<AdminAuditRow> AdminViewService::auditLog(const User& admin) const {
    requireAdmin(admin);
    std::unordered_map<int, std::string> names;
    for (const std::unique_ptr<User>& user : users_.findAll()) {
        names[user->id()] = user->fullName();
    }

    std::vector<AdminAuditRow> result;
    for (const auto& entry : audit_.all(admin)) {
        AdminAuditRow row;
        row.changedAt = entry.changedAt();
        row.submissionId = entry.submissionId();
        const auto found = names.find(entry.changedBy());
        row.changedBy = found != names.end() ? found->second : "Unknown user";
        row.oldValue = entry.oldValue();
        row.newValue = entry.newValue();
        row.reason = entry.reason();
        result.push_back(row);
    }
    return result;
}

}  // namespace lms::application
