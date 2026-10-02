#include <gtest/gtest.h>

#include <chrono>
#include <fstream>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>

#include "AuditService.h"
#include "DatabaseConnection.h"
#include "DatabaseError.h"
#include "MarkAuditEntry.h"
#include "SqliteAuditRepository.h"
#include "UserFactory.h"

using namespace lms::domain;
using lms::application::AccessDeniedError;
using lms::application::AuditService;
using lms::data::DatabaseConnection;
using lms::data::DatabaseError;
using lms::data::SqliteAuditRepository;
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

MarkAuditEntry makeEntry(int submissionId, std::optional<double> oldValue, double newValue,
                         const std::string& reason = "Reason") {
    return MarkAuditEntry(0, submissionId, 1, kTime, oldValue, newValue, reason);
}

std::unique_ptr<User> makeUser(Role role, bool active = true) {
    UserData d;
    d.id = 10;
    d.username = "viewer";
    d.fullName = "Viewer";
    d.active = active;
    d.departmentId = 1;
    d.studentNumber = "S10";
    return UserFactory::create(role, d);
}

class AuditTest : public ::testing::Test {
protected:
    AuditTest() : db(":memory:"), repo(db) {
        db.runScript(readFile(LMS_SCHEMA_PATH));
        db.runScript(R"sql(
INSERT INTO user_account(id, username, full_name, password_hash, role) VALUES
  (1, 'lec1', 'Lecturer One', 'x', 'LECTURER'),
  (2, 'stu1', 'Student One', 'x', 'STUDENT'),
  (3, 'hod1', 'Hod One', 'x', 'HOD');
INSERT INTO student(user_id, student_number) VALUES (2, 'S001');
INSERT INTO department(id, name, hod_user_id) VALUES (1, 'Computer Science', 3);
INSERT INTO course(id, code, title, department_id) VALUES (1, 'CS4433', 'Software Engineering', 1);
INSERT INTO assignment(id, course_id, title, deadline, max_mark) VALUES
  (1, 1, 'A1', '2026-11-01T10:00:00Z', 100),
  (2, 1, 'A2', '2026-11-08T10:00:00Z', 100);
INSERT INTO submission(id, assignment_id, student_id, status) VALUES
  (1, 1, 2, 'MARKED'),
  (2, 2, 2, 'MARKED');
)sql");
    }

    DatabaseConnection db;
    SqliteAuditRepository repo;
};

}  // namespace

// ---- Repository ----
TEST_F(AuditTest, AddAndReadBack) {
    const int id = repo.add(makeEntry(1, 70.0, 75.0, "Re-marked question 3"));
    EXPECT_GT(id, 0);
    auto entries = repo.findBySubmission(1);
    ASSERT_EQ(entries.size(), 1u);
    EXPECT_EQ(entries[0].id(), id);
    EXPECT_EQ(entries[0].changedBy(), 1);
    ASSERT_TRUE(entries[0].oldValue().has_value());
    EXPECT_DOUBLE_EQ(*entries[0].oldValue(), 70.0);
    EXPECT_DOUBLE_EQ(entries[0].newValue(), 75.0);
    EXPECT_EQ(entries[0].reason(), "Re-marked question 3");
}

TEST_F(AuditTest, FirstMarkHasNoOldValue) {
    repo.add(makeEntry(1, std::nullopt, 70.0, "First mark"));
    auto entries = repo.findBySubmission(1);
    ASSERT_EQ(entries.size(), 1u);
    EXPECT_FALSE(entries[0].oldValue().has_value());
}

TEST_F(AuditTest, EntriesComeBackOldestFirst) {
    repo.add(makeEntry(1, std::nullopt, 60.0, "First"));
    repo.add(makeEntry(1, 60.0, 65.0, "Second"));
    auto entries = repo.findBySubmission(1);
    ASSERT_EQ(entries.size(), 2u);
    EXPECT_EQ(entries[0].reason(), "First");
    EXPECT_EQ(entries[1].reason(), "Second");
}

TEST_F(AuditTest, TimeIsStoredAndReadBack) {
    repo.add(makeEntry(1, std::nullopt, 60.0));
    EXPECT_EQ(repo.findBySubmission(1)[0].changedAt(), kTime);
}

TEST_F(AuditTest, UnknownSubmissionIsRejected) {
    EXPECT_THROW(repo.add(makeEntry(99, std::nullopt, 60.0)), DatabaseError);
}

TEST_F(AuditTest, UnknownUserIsRejected) {
    MarkAuditEntry entry(0, 1, 99, kTime, std::nullopt, 60.0, "Reason");
    EXPECT_THROW(repo.add(entry), DatabaseError);
}

TEST_F(AuditTest, SavedEntryCannotBeAddedAgain) {
    MarkAuditEntry saved(5, 1, 1, kTime, std::nullopt, 60.0, "Reason");
    EXPECT_THROW(repo.add(saved), std::invalid_argument);
}

TEST_F(AuditTest, FindAllReturnsEntriesOfAllSubmissions) {
    repo.add(makeEntry(1, std::nullopt, 60.0));
    repo.add(makeEntry(2, std::nullopt, 80.0));
    EXPECT_EQ(repo.findAll().size(), 2u);
    EXPECT_EQ(repo.findBySubmission(2).size(), 1u);
}

// ---- The entry checks its own data ----
TEST(MarkAuditEntryTest, EntryNeedsAReason) {
    EXPECT_THROW(makeEntry(1, std::nullopt, 60.0, ""), std::invalid_argument);
    EXPECT_THROW(makeEntry(1, std::nullopt, 60.0, "   "), std::invalid_argument);
}

TEST(MarkAuditEntryTest, EntryRejectsNegativeMarks) {
    EXPECT_THROW(makeEntry(1, std::nullopt, -1.0), std::invalid_argument);
    EXPECT_THROW(makeEntry(1, -5.0, 60.0), std::invalid_argument);
}

TEST(MarkAuditEntryTest, EntryNeedsASubmissionAndAUser) {
    EXPECT_THROW(MarkAuditEntry(0, 0, 1, kTime, std::nullopt, 60.0, "R"), std::invalid_argument);
    EXPECT_THROW(MarkAuditEntry(0, 1, 0, kTime, std::nullopt, 60.0, "R"), std::invalid_argument);
}

// ---- Service ----
TEST_F(AuditTest, ServiceUsesTheClockForTheTime) {
    AuditService service(repo, [] { return kTime; });
    service.recordMarkChange(1, 1, std::nullopt, 70.0, "First mark");
    EXPECT_EQ(repo.findBySubmission(1)[0].changedAt(), kTime);
}

TEST_F(AuditTest, ServiceRecordsOldAndNewValue) {
    AuditService service(repo, [] { return kTime; });
    service.recordMarkChange(1, 3, 70.0, 78.0, "Moderation: adjusted");
    auto entries = repo.findBySubmission(1);
    ASSERT_EQ(entries.size(), 1u);
    EXPECT_EQ(entries[0].changedBy(), 3);
    EXPECT_DOUBLE_EQ(*entries[0].oldValue(), 70.0);
    EXPECT_DOUBLE_EQ(entries[0].newValue(), 78.0);
}

TEST_F(AuditTest, InvalidChangeIsNotSaved) {
    AuditService service(repo, [] { return kTime; });
    EXPECT_THROW(service.recordMarkChange(1, 1, 70.0, 75.0, ""), std::invalid_argument);
    EXPECT_THROW(service.recordMarkChange(1, 1, 70.0, -1.0, "Reason"), std::invalid_argument);
    EXPECT_TRUE(repo.findAll().empty());
}

TEST_F(AuditTest, HoDCanViewTheAuditLog) {
    AuditService service(repo, [] { return kTime; });
    service.recordMarkChange(1, 1, std::nullopt, 70.0, "First mark");
    auto hod = makeUser(Role::HoD);
    EXPECT_EQ(service.all(*hod).size(), 1u);
    EXPECT_EQ(service.history(*hod, 1).size(), 1u);
}

TEST_F(AuditTest, AdminCanViewTheAuditLog) {
    AuditService service(repo, [] { return kTime; });
    auto admin = makeUser(Role::Admin);
    EXPECT_NO_THROW(service.all(*admin));
}

TEST_F(AuditTest, LecturerCannotViewTheAuditLog) {
    AuditService service(repo, [] { return kTime; });
    auto lecturer = makeUser(Role::Lecturer);
    EXPECT_THROW(service.all(*lecturer), AccessDeniedError);
    EXPECT_THROW(service.history(*lecturer, 1), AccessDeniedError);
}

TEST_F(AuditTest, StudentCannotViewTheAuditLog) {
    AuditService service(repo, [] { return kTime; });
    auto student = makeUser(Role::Student);
    EXPECT_THROW(service.all(*student), AccessDeniedError);
}

TEST_F(AuditTest, InactiveHoDCannotViewTheAuditLog) {
    AuditService service(repo, [] { return kTime; });
    auto hod = makeUser(Role::HoD, false);
    EXPECT_THROW(service.all(*hod), AccessDeniedError);
}

TEST_F(AuditTest, HistoryShowsOnlyThatSubmission) {
    AuditService service(repo, [] { return kTime; });
    service.recordMarkChange(1, 1, std::nullopt, 70.0, "First mark");
    service.recordMarkChange(2, 1, std::nullopt, 80.0, "First mark");
    auto hod = makeUser(Role::HoD);
    EXPECT_EQ(service.history(*hod, 1).size(), 1u);
    EXPECT_EQ(service.history(*hod, 2).size(), 1u);
    EXPECT_EQ(service.all(*hod).size(), 2u);
}
