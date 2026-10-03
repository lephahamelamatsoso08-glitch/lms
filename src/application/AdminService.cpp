#include "AdminService.h"

#include <cctype>
#include <memory>
#include <stdexcept>

#include "Course.h"

namespace lms::application {

using lms::domain::Action;
using lms::domain::Course;
using lms::domain::Role;
using lms::domain::User;
using lms::domain::UserData;
using lms::domain::UserFactory;

AdminService::AdminService(lms::domain::IUserRepository& users,
                           lms::domain::ICourseRepository& courses,
                           const lms::domain::IPasswordHasher& hasher)
    : users_(users), courses_(courses), hasher_(hasher) {}

void AdminService::requireAllowed(const User& admin, Action action) {
    if (!admin.canPerform(action)) {
        throw AccessDeniedError("Only an active administrator can do this.");
    }
}

void AdminService::checkPassword(const std::string& password) {
    bool hasLetter = false;
    bool hasDigit = false;
    for (unsigned char c : password) {
        hasLetter = hasLetter || std::isalpha(c) != 0;
        hasDigit = hasDigit || std::isdigit(c) != 0;
    }
    if (password.size() < 8 || !hasLetter || !hasDigit) {
        throw std::invalid_argument(
            "The password must have at least 8 characters, with a letter and a digit.");
    }
}

int AdminService::createUser(const User& admin, Role role, const UserData& data,
                             const std::string& password) {
    requireAllowed(admin, Action::ManageUsers);
    std::unique_ptr<User> user = UserFactory::create(role, data);  // checks the user data
    checkPassword(password);
    return users_.add(*user, hasher_.hash(password));
}

void AdminService::setActive(const User& admin, int userId, bool active) {
    requireAllowed(admin, Action::ManageUsers);
    if (!active && userId == admin.id()) {
        throw std::invalid_argument("You cannot deactivate your own account.");
    }
    users_.setActive(userId, active);
}

void AdminService::deactivateUser(const User& admin, int userId) { setActive(admin, userId, false); }

void AdminService::reactivateUser(const User& admin, int userId) { setActive(admin, userId, true); }

void AdminService::resetPassword(const User& admin, int userId, const std::string& newPassword) {
    requireAllowed(admin, Action::ResetPassword);
    checkPassword(newPassword);
    if (!users_.findById(userId)) {
        throw std::invalid_argument("User not found.");
    }
    users_.setPasswordHash(userId, hasher_.hash(newPassword));
}

int AdminService::createDepartment(const User& admin, const std::string& name) {
    requireAllowed(admin, Action::ManageCourses);
    return courses_.addDepartment(name);
}

int AdminService::createCourse(const User& admin, const std::string& code,
                               const std::string& title, int departmentId) {
    requireAllowed(admin, Action::ManageCourses);
    return courses_.add(Course(0, code, title, departmentId));  // Course checks its own data
}

void AdminService::assignLecturer(const User& admin, int courseId, int lecturerId) {
    requireAllowed(admin, Action::AssignAndEnrol);
    courses_.assignLecturer(courseId, lecturerId);
}

void AdminService::enrolStudent(const User& admin, int courseId, int studentId) {
    requireAllowed(admin, Action::AssignAndEnrol);
    courses_.enrolStudent(courseId, studentId);
}

}  // namespace lms::application
