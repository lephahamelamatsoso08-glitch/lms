#include <gtest/gtest.h>

#include <chrono>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>

#include "AuditService.h"
#include "DatabaseConnection.h"
#include "IAuditRepository.h"
#include "SqliteAssignmentRepository.h"
#include "SqliteAuditRepository.h"
#include "SqliteCourseRepository.h"
#include "SqliteSubmissionRepository.h"
#include "SqliteTransactionRunner.h"
#include "SubmissionService.h"
#include "TenPercentPerDayPenalty.h"
#include "TimeFormat.h"
#include "UserFactory.h"

using namespace lms::domain;
using namespace lms::application;
using lms::data::DatabaseConnection;
using lms::data::SqliteAssignmentRepository;
using lms::data::SqliteAuditRepository;
using lms::data::SqliteCourseRepository;
using lms::data::SqliteSubmissionRepository;
using lms::data::SqliteTransactionRunner;
using S = SubmissionStatus;
using namespace std::chrono;

namespace {

const TimePoint kDeadline = fromIso8601("2026-11-01T10:00:00Z");

std::string readFile(const std::string& path) {
    std::ifstream in(path);
    if (!in) {
        throw std::runtime_error("cannot read " + path);
    }
    std::ostringstream text;
    text << in.rdbuf();
    return text.str();
}

std::unique_ptr<User> makeUser(Role role, int id, bool active = true) {
    UserData d;
    d.id = id;
    d.username = "user" + std::to_string(id);
    d.fullName = "User " + std::to_string(id);
    d.active = active;
    d.departmentId = 1;
    d.studentNumber = "S" + std::to_string(id);
    return UserFactory::create(role, d);
}

// An audit repository that always fails, to test that nothing is half-saved.
class FailingAuditRepository : public IAuditRepository {
public:
    int add(const MarkAuditEntry&) override { throw std::runtime_error("disk full"); }
    std::vector<MarkAuditEntry> findBySubmission(int) const override { return {}; }
    std::vector<MarkAuditEntry> findAll() const override { return {}; }
};

class MarkTest : public ::testing::Test {
protected:
    MarkTest()
        : db(":memory:"),
          submissions(db),
          assignments(db),
          courses(db),
          auditRepo(db),
          audit(auditRepo, [this] { return now; }),
          runner(db),
          now(kDeadline + hours(30)),
          service(submissions, assignments, courses, penalty, audit, [this] { return now; },
                  &runner) {
        db.runScript(readFile(LMS_SCHEMA_PATH));
        db.runScript(R"sql(
INSERT INTO user_account(id, username, full_name, password_hash, role) VALUES
  (1, 'lec1', 'Lecturer One', 'x', 'LECTURER'),
  (2, 'stu1', 'Student One', 'x', 'STUDENT'),
  (3, 'stu2', 'Student Two', 'x', 'STUDENT'),
  (5, 'lec2', 'Lecturer Two', 'x', 'LECTURER'),
  (6, 'adm1', 'Admin', 'x', 'ADMIN');
INSERT INTO student(user_id, student_number) VALUES (2, 'S2'), (3, 'S3');
INSERT INTO department(id, name) VALUES (1, 'Computer Science');
INSERT INTO course(id, code, title, department_id) VALUES (1, 'CS4433', 'Software Engineering', 1);
INSERT INTO assignment(id, course_id, title, deadline, max_mark)
  VALUES (1, 1, 'A1', '2026-11-01T10:00:00Z', 100);
INSERT INTO enrolment(course_id, student_id) VALUES (1, 2), (1, 3);
INSERT INTO course_lecturer(course_id, lecturer_id) VALUES (1, 1);
)sql");
        lecturer = makeUser(Role::Lecturer, 1);
    }

    SubmissionVersion file() {
        SubmissionVersion v;
        v.filePath = "work.pdf";
        v.submittedAt = kDeadline - hours(1);
        return v;
    }

    int addOnTime(int studentId = 2) {
        return submissions.add(Submission(0, 1, studentId, S::Submitted, 0), file());
    }

    int addLate(int lateDays, int studentId = 3) {
        return submissions.add(Submission(0, 1, studentId, S::Late, lateDays), file());
    }

    DatabaseConnection db;
    SqliteSubmissionRepository submissions;
    SqliteAssignmentRepository assignments;
    SqliteCourseRepository courses;
    SqliteAuditRepository auditRepo;
    AuditService audit;
    SqliteTransactionRunner runner;
    TenPercentPerDayPenalty penalty;
    TimePoint now;
    SubmissionService service;
    std::unique_ptr<User> lecturer;
};

}  // namespace

// ---- UC-7 main flow ----
TEST_F(MarkTest, FirstMarkStoresMarkAndFeedbackAndMovesToMarked) {
    const int id = addOnTime();
    EXPECT_DOUBLE_EQ(service.mark(*lecturer, id, 80.0, "Good work"), 80.0);
    Submission s = *submissions.findById(id);
    EXPECT_EQ(s.status(), S::Marked);
    EXPECT_DOUBLE_EQ(*s.rawMark(), 80.0);
    EXPECT_DOUBLE_EQ(*s.finalMark(), 80.0);
    EXPECT_EQ(s.feedback(), "Good work");
}

TEST_F(MarkTest, FirstMarkWritesAnAuditEntryWithNoOldValue) {
    const int id = addOnTime();
    service.mark(*lecturer, id, 80.0, "Good work");
    auto entries = auditRepo.findBySubmission(id);
    ASSERT_EQ(entries.size(), 1u);
    EXPECT_EQ(entries[0].changedBy(), 1);
    EXPECT_FALSE(entries[0].oldValue().has_value());
    EXPECT_DOUBLE_EQ(entries[0].newValue(), 80.0);
    EXPECT_EQ(entries[0].reason(), "Initial mark");
    EXPECT_EQ(entries[0].changedAt(), now);
}

TEST_F(MarkTest, LatePenaltyIsAppliedAndTheRawMarkIsKept) {
    const int id = addLate(2);
    EXPECT_NEAR(service.mark(*lecturer, id, 80.0, "Late work"), 60.0, 1e-9);
    Submission s = *submissions.findById(id);
    EXPECT_DOUBLE_EQ(*s.rawMark(), 80.0);
    EXPECT_NEAR(*s.finalMark(), 60.0, 1e-9);
    EXPECT_NEAR(auditRepo.findBySubmission(id)[0].newValue(), 60.0, 1e-9);
}

TEST_F(MarkTest, FourDaysLateGivesAFinalMarkOfZero) {
    const int id = addLate(4);
    EXPECT_DOUBLE_EQ(service.mark(*lecturer, id, 80.0, "Very late"), 0.0);
    EXPECT_DOUBLE_EQ(*submissions.findById(id)->finalMark(), 0.0);
}

TEST_F(MarkTest, MarkOfZeroAndMarkOfMaximumAreBothAccepted) {
    const int a = addOnTime(2);
    const int b = addOnTime(3);
    EXPECT_DOUBLE_EQ(service.mark(*lecturer, a, 0.0, "Nothing handed in"), 0.0);
    EXPECT_DOUBLE_EQ(service.mark(*lecturer, b, 100.0, "Perfect"), 100.0);
}

// ---- A1: re-mark ----
TEST_F(MarkTest, ReMarkWithAReasonRecordsOldAndNewValue) {
    const int id = addOnTime();
    service.mark(*lecturer, id, 80.0, "Good work");
    EXPECT_DOUBLE_EQ(service.mark(*lecturer, id, 85.0, "Better feedback", "Question 3 re-read"),
                     85.0);
    Submission s = *submissions.findById(id);
    EXPECT_EQ(s.status(), S::Marked);
    EXPECT_DOUBLE_EQ(*s.finalMark(), 85.0);

    auto entries = auditRepo.findBySubmission(id);
    ASSERT_EQ(entries.size(), 2u);
    EXPECT_DOUBLE_EQ(*entries[1].oldValue(), 80.0);
    EXPECT_DOUBLE_EQ(entries[1].newValue(), 85.0);
    EXPECT_EQ(entries[1].reason(), "Question 3 re-read");
}

TEST_F(MarkTest, ReMarkWithoutAReasonIsRefusedAndNothingChanges) {
    const int id = addOnTime();
    service.mark(*lecturer, id, 80.0, "Good work");
    EXPECT_THROW(service.mark(*lecturer, id, 85.0, "Changed", ""), std::invalid_argument);
    EXPECT_THROW(service.mark(*lecturer, id, 85.0, "Changed", "   "), std::invalid_argument);
    EXPECT_DOUBLE_EQ(*submissions.findById(id)->finalMark(), 80.0);
    EXPECT_EQ(auditRepo.findBySubmission(id).size(), 1u);
}

// ---- Exception flows ----
TEST_F(MarkTest, MarkOutsideZeroToMaximumIsRefusedAndNothingIsSaved) {
    const int id = addOnTime();
    EXPECT_THROW(service.mark(*lecturer, id, 101.0, "Too high"), std::invalid_argument);
    EXPECT_THROW(service.mark(*lecturer, id, -1.0, "Negative"), std::invalid_argument);
    EXPECT_EQ(submissions.findById(id)->status(), S::Submitted);
    EXPECT_TRUE(auditRepo.findAll().empty());
}

TEST_F(MarkTest, BlankFeedbackIsRefusedAndNothingIsSaved) {
    const int id = addOnTime();
    EXPECT_THROW(service.mark(*lecturer, id, 80.0, ""), std::invalid_argument);
    EXPECT_THROW(service.mark(*lecturer, id, 80.0, "   "), std::invalid_argument);
    EXPECT_EQ(submissions.findById(id)->status(), S::Submitted);
    EXPECT_FALSE(submissions.findById(id)->finalMark().has_value());
    EXPECT_TRUE(auditRepo.findAll().empty());
}

TEST_F(MarkTest, UnknownSubmissionIsRefused) {
    EXPECT_THROW(service.mark(*lecturer, 99, 80.0, "Feedback"), std::invalid_argument);
}

TEST_F(MarkTest, ModeratedWorkCannotBeReMarked) {
    const int id = addOnTime();
    service.mark(*lecturer, id, 80.0, "Good work");
    Submission s = *submissions.findById(id);
    s.transitionTo(S::Moderated);
    submissions.save(s);

    EXPECT_THROW(service.mark(*lecturer, id, 90.0, "Changed my mind", "Reason"), std::logic_error);
    EXPECT_DOUBLE_EQ(*submissions.findById(id)->finalMark(), 80.0);
    EXPECT_EQ(auditRepo.findBySubmission(id).size(), 1u);
}

// ---- Who may mark ----
TEST_F(MarkTest, OnlyLecturersCanMark) {
    const int id = addOnTime();
    auto student = makeUser(Role::Student, 2);
    auto hod = makeUser(Role::HoD, 7);
    auto admin = makeUser(Role::Admin, 6);
    EXPECT_THROW(service.mark(*student, id, 80.0, "x"), AccessDeniedError);
    EXPECT_THROW(service.mark(*hod, id, 80.0, "x"), AccessDeniedError);
    EXPECT_THROW(service.mark(*admin, id, 80.0, "x"), AccessDeniedError);
    EXPECT_EQ(submissions.findById(id)->status(), S::Submitted);
}

TEST_F(MarkTest, InactiveLecturerCannotMark) {
    const int id = addOnTime();
    auto inactive = makeUser(Role::Lecturer, 1, false);
    EXPECT_THROW(service.mark(*inactive, id, 80.0, "x"), AccessDeniedError);
}

TEST_F(MarkTest, LecturerWhoDoesNotTeachTheCourseIsRefused) {
    const int id = addOnTime();
    auto other = makeUser(Role::Lecturer, 5);
    EXPECT_THROW(service.mark(*other, id, 80.0, "x"), AccessDeniedError);
    EXPECT_EQ(submissions.findById(id)->status(), S::Submitted);
    EXPECT_TRUE(auditRepo.findAll().empty());
}

// ---- Atomic save: the mark and its audit entry are saved together or not at all ----
TEST_F(MarkTest, FailedAuditEntryUndoesTheMark) {
    const int id = addOnTime();
    FailingAuditRepository failingRepo;
    AuditService brokenAudit(failingRepo, [this] { return now; });
    SubmissionService broken(submissions, assignments, courses, penalty, brokenAudit,
                             [this] { return now; }, &runner);

    EXPECT_THROW(broken.mark(*lecturer, id, 80.0, "Good work"), std::runtime_error);

    Submission s = *submissions.findById(id);
    EXPECT_EQ(s.status(), S::Submitted);
    EXPECT_FALSE(s.rawMark().has_value());
    EXPECT_FALSE(s.finalMark().has_value());
    EXPECT_EQ(s.feedback(), "");
}
