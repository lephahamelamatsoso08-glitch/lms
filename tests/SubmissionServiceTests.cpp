#include <gtest/gtest.h>

#include <chrono>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>

#include "AuditService.h"
#include "DatabaseConnection.h"
#include "SqliteAssignmentRepository.h"
#include "SqliteAuditRepository.h"
#include "SqliteCourseRepository.h"
#include "SqliteSubmissionRepository.h"
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

class SubmitTest : public ::testing::Test {
protected:
    SubmitTest()
        : db(":memory:"),
          submissions(db),
          assignments(db),
          courses(db),
          auditRepo(db),
          audit(auditRepo),
          now(kDeadline - hours(10)),
          service(submissions, assignments, courses, penalty, audit, [this] { return now; }) {
        db.runScript(readFile(LMS_SCHEMA_PATH));
        db.runScript(R"sql(
INSERT INTO user_account(id, username, full_name, password_hash, role) VALUES
  (1, 'lec1', 'Lecturer', 'x', 'LECTURER'),
  (2, 'stu1', 'Student One', 'x', 'STUDENT'),
  (3, 'stu2', 'Student Two', 'x', 'STUDENT'),
  (4, 'stu3', 'Student Three', 'x', 'STUDENT');
INSERT INTO student(user_id, student_number) VALUES (2, 'S2'), (3, 'S3'), (4, 'S4');
INSERT INTO department(id, name) VALUES (1, 'Computer Science');
INSERT INTO course(id, code, title, department_id) VALUES (1, 'CS4433', 'Software Engineering', 1);
INSERT INTO assignment(id, course_id, title, deadline, max_mark)
  VALUES (1, 1, 'A1', '2026-11-01T10:00:00Z', 100);
INSERT INTO enrolment(course_id, student_id) VALUES (1, 2), (1, 3);
)sql");
        student1 = makeUser(Role::Student, 2);
        student2 = makeUser(Role::Student, 3);
    }

    DatabaseConnection db;
    SqliteSubmissionRepository submissions;
    SqliteAssignmentRepository assignments;
    SqliteCourseRepository courses;
    SqliteAuditRepository auditRepo;
    AuditService audit;
    TenPercentPerDayPenalty penalty;
    TimePoint now;
    SubmissionService service;
    std::unique_ptr<User> student1;
    std::unique_ptr<User> student2;
};

}  // namespace

// ---- On time (UC-14 main flow) ----
TEST_F(SubmitTest, OnTimeSubmissionIsSubmittedWithVersionOne) {
    SubmitResult r = service.submit(*student1, 1, "work.pdf");
    EXPECT_EQ(r.submission.status(), S::Submitted);
    EXPECT_EQ(r.submission.lateDays(), 0);
    EXPECT_EQ(r.versionNo, 1);
    EXPECT_FALSE(r.isResubmission);
    EXPECT_DOUBLE_EQ(r.highestPossibleMark, 100.0);
    EXPECT_GT(r.submission.id(), 0);
}

TEST_F(SubmitTest, SubmissionAndFileAreStored) {
    SubmitResult r = service.submit(*student1, 1, "work.pdf");
    auto stored = submissions.findByStudentAndAssignment(2, 1);
    ASSERT_TRUE(stored.has_value());
    EXPECT_EQ(stored->id(), r.submission.id());
    auto versions = submissions.findVersions(r.submission.id());
    ASSERT_EQ(versions.size(), 1u);
    EXPECT_EQ(versions[0].filePath, "work.pdf");
    EXPECT_EQ(versions[0].submittedAt, now);
}

TEST_F(SubmitTest, ExactlyAtTheDeadlineIsOnTime) {
    now = kDeadline;
    EXPECT_EQ(service.submit(*student1, 1, "work.pdf").submission.status(), S::Submitted);
}

// ---- Late (A2, A3, rule C4: never rejected) ----
TEST_F(SubmitTest, OneSecondLateIsLateDayOneWithTenPercentOff) {
    now = kDeadline + seconds(1);
    SubmitResult r = service.submit(*student1, 1, "work.pdf");
    EXPECT_EQ(r.submission.status(), S::Late);
    EXPECT_EQ(r.submission.lateDays(), 1);
    EXPECT_DOUBLE_EQ(r.highestPossibleMark, 90.0);
}

TEST_F(SubmitTest, ThreeDaysLateIsAcceptedWithThirtyPercentOff) {
    now = kDeadline + hours(72);
    SubmitResult r = service.submit(*student1, 1, "work.pdf");
    EXPECT_EQ(r.submission.status(), S::Late);
    EXPECT_EQ(r.submission.lateDays(), 3);
    EXPECT_NEAR(r.highestPossibleMark, 70.0, 1e-9);
}

TEST_F(SubmitTest, FourDaysLateIsStillAcceptedButWorthZero) {
    now = kDeadline + hours(72) + seconds(1);
    SubmitResult r = service.submit(*student1, 1, "work.pdf");
    EXPECT_EQ(r.submission.status(), S::Late);
    EXPECT_EQ(r.submission.lateDays(), 4);
    EXPECT_DOUBLE_EQ(r.highestPossibleMark, 0.0);
    EXPECT_TRUE(submissions.findByStudentAndAssignment(2, 1).has_value());
}

// ---- Extensions (rule C2) ----
TEST_F(SubmitTest, ExtensionMakesALateSubmissionOnTimeForThatStudentOnly) {
    assignments.saveExtension(
        assignments.findById(1)->grantExtension(2, kDeadline + hours(48), "Power outage"));
    now = kDeadline + hours(2);
    EXPECT_EQ(service.submit(*student1, 1, "work.pdf").submission.status(), S::Submitted);
    EXPECT_EQ(service.submit(*student2, 1, "work.pdf").submission.status(), S::Late);
}

TEST_F(SubmitTest, LateDaysAreCountedFromTheExtendedDeadline) {
    assignments.saveExtension(
        assignments.findById(1)->grantExtension(2, kDeadline + hours(48), "Power outage"));
    now = kDeadline + hours(50);
    SubmitResult r = service.submit(*student1, 1, "work.pdf");
    EXPECT_EQ(r.submission.lateDays(), 1);
    EXPECT_DOUBLE_EQ(r.highestPossibleMark, 90.0);
}

// ---- Resubmission (A1, rule C5) ----
TEST_F(SubmitTest, ResubmitBeforeTheDeadlineAddsAVersionAndKeepsTheStatus) {
    service.submit(*student1, 1, "v1.pdf");
    now = kDeadline - hours(5);
    SubmitResult r = service.submit(*student1, 1, "v2.pdf");
    EXPECT_TRUE(r.isResubmission);
    EXPECT_EQ(r.versionNo, 2);
    EXPECT_EQ(r.submission.status(), S::Submitted);

    auto versions = submissions.findVersions(r.submission.id());
    ASSERT_EQ(versions.size(), 2u);
    EXPECT_EQ(versions[0].filePath, "v1.pdf");
    EXPECT_EQ(versions[1].filePath, "v2.pdf");
    EXPECT_EQ(versions[1].submittedAt, now);
    EXPECT_EQ(submissions.findByAssignment(1).size(), 1u);
}

TEST_F(SubmitTest, ResubmitAfterTheDeadlineIsRefused) {
    const int id = service.submit(*student1, 1, "v1.pdf").submission.id();
    now = kDeadline + hours(1);
    EXPECT_THROW(service.submit(*student1, 1, "v2.pdf"), SubmissionNotAllowedError);
    EXPECT_EQ(submissions.findVersions(id).size(), 1u);
}

TEST_F(SubmitTest, LateWorkCannotBeResubmitted) {
    now = kDeadline + hours(1);
    const int id = service.submit(*student1, 1, "v1.pdf").submission.id();
    now = kDeadline + hours(2);
    EXPECT_THROW(service.submit(*student1, 1, "v2.pdf"), SubmissionNotAllowedError);
    EXPECT_EQ(submissions.findVersions(id).size(), 1u);
}

TEST_F(SubmitTest, MarkedWorkCannotBeResubmittedEvenBeforeTheDeadline) {
    const int id = service.submit(*student1, 1, "v1.pdf").submission.id();
    Submission s = *submissions.findById(id);
    s.applyMark(80.0, 80.0, "Good work");
    submissions.save(s);
    now = kDeadline - hours(5);
    EXPECT_THROW(service.submit(*student1, 1, "v2.pdf"), SubmissionNotAllowedError);
    EXPECT_EQ(submissions.findVersions(id).size(), 1u);
}

// ---- Who may submit, and what ----
TEST_F(SubmitTest, StudentNotEnrolledInTheCourseIsRefused) {
    auto outsider = makeUser(Role::Student, 4);
    EXPECT_THROW(service.submit(*outsider, 1, "work.pdf"), AccessDeniedError);
    EXPECT_TRUE(submissions.findByAssignment(1).empty());
}

TEST_F(SubmitTest, OnlyStudentsCanSubmit) {
    auto lecturer = makeUser(Role::Lecturer, 1);
    auto hod = makeUser(Role::HoD, 5);
    EXPECT_THROW(service.submit(*lecturer, 1, "work.pdf"), AccessDeniedError);
    EXPECT_THROW(service.submit(*hod, 1, "work.pdf"), AccessDeniedError);
}

TEST_F(SubmitTest, InactiveStudentIsRefused) {
    auto inactive = makeUser(Role::Student, 2, false);
    EXPECT_THROW(service.submit(*inactive, 1, "work.pdf"), AccessDeniedError);
}

TEST_F(SubmitTest, UnknownAssignmentIsRejected) {
    EXPECT_THROW(service.submit(*student1, 99, "work.pdf"), std::invalid_argument);
}

TEST_F(SubmitTest, UnsuitableFilesAreRejectedAndNothingIsSaved) {
    for (const std::string bad : {"", "   ", "report", "report.", "report.exe", "archive.tar.gz"}) {
        SCOPED_TRACE("file: '" + bad + "'");
        EXPECT_THROW(service.submit(*student1, 1, bad), std::invalid_argument);
    }
    EXPECT_TRUE(submissions.findByAssignment(1).empty());
}

TEST_F(SubmitTest, AllowedFileTypesAreAccepted) {
    int studentId = 2;
    for (const std::string file : {"a.pdf", "a.doc", "a.docx", "a.txt", "a.zip", "A.PDF"}) {
        db.runScript("DELETE FROM submission_version; DELETE FROM submission;");
        auto student = makeUser(Role::Student, studentId);
        SCOPED_TRACE("file: " + file);
        EXPECT_NO_THROW(service.submit(*student, 1, file));
    }
}

TEST_F(SubmitTest, EachStudentHasTheirOwnSubmission) {
    service.submit(*student1, 1, "a.pdf");
    service.submit(*student2, 1, "b.pdf");
    EXPECT_EQ(submissions.findByAssignment(1).size(), 2u);
}
