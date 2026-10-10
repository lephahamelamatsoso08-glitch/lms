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

class HodViewTest : public ::testing::Test {
protected:
    HodViewTest() : ctx(":memory:") {
        ctx.createFirstAdmin("admin", "System Admin", kPassword);
        admin = ctx.auth().login("admin", kPassword);
        dept1 = ctx.admin().createDepartment(*admin, "Computer Science");
        dept2 = ctx.admin().createDepartment(*admin, "Mathematics");
        course1 = ctx.admin().createCourse(*admin, "CS4433", "Software Engineering", dept1);
        course2 = ctx.admin().createCourse(*admin, "MA1000", "Calculus", dept2);
        const int lec1 = addUser(Role::Lecturer, "lec1", "Lecturer One", dept1);
        const int lec2 = addUser(Role::Lecturer, "lec2", "Lecturer Two", dept2);
        addUser(Role::HoD, "hod1", "Head One", dept1);
        addUser(Role::HoD, "hod2", "Head Two", dept2);
        const int stu1 = addUser(Role::Student, "stu1", "Student One", dept1);
        const int stu2 = addUser(Role::Student, "stu2", "Student Two", dept1);
        const int stu3 = addUser(Role::Student, "stu3", "Student Three", dept2);
        ctx.admin().assignLecturer(*admin, course1, lec1);
        ctx.admin().assignLecturer(*admin, course2, lec2);
        ctx.admin().enrolStudent(*admin, course1, stu1);
        ctx.admin().enrolStudent(*admin, course1, stu2);
        ctx.admin().enrolStudent(*admin, course2, stu3);

        lecturer = ctx.auth().login("lec1", kPassword);
        otherLecturer = ctx.auth().login("lec2", kPassword);
        hod = ctx.auth().login("hod1", kPassword);
        otherHod = ctx.auth().login("hod2", kPassword);
        student1 = ctx.auth().login("stu1", kPassword);
        student2 = ctx.auth().login("stu2", kPassword);
        student3 = ctx.auth().login("stu3", kPassword);

        const auto deadline = std::chrono::system_clock::now() + std::chrono::hours(48);
        assignment1 = ctx.assignments().createAssignment(*lecturer, course1, "Assignment 1", deadline, 100.0);
        assignment2 = ctx.assignments().createAssignment(*otherLecturer, course2, "Calculus 1", deadline, 50.0);
    }

    int addUser(Role role, const std::string& username, const std::string& name, int dept) {
        UserData d;
        d.username = username;
        d.fullName = name;
        d.departmentId = dept;
        d.studentNumber = "S-" + username;
        return ctx.admin().createUser(*admin, role, d, kPassword);
    }

    // Student 1 submits and the lecturer marks it. Returns the submission id.
    int submitAndMark(User& student, double raw, const std::string& feedback) {
        const int id = ctx.submissions().submit(student, assignment1, "work.pdf").submission.id();
        ctx.submissions().mark(*lecturer, id, raw, feedback);
        return id;
    }

    AppContext ctx;
    std::unique_ptr<User> admin, lecturer, otherLecturer, hod, otherHod, student1, student2, student3;
    int dept1 = 0, dept2 = 0, course1 = 0, course2 = 0, assignment1 = 0, assignment2 = 0;
};

}  // namespace

TEST_F(HodViewTest, CoursesAreOnlyThoseOfTheHoDsDepartment) {
    const auto mine = ctx.hodView().courses(*hod);
    ASSERT_EQ(mine.size(), 1u);
    EXPECT_EQ(mine[0].code(), "CS4433");
    const auto theirs = ctx.hodView().courses(*otherHod);
    ASSERT_EQ(theirs.size(), 1u);
    EXPECT_EQ(theirs[0].code(), "MA1000");
}

TEST_F(HodViewTest, AssignmentsOfAnOwnCourseButNotOfAnotherDepartment) {
    ASSERT_EQ(ctx.hodView().assignments(*hod, course1).size(), 1u);
    EXPECT_THROW(ctx.hodView().assignments(*hod, course2), AccessDeniedError);
    EXPECT_THROW(ctx.hodView().assignments(*hod, 99), AccessDeniedError);
}

TEST_F(HodViewTest, RowsShowOnlyWorkThatHasAMark) {
    ctx.submissions().submit(*student2, assignment1, "work.pdf");  // submitted, not marked
    EXPECT_TRUE(ctx.hodView().rows(*hod, assignment1).empty());

    const int id = submitAndMark(*student1, 80.0, "Good work");
    const auto rows = ctx.hodView().rows(*hod, assignment1);
    ASSERT_EQ(rows.size(), 1u);
    EXPECT_EQ(rows[0].submissionId, id);
    EXPECT_EQ(rows[0].studentName, "Student One");
    EXPECT_EQ(rows[0].studentNumber, "S-stu1");
    EXPECT_EQ(rows[0].status, SubmissionStatus::Marked);
    EXPECT_DOUBLE_EQ(*rows[0].rawMark, 80.0);
    EXPECT_DOUBLE_EQ(*rows[0].finalMark, 80.0);
    EXPECT_EQ(rows[0].feedback, "Good work");
}

TEST_F(HodViewTest, RowsAreOrderedByStudentNumberAndKeepTheirStatusAfterModerationAndRelease) {
    const int second = submitAndMark(*student2, 70.0, "Fine");
    const int first = submitAndMark(*student1, 80.0, "Good work");
    ctx.moderation().approve(*hod, first);
    ctx.moderation().approve(*hod, second);
    ctx.moderation().release(*hod, assignment1);
    const auto rows = ctx.hodView().rows(*hod, assignment1);
    ASSERT_EQ(rows.size(), 2u);
    EXPECT_EQ(rows[0].studentNumber, "S-stu1");
    EXPECT_EQ(rows[1].studentNumber, "S-stu2");
    EXPECT_EQ(rows[0].status, SubmissionStatus::Released);
    EXPECT_EQ(rows[1].status, SubmissionStatus::Released);
}

TEST_F(HodViewTest, RowsAreRefusedForOtherDepartmentsAndOtherRoles) {
    submitAndMark(*student1, 80.0, "Good work");
    EXPECT_THROW(ctx.hodView().rows(*otherHod, assignment1), AccessDeniedError);
    EXPECT_THROW(ctx.hodView().rows(*lecturer, assignment1), AccessDeniedError);
    EXPECT_THROW(ctx.hodView().rows(*student1, assignment1), AccessDeniedError);
    EXPECT_THROW(ctx.hodView().rows(*admin, assignment1), AccessDeniedError);
    UserData d;
    d.id = hod->id();
    d.username = "hod1";
    d.fullName = "Head One";
    d.departmentId = dept1;
    d.active = false;
    auto inactive = UserFactory::create(Role::HoD, d);
    EXPECT_THROW(ctx.hodView().rows(*inactive, assignment1), AccessDeniedError);
}

TEST_F(HodViewTest, UnknownAssignmentOrSubmissionIsRefused) {
    EXPECT_THROW(ctx.hodView().rows(*hod, 99), std::invalid_argument);
    EXPECT_THROW(ctx.hodView().history(*hod, 99), std::invalid_argument);
}

TEST_F(HodViewTest, HistoryShowsWhoChangedTheMarkAndWhy) {
    const int id = submitAndMark(*student1, 80.0, "Good work");
    ctx.moderation().changeMark(*hod, id, 75.0, "Marking was generous");
    const auto history = ctx.hodView().history(*hod, id);
    ASSERT_EQ(history.size(), 2u);
    EXPECT_EQ(history[0].changedBy, "Lecturer One");
    EXPECT_FALSE(history[0].oldValue.has_value());
    EXPECT_DOUBLE_EQ(history[0].newValue, 80.0);
    EXPECT_EQ(history[0].reason, "Initial mark");
    EXPECT_EQ(history[1].changedBy, "Head One");
    EXPECT_DOUBLE_EQ(*history[1].oldValue, 80.0);
    EXPECT_DOUBLE_EQ(history[1].newValue, 75.0);
    EXPECT_EQ(history[1].reason, "Marking was generous");
}

TEST_F(HodViewTest, HistoryIsRefusedForOtherDepartmentsAndOtherRoles) {
    const int id = submitAndMark(*student1, 80.0, "Good work");
    EXPECT_THROW(ctx.hodView().history(*otherHod, id), AccessDeniedError);
    EXPECT_THROW(ctx.hodView().history(*lecturer, id), AccessDeniedError);
    EXPECT_THROW(ctx.hodView().history(*student1, id), AccessDeniedError);
}
