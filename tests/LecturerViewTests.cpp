#include <gtest/gtest.h>

#include <chrono>
#include <stdexcept>
#include <string>

#include "AppContext.h"
#include "UserFactory.h"

using namespace lms::domain;
using namespace lms::application;
using lms::app::AppContext;

namespace {

const char* const kPassword = "Passw0rd1";

class LecturerViewTest : public ::testing::Test {
protected:
    LecturerViewTest() : ctx(":memory:") {
        ctx.createFirstAdmin("admin", "System Admin", kPassword);
        admin = ctx.auth().login("admin", kPassword);
        const int dept = ctx.admin().createDepartment(*admin, "Computer Science");
        const int course1 = ctx.admin().createCourse(*admin, "CS4433", "Software Engineering", dept);
        const int course2 = ctx.admin().createCourse(*admin, "CS4000", "Other Course", dept);
        const int lec1 = addUser(Role::Lecturer, "lec1", "Lecturer One");
        const int lec2 = addUser(Role::Lecturer, "lec2", "Lecturer Two");
        const int stu1 = addUser(Role::Student, "stu1", "Student One");
        const int stu2 = addUser(Role::Student, "stu2", "Student Two");
        const int stu3 = addUser(Role::Student, "stu3", "Student Three");
        ctx.admin().assignLecturer(*admin, course1, lec1);
        ctx.admin().assignLecturer(*admin, course2, lec2);
        ctx.admin().enrolStudent(*admin, course1, stu1);
        ctx.admin().enrolStudent(*admin, course1, stu2);
        ctx.admin().enrolStudent(*admin, course2, stu3);

        lecturer = ctx.auth().login("lec1", kPassword);
        otherLecturer = ctx.auth().login("lec2", kPassword);
        student1 = ctx.auth().login("stu1", kPassword);
        student2 = ctx.auth().login("stu2", kPassword);
        assignment = ctx.assignments().createAssignment(
            *lecturer, course1, "Assignment 1",
            std::chrono::system_clock::now() + std::chrono::hours(48), 100.0);
    }

    int addUser(Role role, const std::string& username, const std::string& name) {
        UserData d;
        d.username = username;
        d.fullName = name;
        d.departmentId = 1;
        d.studentNumber = "S-" + username;
        return ctx.admin().createUser(*admin, role, d, kPassword);
    }

    AppContext ctx;
    std::unique_ptr<User> admin;
    std::unique_ptr<User> lecturer;
    std::unique_ptr<User> otherLecturer;
    std::unique_ptr<User> student1;
    std::unique_ptr<User> student2;
    int assignment = 0;
};

}  // namespace

TEST_F(LecturerViewTest, ListsEveryEnrolledStudentOrderedByStudentNumber) {
    const auto rows = ctx.lecturerView().rows(*lecturer, assignment);
    ASSERT_EQ(rows.size(), 2u);  // the student of the other course is not listed
    EXPECT_EQ(rows[0].studentName, "Student One");
    EXPECT_EQ(rows[0].studentNumber, "S-stu1");
    EXPECT_EQ(rows[1].studentName, "Student Two");
    EXPECT_EQ(rows[1].studentNumber, "S-stu2");
}

TEST_F(LecturerViewTest, StudentsWhoHaveNotSubmittedHaveNoSubmissionData) {
    const auto rows = ctx.lecturerView().rows(*lecturer, assignment);
    for (const auto& row : rows) {
        EXPECT_FALSE(row.submitted);
        EXPECT_EQ(row.submissionId, 0);
        EXPECT_FALSE(row.status.has_value());
        EXPECT_EQ(row.versionCount, 0);
        EXPECT_TRUE(row.latestFile.empty());
        EXPECT_FALSE(row.finalMark.has_value());
    }
}

TEST_F(LecturerViewTest, SubmittedWorkShowsStatusVersionsAndTheLatestFile) {
    ctx.submissions().submit(*student1, assignment, "v1.pdf");
    ctx.submissions().submit(*student1, assignment, "v2.pdf");
    const auto rows = ctx.lecturerView().rows(*lecturer, assignment);
    ASSERT_EQ(rows.size(), 2u);
    EXPECT_TRUE(rows[0].submitted);
    EXPECT_GT(rows[0].submissionId, 0);
    ASSERT_TRUE(rows[0].status.has_value());
    EXPECT_EQ(*rows[0].status, SubmissionStatus::Submitted);
    EXPECT_EQ(rows[0].versionCount, 2);
    EXPECT_EQ(rows[0].latestFile, "v2.pdf");
    EXPECT_FALSE(rows[1].submitted);
}

TEST_F(LecturerViewTest, TheLecturerSeesTheMarkAndFeedbackBeforeRelease) {
    const int id = ctx.submissions().submit(*student1, assignment, "work.pdf").submission.id();
    ctx.submissions().mark(*lecturer, id, 80.0, "Good work");
    const auto rows = ctx.lecturerView().rows(*lecturer, assignment);
    ASSERT_TRUE(rows[0].status.has_value());
    EXPECT_EQ(*rows[0].status, SubmissionStatus::Marked);
    ASSERT_TRUE(rows[0].rawMark.has_value());
    EXPECT_DOUBLE_EQ(*rows[0].rawMark, 80.0);
    ASSERT_TRUE(rows[0].finalMark.has_value());
    EXPECT_DOUBLE_EQ(*rows[0].finalMark, 80.0);
    EXPECT_EQ(rows[0].feedback, "Good work");
    // The student cannot see it yet (rule C1).
    EXPECT_FALSE(ctx.studentView().mySubmission(*student1, assignment).finalMark.has_value());
}

TEST_F(LecturerViewTest, OnlyTheLecturerWhoTeachesTheCourseMayLook) {
    EXPECT_THROW(ctx.lecturerView().rows(*otherLecturer, assignment), AccessDeniedError);
    EXPECT_THROW(ctx.lecturerView().rows(*student1, assignment), AccessDeniedError);
    EXPECT_THROW(ctx.lecturerView().rows(*admin, assignment), AccessDeniedError);

    UserData d;
    d.id = lecturer->id();
    d.username = "lec1";
    d.fullName = "Lecturer One";
    d.active = false;
    auto inactive = UserFactory::create(Role::Lecturer, d);
    EXPECT_THROW(ctx.lecturerView().rows(*inactive, assignment), AccessDeniedError);
}

TEST_F(LecturerViewTest, UnknownAssignmentIsRefused) {
    EXPECT_THROW(ctx.lecturerView().rows(*lecturer, 99), std::invalid_argument);
}
