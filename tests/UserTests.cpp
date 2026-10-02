#include <gtest/gtest.h>

#include <memory>
#include <set>
#include <stdexcept>
#include <string>

#include "Action.h"
#include "Role.h"
#include "UserFactory.h"
#include "UserRoles.h"

using namespace lms::domain;

namespace {

const Action kAllActions[] = {
    Action::ManageUsers,      Action::ManageCourses,   Action::AssignAndEnrol,
    Action::ResetPassword,    Action::CreateAssignment, Action::GrantExtension,
    Action::MarkSubmission,   Action::ModerateMarks,   Action::ReleaseMarks,
    Action::ViewAuditLog,     Action::ViewOwnCourses,  Action::SubmitWork,
    Action::ViewReleasedMarks};

const Role kAllRoles[] = {Role::Admin, Role::Lecturer, Role::HoD, Role::Student};

UserData makeData() {
    UserData d;
    d.id = 1;
    d.username = "user1";
    d.fullName = "Test User";
    d.active = true;
    d.departmentId = 1;
    d.studentNumber = "S001";
    return d;
}

// The permission table from the SRS (FR3 to FR20).
std::set<Action> allowedFor(Role role) {
    switch (role) {
        case Role::Admin:
            return {Action::ManageUsers, Action::ManageCourses, Action::AssignAndEnrol,
                    Action::ResetPassword, Action::ViewAuditLog};
        case Role::Lecturer:
            return {Action::CreateAssignment, Action::GrantExtension, Action::MarkSubmission};
        case Role::HoD:
            return {Action::ModerateMarks, Action::ReleaseMarks, Action::ViewAuditLog};
        case Role::Student:
            return {Action::ViewOwnCourses, Action::SubmitWork, Action::ViewReleasedMarks};
    }
    return {};
}

}  // namespace

// ---- Role text conversion ----
TEST(RoleTest, TextRoundTrip) {
    for (Role role : kAllRoles) {
        EXPECT_EQ(roleFromString(toString(role)), role);
    }
}

TEST(RoleTest, UnknownTextThrows) {
    EXPECT_THROW(roleFromString("PRINCIPAL"), std::invalid_argument);
    EXPECT_THROW(roleFromString(""), std::invalid_argument);
}

// ---- Factory ----
TEST(UserFactoryTest, CreatesTheRightSubclass) {
    UserData d = makeData();
    for (Role role : kAllRoles) {
        std::unique_ptr<User> user = UserFactory::create(role, d);
        ASSERT_NE(user, nullptr);
        EXPECT_EQ(user->getRole(), role);
    }
    EXPECT_NE(dynamic_cast<Admin*>(UserFactory::create(Role::Admin, d).get()), nullptr);
    EXPECT_NE(dynamic_cast<Lecturer*>(UserFactory::create(Role::Lecturer, d).get()), nullptr);
    EXPECT_NE(dynamic_cast<HoD*>(UserFactory::create(Role::HoD, d).get()), nullptr);
    EXPECT_NE(dynamic_cast<Student*>(UserFactory::create(Role::Student, d).get()), nullptr);
}

TEST(UserFactoryTest, KeepsCommonAndRoleSpecificData) {
    UserData d = makeData();
    auto hod = UserFactory::create(Role::HoD, d);
    auto student = UserFactory::create(Role::Student, d);
    EXPECT_EQ(hod->username(), "user1");
    EXPECT_EQ(hod->fullName(), "Test User");
    EXPECT_EQ(dynamic_cast<HoD*>(hod.get())->departmentId(), 1);
    EXPECT_EQ(dynamic_cast<Student*>(student.get())->studentNumber(), "S001");
}

TEST(UserFactoryTest, HoDWithoutDepartmentThrows) {
    UserData d = makeData();
    d.departmentId = 0;
    EXPECT_THROW(UserFactory::create(Role::HoD, d), std::invalid_argument);
}

TEST(UserFactoryTest, StudentWithoutNumberThrows) {
    UserData d = makeData();
    d.studentNumber = "";
    EXPECT_THROW(UserFactory::create(Role::Student, d), std::invalid_argument);
}

TEST(UserFactoryTest, EmptyUsernameThrows) {
    UserData d = makeData();
    d.username = "";
    EXPECT_THROW(UserFactory::create(Role::Admin, d), std::invalid_argument);
}

TEST(UserFactoryTest, EmptyFullNameThrows) {
    UserData d = makeData();
    d.fullName = "";
    EXPECT_THROW(UserFactory::create(Role::Lecturer, d), std::invalid_argument);
}

TEST(UserFactoryTest, NegativeIdThrows) {
    UserData d = makeData();
    d.id = -1;
    EXPECT_THROW(UserFactory::create(Role::Admin, d), std::invalid_argument);
}

// ---- Role-based access (FR2) ----
TEST(UserPermissionTest, EachRoleMayDoExactlyItsActions) {
    UserData d = makeData();
    for (Role role : kAllRoles) {
        auto user = UserFactory::create(role, d);
        const std::set<Action> allowed = allowedFor(role);
        for (Action action : kAllActions) {
            SCOPED_TRACE(toString(role) + " action " + std::to_string(static_cast<int>(action)));
            EXPECT_EQ(user->canPerform(action), allowed.count(action) == 1);
        }
    }
}

TEST(UserPermissionTest, InactiveUserCanDoNothing) {
    UserData d = makeData();
    d.active = false;
    for (Role role : kAllRoles) {
        auto user = UserFactory::create(role, d);
        EXPECT_FALSE(user->isActive());
        for (Action action : kAllActions) {
            EXPECT_FALSE(user->canPerform(action));
        }
    }
}

TEST(UserPermissionTest, StudentCannotModerateOrMark) {
    auto student = UserFactory::create(Role::Student, makeData());
    EXPECT_FALSE(student->canPerform(Action::ModerateMarks));
    EXPECT_FALSE(student->canPerform(Action::MarkSubmission));
    EXPECT_FALSE(student->canPerform(Action::ReleaseMarks));
}

TEST(UserPermissionTest, LecturerCannotReleaseMarks) {
    auto lecturer = UserFactory::create(Role::Lecturer, makeData());
    EXPECT_FALSE(lecturer->canPerform(Action::ReleaseMarks));
}
