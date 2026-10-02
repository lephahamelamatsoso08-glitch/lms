#include <gtest/gtest.h>

#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>

#include "DatabaseConnection.h"
#include "DatabaseError.h"
#include "Statement.h"

using lms::data::DatabaseConnection;
using lms::data::DatabaseError;
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

class DatabaseTest : public ::testing::Test {
protected:
    DatabaseTest() : db(":memory:") { db.runScript(readFile(LMS_SCHEMA_PATH)); }

    long long scalar(const std::string& sql) {
        Statement s = db.prepare(sql);
        s.step();
        return s.columnInt(0);
    }

    void insertUser(const std::string& username, const std::string& role) {
        Statement s = db.prepare(
            "INSERT INTO user_account(username, full_name, password_hash, role) "
            "VALUES (?, 'Name', 'hash', ?)");
        s.bindText(1, username);
        s.bindText(2, role);
        s.step();
    }

    void seed() {
        db.runScript(R"sql(
INSERT INTO user_account(id, username, full_name, password_hash, role) VALUES
  (1, 'lec1', 'Lecturer One', 'x', 'LECTURER'),
  (2, 'stu1', 'Student One', 'x', 'STUDENT'),
  (3, 'hod1', 'Hod One', 'x', 'HOD');
INSERT INTO student(user_id, student_number) VALUES (2, 'S001');
INSERT INTO department(id, name, hod_user_id) VALUES (1, 'Computer Science', 3);
INSERT INTO course(id, code, title, department_id) VALUES (1, 'CS4433', 'Software Engineering', 1);
INSERT INTO assignment(id, course_id, title, deadline, max_mark)
  VALUES (1, 1, 'A1', '2026-11-01T10:00:00Z', 100);
INSERT INTO submission(id, assignment_id, student_id, status) VALUES (1, 1, 2, 'SUBMITTED');
)sql");
    }

    DatabaseConnection db;
};

}  // namespace

TEST_F(DatabaseTest, SchemaCreatesAllTwelveTables) {
    EXPECT_EQ(scalar("SELECT COUNT(*) FROM sqlite_master "
                     "WHERE type = 'table' AND name NOT LIKE 'sqlite_%'"),
              12);
}

// ---- NFR4: input is data, never SQL ----
TEST_F(DatabaseTest, ParameterisedQueryTreatsInputAsData) {
    const std::string nasty = "x'); DROP TABLE user_account;--";
    Statement insert = db.prepare(
        "INSERT INTO user_account(username, full_name, password_hash, role) "
        "VALUES (?, ?, ?, ?)");
    insert.bindText(1, nasty);
    insert.bindText(2, "Name");
    insert.bindText(3, "hash");
    insert.bindText(4, "ADMIN");
    insert.step();

    Statement read = db.prepare("SELECT username FROM user_account WHERE username = ?");
    read.bindText(1, nasty);
    ASSERT_TRUE(read.step());
    EXPECT_EQ(read.columnText(0), nasty);
    EXPECT_EQ(scalar("SELECT COUNT(*) FROM user_account"), 1);
}

// ---- The database enforces the design rules ----
TEST_F(DatabaseTest, DuplicateUsernameIsRejected) {
    insertUser("a", "ADMIN");
    EXPECT_THROW(insertUser("a", "STUDENT"), DatabaseError);
}

TEST_F(DatabaseTest, InvalidRoleIsRejected) {
    EXPECT_THROW(insertUser("a", "PRINCIPAL"), DatabaseError);
}

TEST_F(DatabaseTest, ForeignKeyIsEnforced) {
    EXPECT_THROW(db.runScript("INSERT INTO course(code, title, department_id) "
                              "VALUES ('C1', 'T', 999)"),
                 DatabaseError);
}

TEST_F(DatabaseTest, InvalidSubmissionStatusIsRejected) {
    seed();
    EXPECT_THROW(db.runScript("UPDATE submission SET status = 'DONE' WHERE id = 1"),
                 DatabaseError);
}

TEST_F(DatabaseTest, OnlyOneSubmissionPerStudentPerAssignment) {
    seed();
    EXPECT_THROW(db.runScript("INSERT INTO submission(assignment_id, student_id, status) "
                              "VALUES (1, 2, 'SUBMITTED')"),
                 DatabaseError);
}

TEST_F(DatabaseTest, MaxMarkMustBePositive) {
    seed();
    EXPECT_THROW(db.runScript("INSERT INTO assignment(course_id, title, deadline, max_mark) "
                              "VALUES (1, 'A2', '2026-11-02T10:00:00Z', 0)"),
                 DatabaseError);
}

// ---- Audit log (FR15) ----
TEST_F(DatabaseTest, AuditEntryNeedsAReason) {
    seed();
    Statement s = db.prepare(
        "INSERT INTO mark_audit(submission_id, changed_by, changed_at, old_value, new_value, "
        "reason) VALUES (1, 1, '2026-11-03T09:00:00Z', ?, 70, ?)");
    s.bindNull(1);
    s.bindText(2, "");
    EXPECT_THROW(s.step(), DatabaseError);
}

TEST_F(DatabaseTest, FirstMarkHasNullOldValue) {
    seed();
    Statement s = db.prepare(
        "INSERT INTO mark_audit(submission_id, changed_by, changed_at, old_value, new_value, "
        "reason) VALUES (1, 1, '2026-11-03T09:00:00Z', ?, 70, ?)");
    s.bindNull(1);
    s.bindText(2, "First mark");
    s.step();

    Statement read = db.prepare("SELECT old_value, new_value FROM mark_audit");
    ASSERT_TRUE(read.step());
    EXPECT_TRUE(read.isNull(0));
    EXPECT_DOUBLE_EQ(read.columnDouble(1), 70.0);
}

// ---- Transactions ----
TEST_F(DatabaseTest, RollbackUndoesChanges) {
    db.beginTransaction();
    insertUser("a", "ADMIN");
    db.rollback();
    EXPECT_EQ(scalar("SELECT COUNT(*) FROM user_account"), 0);
}

TEST_F(DatabaseTest, CommitKeepsChanges) {
    db.beginTransaction();
    insertUser("a", "ADMIN");
    db.commit();
    EXPECT_EQ(scalar("SELECT COUNT(*) FROM user_account"), 1);
}

// ---- Errors do not crash (NFR6) ----
TEST_F(DatabaseTest, BadSqlThrowsDatabaseError) {
    EXPECT_THROW(db.prepare("SELEC nothing"), DatabaseError);
}

TEST_F(DatabaseTest, BindingOutOfRangeThrowsDatabaseError) {
    Statement s = db.prepare("SELECT ?");
    EXPECT_THROW(s.bindInt(2, 1), DatabaseError);
}

TEST_F(DatabaseTest, LastInsertIdReturnsTheNewId) {
    insertUser("a", "ADMIN");
    EXPECT_EQ(db.lastInsertId(), 1);
}

TEST(DatabaseOpenTest, CannotOpenInMissingFolder) {
    EXPECT_THROW(DatabaseConnection bad("/no/such/folder/lms.db"), DatabaseError);
}
