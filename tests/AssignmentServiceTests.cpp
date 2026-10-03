#include <gtest/gtest.h>

#include <chrono>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include "AssignmentService.h"
#include "AuditService.h"
#include "DatabaseConnection.h"
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

const TimePoint kNow = fromIso8601("2026-10-20T08:00:00Z");
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

class LecturerTest : public ::testing::Test {
protected:
    LecturerTest()
        : db(":memory:"),
          assignments(db),
          courses(db),
          submissions(db),
          auditRepo(db),
          now(kNow),
          audit(auditRepo, [this] { return now; }),
          runner(db),
          service(assignments, courses, submissions, [this] { return now; }),
          submitService(submissions, assignments, courses, penalty, audit,
                        [this] { return now; }, &runner) {
        db.runScript(readFile(LMS_SCHEMA_PATH));
        db.runScript(R"sql(
INSERT INTO user_account(id, username, full_name, password_hash, role) VALUES
  (1, 'lec1', 'Lecturer One', 'x', 'LECTURER'),
  (2, 'stu1', 'Student One', 'x', 'STUDENT'),
  (3, 'stu2', 'Student Two', 'x', 'STUDENT'),
  (4, 'stu3', 'Student Three', 'x', 'STUDENT'),
  (5, 'lec2', 'Lecturer Two', 'x', 'LECTURER'),
  (6, 'hod1', 'Hod One', 'x', 'HOD');
INSERT INTO student(user_id, student_number) VALUES (2, 'S2'), (3, 'S3'), (4, 'S4');
INSERT INTO department(id, name, hod_user_id) VALUES (1, 'Computer Science', 6);
INSERT INTO course(id, code, title, department_id) VALUES
  (1, 'CS4433', 'Software Engineering', 1),
  (2, 'CS4000', 'Other Course', 1);
INSERT INTO assignment(id, course_id, title, deadline, max_mark) VALUES
  (1, 1, 'A1', '2026-11-01T10:00:00Z', 100);
INSERT INTO enrolment(course_id, student_id) VALUES (1, 2), (1, 3);
INSERT INTO course_lecturer(course_id, lecturer_id) VALUES (1, 1), (2, 5);
)sql");
        lecturer = makeUser(Role::Lecturer, 1);
        otherLecturer = makeUser(Role::Lecturer, 5);
        student1 = makeUser(Role::Student, 2);
        student2 = makeUser(Role::Student, 3);
    }

    int addSubmission(int studentId, S status, int lateDays = 0) {
        SubmissionVersion v;
        v.filePath = "work.pdf";
        v.submittedAt = kDeadline - hours(1);
        return submissions.add(Submission(0, 1, studentId, status, lateDays), v);
    }

    DatabaseConnection db;
    SqliteAssignmentRepository assignments;
    SqliteCourseRepository courses;
    SqliteSubmissionRepository submissions;
    SqliteAuditRepository auditRepo;
    TimePoint now;
    AuditService audit;
    SqliteTransactionRunner runner;
    TenPercentPerDayPenalty penalty;
    AssignmentService service;
    SubmissionService submitService;
    std::unique_ptr<User> lecturer;
    std::unique_ptr<User> otherLecturer;
    std::unique_ptr<User> student1;
    std::unique_ptr<User> student2;
};

}  // namespace

// ---- FR6: create assignments ----
TEST_F(LecturerTest, CreateAssignmentStoresAllTheData) {
    const int id = service.createAssignment(*lecturer, 1, "Assignment 2", kDeadline + hours(24), 50.0);
    auto found = assignments.findById(id);
    ASSERT_TRUE(found.has_value());
    EXPECT_EQ(found->courseId(), 1);
    EXPECT_EQ(found->title(), "Assignment 2");
    EXPECT_EQ(found->deadline(), kDeadline + hours(24));
    EXPECT_DOUBLE_EQ(found->maxMark(), 50.0);
}

TEST_F(LecturerTest, DeadlineMustBeInTheFuture) {
    EXPECT_THROW(service.createAssignment(*lecturer, 1, "A", kNow, 100.0), std::invalid_argument);
    EXPECT_THROW(service.createAssignment(*lecturer, 1, "A", kNow - hours(1), 100.0),
                 std::invalid_argument);
    EXPECT_NO_THROW(service.createAssignment(*lecturer, 1, "A", kNow + seconds(1), 100.0));
}

TEST_F(LecturerTest, BadTitleOrMaximumMarkIsRefusedAndNothingIsSaved) {
    EXPECT_THROW(service.createAssignment(*lecturer, 1, "", kDeadline, 100.0), std::invalid_argument);
    EXPECT_THROW(service.createAssignment(*lecturer, 1, "A", kDeadline, 0.0), std::invalid_argument);
    EXPECT_THROW(service.createAssignment(*lecturer, 1, "A", kDeadline, -5.0), std::invalid_argument);
    EXPECT_EQ(assignments.findByCourse(1).size(), 1u);  // only the one from the set-up
}

TEST_F(LecturerTest, OnlyALecturerWhoTeachesTheCourseCanCreateAssignments) {
    EXPECT_THROW(service.createAssignment(*otherLecturer, 1, "A", kDeadline, 100.0), AccessDeniedError);
    auto hod = makeUser(Role::HoD, 6);
    auto admin = makeUser(Role::Admin, 8);
    EXPECT_THROW(service.createAssignment(*student1, 1, "A", kDeadline, 100.0), AccessDeniedError);
    EXPECT_THROW(service.createAssignment(*hod, 1, "A", kDeadline, 100.0), AccessDeniedError);
    EXPECT_THROW(service.createAssignment(*admin, 1, "A", kDeadline, 100.0), AccessDeniedError);
    auto inactive = makeUser(Role::Lecturer, 1, false);
    EXPECT_THROW(service.createAssignment(*inactive, 1, "A", kDeadline, 100.0), AccessDeniedError);
    EXPECT_EQ(assignments.findByCourse(1).size(), 1u);
}

TEST_F(LecturerTest, UnknownCourseIsRefused) {
    EXPECT_THROW(service.createAssignment(*lecturer, 99, "A", kDeadline, 100.0), AccessDeniedError);
}

// ---- FR7: extensions ----
TEST_F(LecturerTest, ExtensionMovesOnlyThatStudentsDeadline) {
    service.grantExtension(*lecturer, 1, 2, kDeadline + hours(48), "Power outage");
    EXPECT_EQ(assignments.getDeadlineFor(2, 1), kDeadline + hours(48));
    EXPECT_EQ(assignments.getDeadlineFor(3, 1), kDeadline);
    EXPECT_EQ(assignments.findExtension(1, 2)->reason(), "Power outage");
}

TEST_F(LecturerTest, ExtensionMustBeLaterThanTheDeadlineAndNeedsAReason) {
    EXPECT_THROW(service.grantExtension(*lecturer, 1, 2, kDeadline, "Reason"), std::invalid_argument);
    EXPECT_THROW(service.grantExtension(*lecturer, 1, 2, kDeadline - hours(1), "Reason"),
                 std::invalid_argument);
    EXPECT_THROW(service.grantExtension(*lecturer, 1, 2, kDeadline + hours(24), ""),
                 std::invalid_argument);
    EXPECT_THROW(service.grantExtension(*lecturer, 1, 2, kDeadline + hours(24), "   "),
                 std::invalid_argument);
    EXPECT_FALSE(assignments.findExtension(1, 2).has_value());
}

TEST_F(LecturerTest, SecondExtensionReplacesTheFirst) {
    service.grantExtension(*lecturer, 1, 2, kDeadline + hours(24), "First");
    service.grantExtension(*lecturer, 1, 2, kDeadline + hours(72), "Second");
    EXPECT_EQ(assignments.getDeadlineFor(2, 1), kDeadline + hours(72));
}

TEST_F(LecturerTest, StudentMustBeEnrolledInTheCourse) {
    EXPECT_THROW(service.grantExtension(*lecturer, 1, 4, kDeadline + hours(24), "Reason"),
                 std::invalid_argument);
    EXPECT_FALSE(assignments.findExtension(1, 4).has_value());
}

TEST_F(LecturerTest, UnknownAssignmentIsRefused) {
    EXPECT_THROW(service.grantExtension(*lecturer, 99, 2, kDeadline + hours(24), "Reason"),
                 std::invalid_argument);
}

TEST_F(LecturerTest, OnlyALecturerWhoTeachesTheCourseCanGrantExtensions) {
    EXPECT_THROW(service.grantExtension(*otherLecturer, 1, 2, kDeadline + hours(24), "Reason"),
                 AccessDeniedError);
    EXPECT_THROW(service.grantExtension(*student1, 1, 2, kDeadline + hours(24), "Reason"),
                 AccessDeniedError);
    EXPECT_FALSE(assignments.findExtension(1, 2).has_value());
}

TEST_F(LecturerTest, ExtensionIsAllowedAfterAnOnTimeSubmission) {
    addSubmission(2, S::Submitted);
    EXPECT_NO_THROW(service.grantExtension(*lecturer, 1, 2, kDeadline + hours(24), "Reason"));
}

TEST_F(LecturerTest, ExtensionIsRefusedAfterLateOrMarkedWork) {
    addSubmission(2, S::Late, 1);
    EXPECT_THROW(service.grantExtension(*lecturer, 1, 2, kDeadline + hours(24), "Reason"),
                 std::logic_error);

    const int id = addSubmission(3, S::Submitted);
    Submission s = *submissions.findById(id);
    s.applyMark(70.0, 70.0, "Fine");
    submissions.save(s);
    EXPECT_THROW(service.grantExtension(*lecturer, 1, 3, kDeadline + hours(24), "Reason"),
                 std::logic_error);
    EXPECT_FALSE(assignments.findExtension(1, 2).has_value());
}

// ---- Lists ----
TEST_F(LecturerTest, MyCoursesListsOnlyTheirCourses) {
    auto list = service.myCourses(*lecturer);
    ASSERT_EQ(list.size(), 1u);
    EXPECT_EQ(list[0].code(), "CS4433");
    EXPECT_THROW(service.myCourses(*student1), AccessDeniedError);
}

TEST_F(LecturerTest, ListAssignmentsOnlyForACourseTheyTeach) {
    service.createAssignment(*lecturer, 1, "A2", kDeadline + hours(24), 50.0);
    auto list = service.listAssignments(*lecturer, 1);
    ASSERT_EQ(list.size(), 2u);
    EXPECT_EQ(list[0].title(), "A1");
    EXPECT_EQ(list[1].title(), "A2");
    EXPECT_THROW(service.listAssignments(*lecturer, 2), AccessDeniedError);
}

TEST_F(LecturerTest, ListSubmissionsShowsTheMarksToTheLecturer) {
    addSubmission(2, S::Submitted);
    const int marked = addSubmission(3, S::Submitted);
    Submission s = *submissions.findById(marked);
    s.applyMark(80.0, 80.0, "Good work");
    submissions.save(s);

    auto list = service.listSubmissions(*lecturer, 1);
    ASSERT_EQ(list.size(), 2u);
    EXPECT_FALSE(list[0].finalMark().has_value());
    ASSERT_TRUE(list[1].finalMark().has_value());
    EXPECT_DOUBLE_EQ(*list[1].finalMark(), 80.0);
    EXPECT_EQ(list[1].feedback(), "Good work");
}

TEST_F(LecturerTest, ListSubmissionsOnlyForACourseTheyTeach) {
    addSubmission(2, S::Submitted);
    EXPECT_THROW(service.listSubmissions(*otherLecturer, 1), AccessDeniedError);
    EXPECT_THROW(service.listSubmissions(*student1, 1), AccessDeniedError);
    EXPECT_THROW(service.listSubmissions(*lecturer, 99), std::invalid_argument);
}

// ---- The two services working together ----
TEST_F(LecturerTest, ExtensionDecidesWhoIsOnTimeWhenBothSubmitTwoHoursLate) {
    const int id = service.createAssignment(*lecturer, 1, "A3", kNow + hours(24), 100.0);
    service.grantExtension(*lecturer, id, 2, kNow + hours(24 + 72), "Power outage");

    now = kNow + hours(26);  // two hours after the normal deadline
    EXPECT_EQ(submitService.submit(*student1, id, "a.pdf").submission.status(), S::Submitted);
    EXPECT_EQ(submitService.submit(*student2, id, "b.pdf").submission.status(), S::Late);
}
