#include <gtest/gtest.h>

#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include "AdminService.h"
#include "AuthService.h"
#include "DatabaseConnection.h"
#include "DatabaseError.h"
#include "IPasswordHasher.h"
#include "SqliteCourseRepository.h"
#include "SqliteUserRepository.h"
#include "UserFactory.h"

using namespace lms::domain;
using namespace lms::application;
using lms::data::DatabaseConnection;
using lms::data::DatabaseError;
using lms::data::SqliteCourseRepository;
using lms::data::SqliteUserRepository;
using lms::data::Statement;

namespace {

std::string readFile(const std::string& path) {
    std::ifstream in(path);
    if (!in) {
        throw std::runtime_error("cannot read " + path);
    }
    std::ostringstream text;
    text << in.rdbuf();
    return text.str();
}

// A fast hasher for tests (the real Argon2 hasher is tested elsewhere).
class FakeHasher : public IPasswordHasher {
public:
    std::string hash(const std::string& password) const override { return "fake:" + password; }
    bool verify(const std::string& password, const std::string& stored) const override {
        return stored == "fake:" + password;
    }
};

std::unique_ptr<User> makeUser(Role role, int id, bool active = true) {
    UserData d;
    d.id = id;
    d.username = "caller" + std::to_string(id);
    d.fullName = "Caller " + std::to_string(id);
    d.active = active;
    d.departmentId = 1;
    d.studentNumber = "C" + std::to_string(id);
    return UserFactory::create(role, d);
}

UserData data(const std::string& username) {
    UserData d;
    d.username = username;
    d.fullName = "Full " + username;
    d.departmentId = 1;
    d.studentNumber = "N-" + username;
    return d;
}

class AdminTest : public ::testing::Test {
protected:
    AdminTest()
        : db(":memory:"), users(db), courses(db), service(users, courses, hasher), auth(users, hasher) {
        db.runScript(readFile(LMS_SCHEMA_PATH));
        db.runScript(R"sql(
INSERT INTO user_account(id, username, full_name, password_hash, role) VALUES
  (8, 'adm1', 'Admin', 'fake:Admin1234', 'ADMIN');
INSERT INTO department(id, name) VALUES (1, 'Computer Science');
)sql");
        admin = makeUser(Role::Admin, 8);
    }

    long long countUsers() {
        Statement s = db.prepare("SELECT COUNT(*) FROM user_account");
        s.step();
        return s.columnInt(0);
    }

    DatabaseConnection db;
    SqliteUserRepository users;
    SqliteCourseRepository courses;
    FakeHasher hasher;
    AdminService service;
    AuthService auth;
    std::unique_ptr<User> admin;
};

}  // namespace

// ---- FR3: create users ----
TEST_F(AdminTest, CreateUserOfEveryRole) {
    service.createUser(*admin, Role::Lecturer, data("lec1"), "Passw0rd1");
    service.createUser(*admin, Role::Student, data("stu1"), "Passw0rd1");
    service.createUser(*admin, Role::HoD, data("hod1"), "Passw0rd1");
    service.createUser(*admin, Role::Admin, data("adm2"), "Passw0rd1");
    EXPECT_EQ(users.findByUsername("lec1")->getRole(), Role::Lecturer);
    EXPECT_EQ(users.findByUsername("stu1")->getRole(), Role::Student);
    EXPECT_EQ(users.findByUsername("hod1")->getRole(), Role::HoD);
    EXPECT_EQ(users.findByUsername("adm2")->getRole(), Role::Admin);
}

TEST_F(AdminTest, CreateUserReturnsTheNewIdAndStoresOnlyAHash) {
    const int id = service.createUser(*admin, Role::Lecturer, data("lec1"), "Passw0rd1");
    EXPECT_EQ(users.findById(id)->username(), "lec1");
    const std::string stored = users.findPasswordHash("lec1");
    EXPECT_EQ(stored, "fake:Passw0rd1");
    EXPECT_NE(stored, "Passw0rd1");
}

TEST_F(AdminTest, NewUserCanLogIn) {
    service.createUser(*admin, Role::Student, data("stu1"), "Passw0rd1");
    EXPECT_EQ(auth.login("stu1", "Passw0rd1")->getRole(), Role::Student);
}

TEST_F(AdminTest, WeakPasswordsAreRefusedAndNothingIsSaved) {
    const long long before = countUsers();
    for (const std::string weak : {"", "short1", "onlyletters", "12345678"}) {
        SCOPED_TRACE("password: '" + weak + "'");
        EXPECT_THROW(service.createUser(*admin, Role::Student, data("stu1"), weak),
                     std::invalid_argument);
    }
    EXPECT_EQ(countUsers(), before);
}

TEST_F(AdminTest, PasswordRuleBoundaryIsEightCharacters) {
    EXPECT_THROW(AdminService::checkPassword("Abcdef1"), std::invalid_argument);   // 7
    EXPECT_NO_THROW(AdminService::checkPassword("Abcdefg1"));                       // 8
}

TEST_F(AdminTest, BadUserDataIsRefused) {
    UserData noName = data("x");
    noName.username = "";
    EXPECT_THROW(service.createUser(*admin, Role::Lecturer, noName, "Passw0rd1"),
                 std::invalid_argument);
    UserData noNumber = data("stu1");
    noNumber.studentNumber = "";
    EXPECT_THROW(service.createUser(*admin, Role::Student, noNumber, "Passw0rd1"),
                 std::invalid_argument);
}

TEST_F(AdminTest, DuplicateUsernameIsRefused) {
    service.createUser(*admin, Role::Lecturer, data("same"), "Passw0rd1");
    EXPECT_THROW(service.createUser(*admin, Role::Student, data("same"), "Passw0rd1"),
                 DatabaseError);
}

TEST_F(AdminTest, SecondHoDForTheSameDepartmentIsRefused) {
    service.createUser(*admin, Role::HoD, data("hod1"), "Passw0rd1");
    EXPECT_THROW(service.createUser(*admin, Role::HoD, data("hod2"), "Passw0rd1"), DatabaseError);
    EXPECT_FALSE(users.findByUsername("hod2"));
}

// ---- FR3: deactivate and reactivate ----
TEST_F(AdminTest, DeactivatedUserCannotLogInUntilReactivated) {
    const int id = service.createUser(*admin, Role::Student, data("stu1"), "Passw0rd1");
    service.deactivateUser(*admin, id);
    EXPECT_FALSE(users.findById(id)->isActive());
    EXPECT_THROW(auth.login("stu1", "Passw0rd1"), AuthenticationError);
    service.reactivateUser(*admin, id);
    EXPECT_NE(auth.login("stu1", "Passw0rd1"), nullptr);
}

TEST_F(AdminTest, AdminCannotDeactivateTheirOwnAccount) {
    EXPECT_THROW(service.deactivateUser(*admin, 8), std::invalid_argument);
    EXPECT_TRUE(users.findById(8)->isActive());
}

TEST_F(AdminTest, DeactivateUnknownUserIsRefused) {
    EXPECT_THROW(service.deactivateUser(*admin, 99), std::invalid_argument);
    EXPECT_THROW(service.reactivateUser(*admin, 99), std::invalid_argument);
}

// ---- FR20: reset a password ----
TEST_F(AdminTest, ResetPasswordReplacesTheOldPassword) {
    const int id = service.createUser(*admin, Role::Student, data("stu1"), "Passw0rd1");
    service.resetPassword(*admin, id, "NewPassw0rd2");
    EXPECT_NE(auth.login("stu1", "NewPassw0rd2"), nullptr);
    EXPECT_THROW(auth.login("stu1", "Passw0rd1"), AuthenticationError);
}

TEST_F(AdminTest, ResetPasswordRefusesWeakPasswordsAndKeepsTheOldOne) {
    const int id = service.createUser(*admin, Role::Student, data("stu1"), "Passw0rd1");
    EXPECT_THROW(service.resetPassword(*admin, id, "weak"), std::invalid_argument);
    EXPECT_NE(auth.login("stu1", "Passw0rd1"), nullptr);
}

TEST_F(AdminTest, ResetPasswordForUnknownUserIsRefused) {
    EXPECT_THROW(service.resetPassword(*admin, 99, "Passw0rd1"), std::invalid_argument);
}

// ---- FR4: departments and courses ----
TEST_F(AdminTest, CreateDepartmentAndCourse) {
    const int dept = service.createDepartment(*admin, "Mathematics");
    const int course = service.createCourse(*admin, "MA1000", "Calculus", dept);
    auto found = courses.findById(course);
    ASSERT_TRUE(found.has_value());
    EXPECT_EQ(found->code(), "MA1000");
    EXPECT_EQ(found->departmentId(), dept);
}

TEST_F(AdminTest, DuplicateOrInvalidDepartmentsAndCoursesAreRefused) {
    service.createCourse(*admin, "CS1", "Intro", 1);
    EXPECT_THROW(service.createCourse(*admin, "CS1", "Again", 1), DatabaseError);
    EXPECT_THROW(service.createCourse(*admin, "CS2", "Title", 99), DatabaseError);
    EXPECT_THROW(service.createCourse(*admin, "", "Title", 1), std::invalid_argument);
    EXPECT_THROW(service.createDepartment(*admin, "Computer Science"), DatabaseError);
    EXPECT_THROW(service.createDepartment(*admin, "  "), std::invalid_argument);
}

// ---- FR5: assign lecturers and enrol students ----
TEST_F(AdminTest, AssignLecturerAndEnrolStudent) {
    const int course = service.createCourse(*admin, "CS1", "Intro", 1);
    const int lecturer = service.createUser(*admin, Role::Lecturer, data("lec1"), "Passw0rd1");
    const int student = service.createUser(*admin, Role::Student, data("stu1"), "Passw0rd1");
    service.assignLecturer(*admin, course, lecturer);
    service.enrolStudent(*admin, course, student);
    EXPECT_TRUE(courses.isLecturerAssigned(lecturer, course));
    EXPECT_TRUE(courses.isStudentEnrolled(student, course));
}

TEST_F(AdminTest, WrongKindOfUserCannotBeAssignedOrEnrolled) {
    const int course = service.createCourse(*admin, "CS1", "Intro", 1);
    const int lecturer = service.createUser(*admin, Role::Lecturer, data("lec1"), "Passw0rd1");
    const int student = service.createUser(*admin, Role::Student, data("stu1"), "Passw0rd1");
    EXPECT_THROW(service.assignLecturer(*admin, course, student), std::invalid_argument);
    EXPECT_THROW(service.enrolStudent(*admin, course, lecturer), DatabaseError);
}

TEST_F(AdminTest, AssignAndEnrolTwiceIsHarmless) {
    const int course = service.createCourse(*admin, "CS1", "Intro", 1);
    const int lecturer = service.createUser(*admin, Role::Lecturer, data("lec1"), "Passw0rd1");
    const int student = service.createUser(*admin, Role::Student, data("stu1"), "Passw0rd1");
    service.assignLecturer(*admin, course, lecturer);
    service.enrolStudent(*admin, course, student);
    EXPECT_NO_THROW(service.assignLecturer(*admin, course, lecturer));
    EXPECT_NO_THROW(service.enrolStudent(*admin, course, student));
}

// ---- FR2: only an active administrator ----
TEST_F(AdminTest, EveryAdminActionIsRefusedForEveryoneElse) {
    std::vector<std::unique_ptr<User>> callers;
    callers.push_back(makeUser(Role::Lecturer, 1));
    callers.push_back(makeUser(Role::HoD, 6));
    callers.push_back(makeUser(Role::Student, 2));
    callers.push_back(makeUser(Role::Admin, 8, false));  // a deactivated administrator
    const long long before = countUsers();

    for (const auto& caller : callers) {
        SCOPED_TRACE("caller role " + toString(caller->getRole()));
        EXPECT_THROW(service.createUser(*caller, Role::Student, data("x1"), "Passw0rd1"),
                     AccessDeniedError);
        EXPECT_THROW(service.deactivateUser(*caller, 8), AccessDeniedError);
        EXPECT_THROW(service.reactivateUser(*caller, 8), AccessDeniedError);
        EXPECT_THROW(service.resetPassword(*caller, 8, "Passw0rd1"), AccessDeniedError);
        EXPECT_THROW(service.createDepartment(*caller, "Physics"), AccessDeniedError);
        EXPECT_THROW(service.createCourse(*caller, "PH1", "Title", 1), AccessDeniedError);
        EXPECT_THROW(service.assignLecturer(*caller, 1, 1), AccessDeniedError);
        EXPECT_THROW(service.enrolStudent(*caller, 1, 1), AccessDeniedError);
    }
    EXPECT_EQ(countUsers(), before);
    EXPECT_TRUE(users.findById(8)->isActive());
}
