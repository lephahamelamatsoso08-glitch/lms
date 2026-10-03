#include <gtest/gtest.h>

#include <chrono>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>

#include "AuditService.h"
#include "DatabaseConnection.h"
#include "ModerationService.h"
#include "SqliteAssignmentRepository.h"
#include "SqliteAuditRepository.h"
#include "SqliteCourseRepository.h"
#include "SqliteSubmissionRepository.h"
#include "SqliteTransactionRunner.h"
#include "StudentViewService.h"
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

class StudentViewTest : public ::testing::Test {
protected:
    StudentViewTest()
        : db(":memory:"),
          submissions(db),
          assignments(db),
          courses(db),
          auditRepo(db),
          audit(auditRepo, [this] { return now; }),
          runner(db),
          now(kDeadline - hours(10)),
          view(submissions, assignments, courses),
          submitService(submissions, assignments, courses, penalty, audit,
                        [this] { return now; }, &runner),
          moderation(submissions, assignments, courses, audit, runner, [this] { return now; }) {
        db.runScript(readFile(LMS_SCHEMA_PATH));
        db.runScript(R"sql(
INSERT INTO user_account(id, username, full_name, password_hash, role) VALUES
  (1, 'lec1', 'Lecturer', 'x', 'LECTURER'),
  (2, 'stu1', 'Student One', 'x', 'STUDENT'),
  (3, 'stu2', 'Student Two', 'x', 'STUDENT'),
  (4, 'stu3', 'Student Three', 'x', 'STUDENT'),
  (6, 'hod1', 'Hod One', 'x', 'HOD'),
  (8, 'adm1', 'Admin', 'x', 'ADMIN');
INSERT INTO student(user_id, student_number) VALUES (2, 'S2'), (3, 'S3'), (4, 'S4');
INSERT INTO department(id, name, hod_user_id) VALUES (1, 'Computer Science', 6);
INSERT INTO course(id, code, title, department_id) VALUES
  (1, 'CS4433', 'Software Engineering', 1),
  (2, 'CS4000', 'Other Course', 1);
INSERT INTO assignment(id, course_id, title, deadline, max_mark) VALUES
  (1, 1, 'A1', '2026-11-01T10:00:00Z', 100),
  (2, 1, 'A2', '2026-11-08T10:00:00Z', 50),
  (3, 2, 'Other', '2026-11-08T10:00:00Z', 50);
INSERT INTO enrolment(course_id, student_id) VALUES (1, 2), (1, 3);
INSERT INTO course_lecturer(course_id, lecturer_id) VALUES (1, 1);
)sql");
        student = makeUser(Role::Student, 2);
        otherStudent = makeUser(Role::Student, 3);
        outsider = makeUser(Role::Student, 4);
        lecturer = makeUser(Role::Lecturer, 1);
        hod = makeUser(Role::HoD, 6);
    }

    // A submission for assignment 1 that is already in the given status.
    int addSubmission(int studentId, S status, int lateDays = 0) {
        SubmissionVersion v;
        v.filePath = "work.pdf";
        v.submittedAt = kDeadline - hours(1);
        const int id = submissions.add(Submission(0, 1, studentId, S::Submitted, 0), v);
        if (status == S::Submitted) {
            return id;
        }
        Submission s(id, 1, studentId, status == S::Late ? S::Late : S::Submitted,
                     status == S::Late ? lateDays : 0);
        if (status == S::Late) {
            submissions.save(s);
            return id;
        }
        s.applyMark(80.0, 60.0, "Detailed feedback");
        if (status == S::Moderated || status == S::Released) {
            s.transitionTo(S::Moderated);
        }
        if (status == S::Released) {
            s.release(kDeadline + hours(200));
        }
        submissions.save(s);
        return id;
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
    StudentViewService view;
    SubmissionService submitService;
    ModerationService moderation;
    std::unique_ptr<User> student;
    std::unique_ptr<User> otherStudent;
    std::unique_ptr<User> outsider;
    std::unique_ptr<User> lecturer;
    std::unique_ptr<User> hod;
};

}  // namespace

// ---- FR8: courses and assignments ----
TEST_F(StudentViewTest, MyCoursesListsOnlyEnrolledCourses) {
    auto list = view.myCourses(*student);
    ASSERT_EQ(list.size(), 1u);
    EXPECT_EQ(list[0].code(), "CS4433");
    EXPECT_TRUE(view.myCourses(*outsider).empty());
}

TEST_F(StudentViewTest, MyAssignmentsListsTheCourseAssignmentsInDeadlineOrder) {
    auto items = view.myAssignments(*student, 1);
    ASSERT_EQ(items.size(), 2u);
    EXPECT_EQ(items[0].title, "A1");
    EXPECT_EQ(items[0].deadline, kDeadline);
    EXPECT_DOUBLE_EQ(items[0].maxMark, 100.0);
    EXPECT_EQ(items[1].title, "A2");
}

TEST_F(StudentViewTest, MyAssignmentsShowsTheStudentsOwnDeadline) {
    assignments.saveExtension(
        assignments.findById(1)->grantExtension(2, kDeadline + hours(48), "Power outage"));
    EXPECT_EQ(view.myAssignments(*student, 1)[0].deadline, kDeadline + hours(48));
    EXPECT_EQ(view.myAssignments(*otherStudent, 1)[0].deadline, kDeadline);
}

TEST_F(StudentViewTest, AssignmentListShowsNotSubmittedThenTheStatus) {
    EXPECT_EQ(view.myAssignments(*student, 1)[0].statusLabel, "Not submitted");
    addSubmission(2, S::Submitted);
    EXPECT_EQ(view.myAssignments(*student, 1)[0].statusLabel, "Submitted");
    EXPECT_EQ(view.myAssignments(*student, 1)[1].statusLabel, "Not submitted");
}

TEST_F(StudentViewTest, CourseTheStudentIsNotEnrolledInIsRefused) {
    EXPECT_THROW(view.myAssignments(*student, 2), AccessDeniedError);
    EXPECT_THROW(view.myAssignments(*outsider, 1), AccessDeniedError);
}

// ---- FR17 and rule C1 ----
TEST_F(StudentViewTest, NotSubmittedWorkShowsNoMarkData) {
    StudentSubmissionView v = view.mySubmission(*student, 1);
    EXPECT_FALSE(v.submitted);
    EXPECT_EQ(v.statusLabel, "Not submitted");
    EXPECT_EQ(v.versionCount, 0);
    EXPECT_FALSE(v.finalMark.has_value());
    EXPECT_EQ(v.assignmentTitle, "A1");
}

TEST_F(StudentViewTest, SubmittedWorkShowsStatusAndVersionCount) {
    const int id = addSubmission(2, S::Submitted);
    StudentSubmissionView v = view.mySubmission(*student, 1);
    EXPECT_TRUE(v.submitted);
    EXPECT_EQ(v.statusLabel, "Submitted");
    EXPECT_EQ(v.versionCount, 1);

    SubmissionVersion second;
    second.filePath = "v2.pdf";
    second.submittedAt = kDeadline - hours(1);
    submissions.addVersion(id, second);
    EXPECT_EQ(view.mySubmission(*student, 1).versionCount, 2);
}

TEST_F(StudentViewTest, LateWorkShowsTheLateDays) {
    addSubmission(2, S::Late, 2);
    StudentSubmissionView v = view.mySubmission(*student, 1);
    EXPECT_EQ(v.statusLabel, "Submitted late");
    EXPECT_EQ(v.lateDays, 2);
}

TEST_F(StudentViewTest, MarkedWorkHidesTheMarkAndTheFeedback) {
    const int id = addSubmission(2, S::Marked);
    // The mark and feedback really are stored ...
    ASSERT_TRUE(submissions.findById(id)->finalMark().has_value());
    ASSERT_FALSE(submissions.findById(id)->feedback().empty());
    // ... but the student must not see them (rule C1).
    StudentSubmissionView v = view.mySubmission(*student, 1);
    EXPECT_EQ(v.statusLabel, "Marked, awaiting moderation");
    EXPECT_FALSE(v.rawMark.has_value());
    EXPECT_FALSE(v.finalMark.has_value());
    EXPECT_FALSE(v.feedback.has_value());
    EXPECT_FALSE(v.releasedAt.has_value());
}

TEST_F(StudentViewTest, ModeratedWorkStillHidesTheMarkAndTheFeedback) {
    addSubmission(2, S::Moderated);
    StudentSubmissionView v = view.mySubmission(*student, 1);
    EXPECT_EQ(v.statusLabel, "Moderated, awaiting release");
    EXPECT_FALSE(v.rawMark.has_value());
    EXPECT_FALSE(v.finalMark.has_value());
    EXPECT_FALSE(v.feedback.has_value());
}

TEST_F(StudentViewTest, ReleasedWorkShowsMarkPenaltyAndFeedback) {
    addSubmission(2, S::Released);
    StudentSubmissionView v = view.mySubmission(*student, 1);
    EXPECT_EQ(v.statusLabel, "Released");
    ASSERT_TRUE(v.rawMark.has_value());
    ASSERT_TRUE(v.finalMark.has_value());
    ASSERT_TRUE(v.feedback.has_value());
    ASSERT_TRUE(v.releasedAt.has_value());
    EXPECT_DOUBLE_EQ(*v.rawMark, 80.0);
    EXPECT_DOUBLE_EQ(*v.finalMark, 60.0);
    EXPECT_EQ(*v.feedback, "Detailed feedback");
    EXPECT_EQ(*v.releasedAt, kDeadline + hours(200));
}

TEST_F(StudentViewTest, AStudentOnlySeesTheirOwnSubmission) {
    addSubmission(2, S::Released);
    addSubmission(3, S::Submitted);
    EXPECT_EQ(view.mySubmission(*student, 1).statusLabel, "Released");
    StudentSubmissionView other = view.mySubmission(*otherStudent, 1);
    EXPECT_EQ(other.statusLabel, "Submitted");
    EXPECT_FALSE(other.finalMark.has_value());
}

TEST_F(StudentViewTest, SubmissionViewShowsTheOwnDeadline) {
    assignments.saveExtension(
        assignments.findById(1)->grantExtension(2, kDeadline + hours(48), "Power outage"));
    EXPECT_EQ(view.mySubmission(*student, 1).deadline, kDeadline + hours(48));
    EXPECT_EQ(view.mySubmission(*otherStudent, 1).deadline, kDeadline);
}

// ---- Access ----
TEST_F(StudentViewTest, StudentNotEnrolledCannotViewTheSubmission) {
    EXPECT_THROW(view.mySubmission(*outsider, 1), AccessDeniedError);
}

TEST_F(StudentViewTest, OnlyStudentsCanUseTheStudentPages) {
    auto admin = makeUser(Role::Admin, 8);
    for (const User* user : {lecturer.get(), hod.get(), admin.get()}) {
        EXPECT_THROW(view.myCourses(*user), AccessDeniedError);
        EXPECT_THROW(view.myAssignments(*user, 1), AccessDeniedError);
        EXPECT_THROW(view.mySubmission(*user, 1), AccessDeniedError);
    }
}

TEST_F(StudentViewTest, InactiveStudentIsRefused) {
    auto inactive = makeUser(Role::Student, 2, false);
    EXPECT_THROW(view.myCourses(*inactive), AccessDeniedError);
    EXPECT_THROW(view.myAssignments(*inactive, 1), AccessDeniedError);
    EXPECT_THROW(view.mySubmission(*inactive, 1), AccessDeniedError);
}

TEST_F(StudentViewTest, UnknownAssignmentIsRefused) {
    EXPECT_THROW(view.mySubmission(*student, 99), std::invalid_argument);
}

// ---- The whole journey: submit, mark, moderate, release ----
TEST_F(StudentViewTest, WholeJourneyShowsTheMarkOnlyAfterRelease) {
    now = kDeadline + hours(30);  // 2 late days
    const int id = submitService.submit(*student, 1, "work.pdf").submission.id();
    EXPECT_EQ(view.mySubmission(*student, 1).statusLabel, "Submitted late");

    submitService.mark(*lecturer, id, 80.0, "Late but solid work");
    StudentSubmissionView marked = view.mySubmission(*student, 1);
    EXPECT_EQ(marked.statusLabel, "Marked, awaiting moderation");
    EXPECT_FALSE(marked.finalMark.has_value());
    EXPECT_FALSE(marked.feedback.has_value());

    moderation.approve(*hod, id);
    StudentSubmissionView moderated = view.mySubmission(*student, 1);
    EXPECT_EQ(moderated.statusLabel, "Moderated, awaiting release");
    EXPECT_FALSE(moderated.finalMark.has_value());

    moderation.release(*hod, 1);
    StudentSubmissionView released = view.mySubmission(*student, 1);
    EXPECT_EQ(released.statusLabel, "Released");
    ASSERT_TRUE(released.finalMark.has_value());
    EXPECT_NEAR(*released.finalMark, 60.0, 1e-9);  // 80 minus 20% of 100
    EXPECT_DOUBLE_EQ(*released.rawMark, 80.0);
    EXPECT_EQ(*released.feedback, "Late but solid work");
    EXPECT_EQ(released.lateDays, 2);
}
