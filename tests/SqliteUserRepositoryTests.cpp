#include <gtest/gtest.h>

#include <fstream>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>

#include "Argon2Hasher.h"
#include "DatabaseConnection.h"
#include "DatabaseError.h"
#include "SqliteUserRepository.h"
#include "UserFactory.h"
#include "UserRoles.h"

using namespace lms::domain;
using lms::data::Argon2Hasher;
using lms::data::DatabaseConnection;
using lms::data::DatabaseError;
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

class UserRepositoryTest : public ::testing::Test {
protected:
    UserRepositoryTest() : db(":memory:"), repo(db) {
        db.runScript(readFile(LMS_SCHEMA_PATH));
        db.runScript("INSERT INTO department(id, name) VALUES (1, 'Computer Science')");
    }

    static UserData data(const std::string& username) {
        UserData d;
        d.username = username;
        d.fullName = "Full " + username;
        d.departmentId = 1;
        d.studentNumber = "S-" + username;
        return d;
    }

    int addUser(Role role, const std::string& username) {
        return repo.add(*UserFactory::create(role, data(username)), "stored-hash");
    }

    long long countUsers() {
        Statement s = db.prepare("SELECT COUNT(*) FROM user_account");
        s.step();
        return s.columnInt(0);
    }

    DatabaseConnection db;
    SqliteUserRepository repo;
};

}  // namespace

TEST_F(UserRepositoryTest, AddAdminThenFindByUsername) {
    const int id = addUser(Role::Admin, "admin1");
    EXPECT_GT(id, 0);
    auto found = repo.findByUsername("admin1");
    ASSERT_NE(found, nullptr);
    EXPECT_EQ(found->getRole(), Role::Admin);
    EXPECT_EQ(found->id(), id);
    EXPECT_EQ(found->fullName(), "Full admin1");
    EXPECT_TRUE(found->isActive());
}

TEST_F(UserRepositoryTest, FindByIdWorks) {
    const int id = addUser(Role::Lecturer, "lec1");
    auto found = repo.findById(id);
    ASSERT_NE(found, nullptr);
    EXPECT_EQ(found->username(), "lec1");
    EXPECT_EQ(found->getRole(), Role::Lecturer);
}

TEST_F(UserRepositoryTest, StudentKeepsStudentNumber) {
    const int id = addUser(Role::Student, "stu1");
    auto found = repo.findById(id);
    ASSERT_NE(found, nullptr);
    auto* student = dynamic_cast<Student*>(found.get());
    ASSERT_NE(student, nullptr);
    EXPECT_EQ(student->studentNumber(), "S-stu1");
}

TEST_F(UserRepositoryTest, HoDIsLinkedToTheirDepartment) {
    const int id = addUser(Role::HoD, "hod1");
    auto found = repo.findById(id);
    ASSERT_NE(found, nullptr);
    auto* hod = dynamic_cast<HoD*>(found.get());
    ASSERT_NE(hod, nullptr);
    EXPECT_EQ(hod->departmentId(), 1);
}

TEST_F(UserRepositoryTest, MissingUserIsNull) {
    EXPECT_TRUE(repo.findById(99) == nullptr);
    EXPECT_TRUE(repo.findByUsername("nobody") == nullptr);
}

TEST_F(UserRepositoryTest, DuplicateUsernameIsRejected) {
    addUser(Role::Admin, "same");
    EXPECT_THROW(addUser(Role::Lecturer, "same"), DatabaseError);
    EXPECT_EQ(countUsers(), 1);
}

// The user row and the student row are saved together or not at all.
TEST_F(UserRepositoryTest, DuplicateStudentNumberRollsBackTheWholeUser) {
    UserData a = data("stu1");
    a.studentNumber = "S001";
    repo.add(*UserFactory::create(Role::Student, a), "stored-hash");

    UserData b = data("stu2");
    b.studentNumber = "S001";
    EXPECT_THROW(repo.add(*UserFactory::create(Role::Student, b), "stored-hash"), DatabaseError);
    EXPECT_TRUE(repo.findByUsername("stu2") == nullptr);
    EXPECT_EQ(countUsers(), 1);
}

TEST_F(UserRepositoryTest, HoDWithUnknownDepartmentRollsBack) {
    UserData d = data("hod1");
    d.departmentId = 99;
    EXPECT_THROW(repo.add(*UserFactory::create(Role::HoD, d), "stored-hash"), DatabaseError);
    EXPECT_TRUE(repo.findByUsername("hod1") == nullptr);
    EXPECT_EQ(countUsers(), 0);
}

TEST_F(UserRepositoryTest, SecondHoDForTheSameDepartmentIsRejected) {
    addUser(Role::HoD, "hod1");
    EXPECT_THROW(addUser(Role::HoD, "hod2"), DatabaseError);
    EXPECT_TRUE(repo.findByUsername("hod2") == nullptr);
}

TEST_F(UserRepositoryTest, AlreadySavedUserCannotBeAddedAgain) {
    UserData d = data("old");
    d.id = 5;
    EXPECT_THROW(repo.add(*UserFactory::create(Role::Admin, d), "stored-hash"),
                 std::invalid_argument);
}

TEST_F(UserRepositoryTest, EmptyPasswordHashIsRejected) {
    EXPECT_THROW(repo.add(*UserFactory::create(Role::Admin, data("a")), ""),
                 std::invalid_argument);
}

// ---- NFR3: only salted hashes are stored ----
TEST_F(UserRepositoryTest, StoredPasswordIsAHashAndNeverPlainText) {
    Argon2Hasher hasher;
    repo.add(*UserFactory::create(Role::Admin, data("admin1")), hasher.hash("Secret123"));

    const std::string stored = repo.findPasswordHash("admin1");
    EXPECT_TRUE(hasher.verify("Secret123", stored));
    EXPECT_FALSE(hasher.verify("wrong", stored));

    Statement s = db.prepare("SELECT COUNT(*) FROM user_account WHERE password_hash = ?");
    s.bindText(1, "Secret123");
    s.step();
    EXPECT_EQ(s.columnInt(0), 0);
}

TEST_F(UserRepositoryTest, UnknownUserHasNoPasswordHash) {
    EXPECT_EQ(repo.findPasswordHash("nobody"), "");
}

TEST_F(UserRepositoryTest, SetPasswordHashChangesTheHash) {
    const int id = addUser(Role::Admin, "admin1");
    repo.setPasswordHash(id, "new-hash");
    EXPECT_EQ(repo.findPasswordHash("admin1"), "new-hash");
}

TEST_F(UserRepositoryTest, SetPasswordHashRejectsBadInput) {
    const int id = addUser(Role::Admin, "admin1");
    EXPECT_THROW(repo.setPasswordHash(id, ""), std::invalid_argument);
    EXPECT_THROW(repo.setPasswordHash(99, "hash"), std::invalid_argument);
}

// ---- FR3: deactivate and reactivate ----
TEST_F(UserRepositoryTest, SetActiveDeactivatesAndReactivates) {
    const int id = addUser(Role::Student, "stu1");
    repo.setActive(id, false);
    EXPECT_FALSE(repo.findById(id)->isActive());
    repo.setActive(id, true);
    EXPECT_TRUE(repo.findById(id)->isActive());
}

TEST_F(UserRepositoryTest, SetActiveOnUnknownUserThrows) {
    EXPECT_THROW(repo.setActive(99, false), std::invalid_argument);
}
