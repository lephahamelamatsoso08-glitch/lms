#include "UserRoles.h"

#include <algorithm>
#include <initializer_list>
#include <stdexcept>
#include <utility>

namespace lms::domain {

namespace {

bool contains(std::initializer_list<Action> list, Action action) {
    return std::find(list.begin(), list.end(), action) != list.end();
}

}  // namespace

// ---- Admin ----
Role Admin::getRole() const { return Role::Admin; }

bool Admin::canPerform(Action action) const {
    return isActive() &&
           contains({Action::ManageUsers, Action::ManageCourses, Action::AssignAndEnrol,
                     Action::ResetPassword, Action::ViewAuditLog},
                    action);
}

// ---- Lecturer ----
Role Lecturer::getRole() const { return Role::Lecturer; }

bool Lecturer::canPerform(Action action) const {
    return isActive() &&
           contains({Action::CreateAssignment, Action::GrantExtension, Action::MarkSubmission},
                    action);
}

// ---- Head of Department ----
HoD::HoD(int id, std::string username, std::string fullName, bool active, int departmentId)
    : User(id, std::move(username), std::move(fullName), active), departmentId_(departmentId) {
    if (departmentId_ <= 0) {
        throw std::invalid_argument("a head of department needs a department");
    }
}

Role HoD::getRole() const { return Role::HoD; }

bool HoD::canPerform(Action action) const {
    return isActive() &&
           contains({Action::ModerateMarks, Action::ReleaseMarks, Action::ViewAuditLog}, action);
}

// ---- Student ----
Student::Student(int id, std::string username, std::string fullName, bool active,
                 std::string studentNumber)
    : User(id, std::move(username), std::move(fullName), active),
      studentNumber_(std::move(studentNumber)) {
    if (studentNumber_.empty()) {
        throw std::invalid_argument("a student needs a student number");
    }
}

Role Student::getRole() const { return Role::Student; }

bool Student::canPerform(Action action) const {
    return isActive() &&
           contains({Action::ViewOwnCourses, Action::SubmitWork, Action::ViewReleasedMarks},
                    action);
}

}  // namespace lms::domain
