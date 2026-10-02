#include <gtest/gtest.h>

#include <chrono>
#include <fstream>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>

#include "Argon2Hasher.h"
#include "AuthService.h"
#include "DatabaseConnection.h"
#include "SqliteUserRepository.h"
#include "UserFactory.h"
#include "UserRoles.h"

using namespace lms::domain;
using lms::application::AuthenticationError;
using lms::application::AuthService;
using lms::data::Argon2Hasher;
using lms::data::DatabaseConnection;
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

std::string loginError(const AuthService& service, const std::string& user,
                       const std::string& password) {
    try {
        service.login(user, password);
    } catch (const AuthenticationError& e) {
        return e.what();
    }
    return "NO ERROR";
}

// A hasher that counts how often verify() is called.
class SpyHasher : public IPasswordHasher {
public:
    std::string hash(const std::string& password) const override { return "hash:" + password; }
    bool verify(const std::string& password, const std::string& stored) const override {
        ++verifyCalls;
        return stored == "hash:" + password;
    }
    mutable int verifyCalls = 0;
};

class AuthTest : public ::testing::Test {
protected:
    AuthTest() : db(":memory:"), repo(db), service(repo, hasher) {
        db.runScript(readFile(LMS_SCHEMA_PATH));
        db.runScript("INSERT INTO department(id, name) VALUES (1, 'Computer Science')");
    }

    int addUser(Role role, const std::string& username, const std::string& password,
                bool active = true) {
        UserData d;
        d.username = username;
        d.fullName = "Full " + username;
        d.active = active;
        d.departmentId = 1;
        d.studentNumber = "S-" + username;
        return repo.add(*UserFactory::create(role, d), hasher.hash(password));
    }

    DatabaseConnection db;
    SqliteUserRepository repo;
    Argon2Hasher hasher;
    AuthService service;
};

}  // namespace

TEST_F(AuthTest, ValidLoginReturnsTheUser) {
    const int id = addUser(Role::Admin, "admin1", "Secret123");
    auto user = service.login("admin1", "Secret123");
    ASSERT_NE(user, nullptr);
    EXPECT_EQ(user->id(), id);
    EXPECT_EQ(user->getRole(), Role::Admin);
}

TEST_F(AuthTest, EveryRoleCanLogIn) {
    addUser(Role::Admin, "a", "pw-a");
    addUser(Role::Lecturer, "l", "pw-l");
    addUser(Role::HoD, "h", "pw-h");
    addUser(Role::Student, "s", "pw-s");
    EXPECT_EQ(service.login("a", "pw-a")->getRole(), Role::Admin);
    EXPECT_EQ(service.login("l", "pw-l")->getRole(), Role::Lecturer);
    EXPECT_EQ(service.login("h", "pw-h")->getRole(), Role::HoD);
    EXPECT_EQ(service.login("s", "pw-s")->getRole(), Role::Student);
}

TEST_F(AuthTest, WrongPasswordFails) {
    addUser(Role::Admin, "admin1", "Secret123");
    EXPECT_THROW(service.login("admin1", "Wrong"), AuthenticationError);
}

TEST_F(AuthTest, PasswordIsCaseSensitive) {
    addUser(Role::Admin, "admin1", "Secret123");
    EXPECT_THROW(service.login("admin1", "secret123"), AuthenticationError);
}

TEST_F(AuthTest, UnknownUserFails) {
    EXPECT_THROW(service.login("nobody", "Secret123"), AuthenticationError);
}

TEST_F(AuthTest, WrongPasswordAndUnknownUserGiveTheSameMessage) {
    addUser(Role::Admin, "admin1", "Secret123");
    EXPECT_EQ(loginError(service, "admin1", "Wrong"), loginError(service, "nobody", "Wrong"));
}

TEST_F(AuthTest, EmptyUsernameFails) {
    EXPECT_THROW(service.login("", "Secret123"), AuthenticationError);
}

TEST_F(AuthTest, EmptyPasswordFails) {
    addUser(Role::Admin, "admin1", "Secret123");
    EXPECT_THROW(service.login("admin1", ""), AuthenticationError);
}

TEST_F(AuthTest, DeactivatedUserCannotLogInEvenWithCorrectPassword) {
    addUser(Role::Student, "stu1", "Secret123", false);
    const std::string message = loginError(service, "stu1", "Secret123");
    EXPECT_NE(message.find("deactivated"), std::string::npos);
}

TEST_F(AuthTest, DeactivatedUserWithWrongPasswordGetsTheGenericMessage) {
    addUser(Role::Student, "stu1", "Secret123", false);
    EXPECT_EQ(loginError(service, "stu1", "Wrong"), loginError(service, "nobody", "Wrong"));
}

TEST_F(AuthTest, ReactivatedUserCanLogInAgain) {
    const int id = addUser(Role::Student, "stu1", "Secret123", false);
    repo.setActive(id, true);
    EXPECT_NE(service.login("stu1", "Secret123"), nullptr);
}

TEST(AuthSpyTest, UnknownUserStillRunsOneHashCheck) {
    DatabaseConnection db(":memory:");
    db.runScript(readFile(LMS_SCHEMA_PATH));
    SqliteUserRepository repo(db);
    SpyHasher spy;
    AuthService service(repo, spy);
    EXPECT_THROW(service.login("nobody", "pw"), AuthenticationError);
    EXPECT_EQ(spy.verifyCalls, 1);
}

// NFR1: login completes within 2 seconds with 800 students stored.
TEST_F(AuthTest, Nfr1LoginWithEightHundredStudentsIsUnderTwoSeconds) {
    const std::string hash = hasher.hash("Secret123");
    Statement insertUser = db.prepare(
        "INSERT INTO user_account(username, full_name, password_hash, role) "
        "VALUES (?, ?, ?, 'STUDENT')");
    Statement insertStudent =
        db.prepare("INSERT INTO student(user_id, student_number) VALUES (?, ?)");

    db.beginTransaction();
    for (int i = 1; i <= 800; ++i) {
        insertUser.bindText(1, "student" + std::to_string(i));
        insertUser.bindText(2, "Student " + std::to_string(i));
        insertUser.bindText(3, hash);
        insertUser.step();
        insertUser.reset();
        insertStudent.bindInt(1, db.lastInsertId());
        insertStudent.bindText(2, "S" + std::to_string(i));
        insertStudent.step();
        insertStudent.reset();
    }
    db.commit();

    const auto start = std::chrono::steady_clock::now();
    auto user = service.login("student800", "Secret123");
    const auto elapsed = std::chrono::steady_clock::now() - start;
    const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count();
    std::cout << "[ NFR1 ] login with 800 students stored took " << ms << " ms\n";

    ASSERT_NE(user, nullptr);
    EXPECT_EQ(user->username(), "student800");
    EXPECT_LT(ms, 2000);
}
