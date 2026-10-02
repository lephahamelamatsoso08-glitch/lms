#include <gtest/gtest.h>

#include <chrono>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>

#include "DatabaseConnection.h"
#include "DatabaseError.h"
#include "LateDays.h"
#include "SqliteAssignmentRepository.h"

using namespace lms::domain;
using lms::data::DatabaseConnection;
using lms::data::DatabaseError;
using lms::data::SqliteAssignmentRepository;
using namespace std::chrono;

namespace {

const TimePoint kDeadline = TimePoint{} + seconds(1794009600) + hours(10);

std::string readFile(const std::string& path) {
    std::ifstream in(path);
    if (!in) {
        throw std::runtime_error("cannot read " + path);
    }
    std::ostringstream text;
    text << in.rdbuf();
    return text.str();
}

class AssignmentRepoTest : public ::testing::Test {
protected:
    AssignmentRepoTest() : db(":memory:"), repo(db) {
        db.runScript(readFile(LMS_SCHEMA_PATH));
        db.runScript(R"sql(
INSERT INTO user_account(id, username, full_name, password_hash, role) VALUES
  (1, 'lec1', 'Lecturer', 'x', 'LECTURER'),
  (2, 'stu1', 'Student One', 'x', 'STUDENT'),
  (3, 'stu2', 'Student Two', 'x', 'STUDENT');
INSERT INTO student(user_id, student_number) VALUES (2, 'S001'), (3, 'S002');
INSERT INTO department(id, name) VALUES (1, 'Computer Science');
INSERT INTO course(id, code, title, department_id) VALUES
  (1, 'CS4433', 'Software Engineering', 1),
  (2, 'CS4000', 'Other Course', 1);
)sql");
    }

    int addAssignment(const std::string& title = "A1", int courseId = 1,
                      TimePoint deadline = kDeadline, double maxMark = 100.0) {
        return repo.add(Assignment(0, courseId, title, deadline, maxMark));
    }

    DatabaseConnection db;
    SqliteAssignmentRepository repo;
};

}  // namespace

TEST_F(AssignmentRepoTest, AddAndFindById) {
    const int id = addAssignment("Assignment 1", 1, kDeadline, 80.0);
    EXPECT_GT(id, 0);
    auto found = repo.findById(id);
    ASSERT_TRUE(found.has_value());
    EXPECT_EQ(found->id(), id);
    EXPECT_EQ(found->courseId(), 1);
    EXPECT_EQ(found->title(), "Assignment 1");
    EXPECT_EQ(found->deadline(), kDeadline);
    EXPECT_DOUBLE_EQ(found->maxMark(), 80.0);
}

TEST_F(AssignmentRepoTest, MissingAssignmentIsEmpty) {
    EXPECT_FALSE(repo.findById(99).has_value());
}

TEST_F(AssignmentRepoTest, FindByCourseIsOrderedByDeadlineAndFiltersByCourse) {
    addAssignment("Later", 1, kDeadline + hours(48));
    addAssignment("Earlier", 1, kDeadline);
    addAssignment("Other course", 2, kDeadline);
    auto list = repo.findByCourse(1);
    ASSERT_EQ(list.size(), 2u);
    EXPECT_EQ(list[0].title(), "Earlier");
    EXPECT_EQ(list[1].title(), "Later");
    EXPECT_EQ(repo.findByCourse(2).size(), 1u);
    EXPECT_TRUE(repo.findByCourse(99).empty());
}

TEST_F(AssignmentRepoTest, SavedAssignmentCannotBeAddedAgain) {
    EXPECT_THROW(repo.add(Assignment(5, 1, "A", kDeadline, 100.0)), std::invalid_argument);
}

TEST_F(AssignmentRepoTest, UnknownCourseIsRejected) {
    EXPECT_THROW(addAssignment("A", 99), DatabaseError);
}

TEST_F(AssignmentRepoTest, TitleWithQuotesIsStoredAsData) {
    const std::string title = "x'); DROP TABLE assignment;--";
    const int id = addAssignment(title);
    EXPECT_EQ(repo.findById(id)->title(), title);
}

// ---- Extensions (FR7, rule C2) ----
TEST_F(AssignmentRepoTest, ExtensionRoundTrip) {
    const int a = addAssignment();
    Extension e = repo.findById(a)->grantExtension(2, kDeadline + hours(48), "Power outage");
    const int id = repo.saveExtension(e);
    auto found = repo.findExtension(a, 2);
    ASSERT_TRUE(found.has_value());
    EXPECT_EQ(found->id(), id);
    EXPECT_EQ(found->studentId(), 2);
    EXPECT_EQ(found->newDeadline(), kDeadline + hours(48));
    EXPECT_EQ(found->reason(), "Power outage");
}

TEST_F(AssignmentRepoTest, NoExtensionIsEmpty) {
    const int a = addAssignment();
    EXPECT_FALSE(repo.findExtension(a, 2).has_value());
}

TEST_F(AssignmentRepoTest, DeadlineForWithoutExtensionIsTheNormalDeadline) {
    const int a = addAssignment();
    EXPECT_EQ(repo.getDeadlineFor(2, a), kDeadline);
}

TEST_F(AssignmentRepoTest, DeadlineForWithExtensionIsTheNewDeadline) {
    const int a = addAssignment();
    repo.saveExtension(repo.findById(a)->grantExtension(2, kDeadline + hours(48), "Reason"));
    EXPECT_EQ(repo.getDeadlineFor(2, a), kDeadline + hours(48));
}

TEST_F(AssignmentRepoTest, ExtensionOnlyAffectsThatStudent) {
    const int a = addAssignment();
    repo.saveExtension(repo.findById(a)->grantExtension(2, kDeadline + hours(48), "Reason"));
    EXPECT_EQ(repo.getDeadlineFor(3, a), kDeadline);
}

TEST_F(AssignmentRepoTest, SecondExtensionReplacesTheFirst) {
    const int a = addAssignment();
    const int first =
        repo.saveExtension(repo.findById(a)->grantExtension(2, kDeadline + hours(24), "First"));
    const int second =
        repo.saveExtension(repo.findById(a)->grantExtension(2, kDeadline + hours(72), "Second"));
    EXPECT_EQ(first, second);
    EXPECT_EQ(repo.getDeadlineFor(2, a), kDeadline + hours(72));
    EXPECT_EQ(repo.findExtension(a, 2)->reason(), "Second");
}

TEST_F(AssignmentRepoTest, ExtensionForUnknownStudentIsRejected) {
    const int a = addAssignment();
    EXPECT_THROW(repo.saveExtension(Extension(0, a, 99, kDeadline + hours(24), "Reason")),
                 DatabaseError);
}

TEST_F(AssignmentRepoTest, SavedExtensionCannotBeSavedAgain) {
    const int a = addAssignment();
    EXPECT_THROW(repo.saveExtension(Extension(7, a, 2, kDeadline + hours(24), "Reason")),
                 std::invalid_argument);
}

TEST_F(AssignmentRepoTest, DeadlineForUnknownAssignmentThrows) {
    EXPECT_THROW(repo.getDeadlineFor(2, 99), std::invalid_argument);
}

TEST_F(AssignmentRepoTest, ExtensionRemovesLateDaysThroughTheRepository) {
    const int a = addAssignment();
    const TimePoint submittedAt = kDeadline + hours(2);
    EXPECT_EQ(calculateLateDays(repo.getDeadlineFor(2, a), submittedAt), 1);
    repo.saveExtension(repo.findById(a)->grantExtension(2, kDeadline + hours(48), "Reason"));
    EXPECT_EQ(calculateLateDays(repo.getDeadlineFor(2, a), submittedAt), 0);
    EXPECT_EQ(calculateLateDays(repo.getDeadlineFor(3, a), submittedAt), 1);
}
