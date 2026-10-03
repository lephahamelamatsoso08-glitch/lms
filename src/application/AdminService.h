#pragma once

#include <string>

#include "AccessDeniedError.h"
#include "Action.h"
#include "ICourseRepository.h"
#include "IPasswordHasher.h"
#include "IUserRepository.h"
#include "Role.h"
#include "User.h"
#include "UserFactory.h"

namespace lms::application {

// Everything the administrator does (FR3, FR4, FR5, FR20). Every method first
// checks that the caller is an active user who is allowed to do it (FR2).
class AdminService {
public:
    AdminService(lms::domain::IUserRepository& users, lms::domain::ICourseRepository& courses,
                 const lms::domain::IPasswordHasher& hasher);

    // Password rule: at least 8 characters, with at least one letter and one digit.
    // Throws std::invalid_argument if it is not met.
    static void checkPassword(const std::string& password);

    // FR3. Returns the new user id.
    //  - std::invalid_argument: bad user data or a weak password.
    //  - DatabaseError (from the repository): duplicate username, student number or
    //    department already has a head. Nothing is saved in that case.
    int createUser(const lms::domain::User& admin, lms::domain::Role role,
                   const lms::domain::UserData& data, const std::string& password);

    // FR3. An administrator cannot deactivate their own account.
    void deactivateUser(const lms::domain::User& admin, int userId);
    void reactivateUser(const lms::domain::User& admin, int userId);

    // FR20. Throws std::invalid_argument for an unknown user or a weak password.
    void resetPassword(const lms::domain::User& admin, int userId, const std::string& newPassword);

    // FR4. Return the new id.
    int createDepartment(const lms::domain::User& admin, const std::string& name);
    int createCourse(const lms::domain::User& admin, const std::string& code,
                     const std::string& title, int departmentId);

    // FR5. Doing the same thing twice is harmless.
    void assignLecturer(const lms::domain::User& admin, int courseId, int lecturerId);
    void enrolStudent(const lms::domain::User& admin, int courseId, int studentId);

private:
    static void requireAllowed(const lms::domain::User& admin, lms::domain::Action action);
    void setActive(const lms::domain::User& admin, int userId, bool active);

    lms::domain::IUserRepository& users_;
    lms::domain::ICourseRepository& courses_;
    const lms::domain::IPasswordHasher& hasher_;
};

}  // namespace lms::application
