#include <gtest/gtest.h>

#include <chrono>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>

#include "DatabaseConnection.h"
#include "DatabaseError.h"
#include "SqliteSubmissionRepository.h"

using namespace lms::domain;
using lms::data::DatabaseConnection;
using lms::data::DatabaseError;
using lms::data::SqliteSubmissionRepository;
using S = SubmissionStatus;
using namespace std::chrono;

namespace {

const TimePoint kTime = TimePoint{} + seconds(1794009600) + hours(10);

std::string readFile(const std::string& path) {
    std::ifstream in(path);
    if (!in) {
        throw std::runtime_error("cannot read " + path);
    }
    std::ostringstream text;
    text << in.rdbuf();
    return text.str();
}

SubmissionVersion newVersion(const std::string& path, TimePoint at = kTime) {
    SubmissionVersion v;
    v.filePath = path;
    v.submittedAt = at;
    return v;
}

class SubmissionRepoTest : public ::testing::Test {
protected:
    SubmissionRepoTest() : db(":memory:"), repo(db) {
        db.runScript(readFile(LMS_SCHEMA_PATH));
        db.runScript(R"sql(
INSERT INTO user_account(id, username, full_name, password_hash, role) VALUES
  (1, 'lec1', 'Lecturer', 'x', 'LECTURER'),
  (2, 'stu1', 'Student One', 'x', 'STUDENT'),
  (3, 'stu2', 'Student Two', 'x', 'STUDENT');
INSERT INTO student(user_id, student_number) VALUES (2, 'S001'), (3, 'S002');
INSERT INTO department(id, name) VALUES (1, 'Computer Science');
INSERT INTO course(id, code, title, department_id) VALUES (1, 'CS4433', 'Software Engineering', 1);
INSERT INTO assignment(id, course_id, title, deadline, max_mark) VALUES
  (1, 1, 'A1', '2026-11-01T10:00:00Z', 100),
  (2, 1, 'A2', '2026-11-08T10:00:00Z', 100);
)sql");
    }

    int addOnTime(int studentId = 2, int assignmentId = 1, const std::string& path = "a.pdf") {
        return repo.add(Submission(0, assignmentId, studentId, S::Submitted, 0), newVersion(path));
    }

    DatabaseConnection db;
    SqliteSubmissionRepository repo;
};

}  // namespace

TEST_F(SubmissionRepoTest, AddStoresSubmissionAndFirstVersion) {
    const int id = addOnTime();
    EXPECT_GT(id, 0);
    auto found = repo.findById(id);
    ASSERT_TRUE(found.has_value());
    EXPECT_EQ(found->id(), id);
    EXPECT_EQ(found->assignmentId(), 1);
    EXPECT_EQ(found->studentId(), 2);
    EXPECT_EQ(found->status(), S::Submitted);
    EXPECT_EQ(found->lateDays(), 0);
    EXPECT_FALSE(found->rawMark().has_value());

    auto versions = repo.findVersions(id);
    ASSERT_EQ(versions.size(), 1u);
    EXPECT_EQ(versions[0].versionNo, 1);
    EXPECT_EQ(versions[0].filePath, "a.pdf");
    EXPECT_EQ(versions[0].submittedAt, kTime);
}

TEST_F(SubmissionRepoTest, LateSubmissionKeepsStatusAndLateDays) {
    const int id = repo.add(Submission(0, 1, 2, S::Late, 2), newVersion("late.pdf"));
    auto found = repo.findById(id);
    ASSERT_TRUE(found.has_value());
    EXPECT_EQ(found->status(), S::Late);
    EXPECT_EQ(found->lateDays(), 2);
}

TEST_F(SubmissionRepoTest, FindByStudentAndAssignment) {
    const int id = addOnTime(2, 1);
    addOnTime(3, 1);
    auto found = repo.findByStudentAndAssignment(2, 1);
    ASSERT_TRUE(found.has_value());
    EXPECT_EQ(found->id(), id);
    EXPECT_FALSE(repo.findByStudentAndAssignment(2, 2).has_value());
}

TEST_F(SubmissionRepoTest, MissingSubmissionIsEmpty) {
    EXPECT_FALSE(repo.findById(99).has_value());
}

TEST_F(SubmissionRepoTest, FindByAssignmentReturnsOnlyThatAssignment) {
    addOnTime(2, 1);
    addOnTime(3, 1);
    addOnTime(2, 2);
    EXPECT_EQ(repo.findByAssignment(1).size(), 2u);
    EXPECT_EQ(repo.findByAssignment(2).size(), 1u);
    EXPECT_TRUE(repo.findByAssignment(99).empty());
}

TEST_F(SubmissionRepoTest, SecondSubmissionForTheSameAssignmentIsRejectedAndNothingIsLeftBehind) {
    const int id = addOnTime(2, 1, "first.pdf");
    EXPECT_THROW(addOnTime(2, 1, "second.pdf"), DatabaseError);
    EXPECT_EQ(repo.findByAssignment(1).size(), 1u);
    ASSERT_EQ(repo.findVersions(id).size(), 1u);
    EXPECT_EQ(repo.findVersions(id)[0].filePath, "first.pdf");
}

TEST_F(SubmissionRepoTest, UnknownAssignmentOrStudentIsRejected) {
    EXPECT_THROW(addOnTime(2, 99), DatabaseError);
    EXPECT_THROW(addOnTime(99, 1), DatabaseError);
}

TEST_F(SubmissionRepoTest, AddRejectsBadInput) {
    EXPECT_THROW(repo.add(Submission(7, 1, 2, S::Submitted, 0), newVersion("a.pdf")),
                 std::invalid_argument);
    EXPECT_THROW(repo.add(Submission(0, 1, 2, S::Submitted, 0), newVersion("")),
                 std::invalid_argument);
    SubmissionVersion numbered = newVersion("a.pdf");
    numbered.versionNo = 3;
    EXPECT_THROW(repo.add(Submission(0, 1, 2, S::Submitted, 0), numbered), std::invalid_argument);
}

// ---- FR10: resubmission keeps the old versions ----
TEST_F(SubmissionRepoTest, AddVersionNumbersVersionsAndKeepsTheOldOnes) {
    const int id = addOnTime(2, 1, "v1.pdf");
    EXPECT_EQ(repo.addVersion(id, newVersion("v2.pdf", kTime + hours(1))), 2);
    EXPECT_EQ(repo.addVersion(id, newVersion("v3.pdf", kTime + hours(2))), 3);
    auto versions = repo.findVersions(id);
    ASSERT_EQ(versions.size(), 3u);
    EXPECT_EQ(versions[0].filePath, "v1.pdf");
    EXPECT_EQ(versions[1].filePath, "v2.pdf");
    EXPECT_EQ(versions[2].filePath, "v3.pdf");
    EXPECT_EQ(versions[2].submittedAt, kTime + hours(2));
}

TEST_F(SubmissionRepoTest, AddVersionToUnknownSubmissionIsRejected) {
    EXPECT_THROW(repo.addVersion(99, newVersion("a.pdf")), DatabaseError);
}

TEST_F(SubmissionRepoTest, AddVersionRejectsBadInput) {
    const int id = addOnTime();
    EXPECT_THROW(repo.addVersion(id, newVersion("")), std::invalid_argument);
    SubmissionVersion numbered = newVersion("a.pdf");
    numbered.versionNo = 2;
    EXPECT_THROW(repo.addVersion(id, numbered), std::invalid_argument);
}

TEST_F(SubmissionRepoTest, FilePathWithQuotesIsStoredAsData) {
    const std::string path = "x'); DROP TABLE submission;--.pdf";
    const int id = addOnTime(2, 1, path);
    EXPECT_EQ(repo.findVersions(id)[0].filePath, path);
}

// ---- Marking data (FR12 to FR14) ----
TEST_F(SubmissionRepoTest, SaveStoresMarkAndFeedback) {
    const int id = repo.add(Submission(0, 1, 2, S::Late, 2), newVersion("late.pdf"));
    Submission s = *repo.findById(id);
    s.applyMark(80.0, 60.0, "Late, 20% deducted");
    repo.save(s);

    auto reloaded = repo.findById(id);
    ASSERT_TRUE(reloaded.has_value());
    EXPECT_EQ(reloaded->status(), S::Marked);
    EXPECT_EQ(reloaded->lateDays(), 2);
    EXPECT_DOUBLE_EQ(*reloaded->rawMark(), 80.0);
    EXPECT_DOUBLE_EQ(*reloaded->finalMark(), 60.0);
    EXPECT_EQ(reloaded->feedback(), "Late, 20% deducted");
}

TEST_F(SubmissionRepoTest, SaveStoresModerationAndRelease) {
    const int id = addOnTime();
    Submission s = *repo.findById(id);
    s.applyMark(70.0, 70.0, "Fine");
    s.changeFinalMark(72.0);
    s.transitionTo(S::Moderated);
    s.release(kTime + hours(5));
    repo.save(s);

    auto reloaded = repo.findById(id);
    ASSERT_TRUE(reloaded.has_value());
    EXPECT_EQ(reloaded->status(), S::Released);
    EXPECT_DOUBLE_EQ(*reloaded->finalMark(), 72.0);
    ASSERT_TRUE(reloaded->releasedAt().has_value());
    EXPECT_EQ(*reloaded->releasedAt(), kTime + hours(5));
}

TEST_F(SubmissionRepoTest, SaveRejectsUnknownOrUnsavedSubmission) {
    EXPECT_THROW(repo.save(Submission(99, 1, 2, S::Submitted, 0)), std::invalid_argument);
    EXPECT_THROW(repo.save(Submission(0, 1, 2, S::Submitted, 0)), std::invalid_argument);
}

TEST_F(SubmissionRepoTest, MarkIsStoredSeparatelyPerStudent) {
    const int a = addOnTime(2, 1);
    const int b = addOnTime(3, 1);
    Submission s = *repo.findById(a);
    s.applyMark(50.0, 50.0, "OK");
    repo.save(s);
    EXPECT_EQ(repo.findById(a)->status(), S::Marked);
    EXPECT_EQ(repo.findById(b)->status(), S::Submitted);
    EXPECT_FALSE(repo.findById(b)->finalMark().has_value());
}
