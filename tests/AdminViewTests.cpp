#include <gtest/gtest.h>

#include "AppContext.h"
#include "DemoData.h"

using namespace lms::domain;
using namespace lms::application;
using lms::app::AppContext;
using lms::app::kDemoPassword;
using lms::app::seedDemoData;

namespace {

class AdminViewTest : public ::testing::Test {
protected:
    AdminViewTest() : ctx(":memory:") {
        seedDemoData(ctx);
        admin = ctx.auth().login("admin", kDemoPassword);
    }
    AppContext ctx;
    std::unique_ptr<User> admin;
};

}  // namespace

TEST_F(AdminViewTest, UsersAreListedByUsernameWithTheirDetails) {
    const auto rows = ctx.adminView().users(*admin);
    ASSERT_EQ(rows.size(), 5u);
    EXPECT_EQ(rows[0].username, "admin");
    EXPECT_EQ(rows[0].role, Role::Admin);
    EXPECT_EQ(rows[1].username, "hod1");
    EXPECT_EQ(rows[1].role, Role::HoD);
    EXPECT_EQ(rows[1].detail, "Computer Science");
    EXPECT_EQ(rows[2].username, "lecturer1");
    EXPECT_EQ(rows[2].role, Role::Lecturer);
    EXPECT_EQ(rows[3].username, "student1");
    EXPECT_EQ(rows[3].fullName, "Student One");
    EXPECT_EQ(rows[3].detail, "S001");
    EXPECT_TRUE(rows[3].active);
    EXPECT_EQ(rows[4].username, "student2");
}

TEST_F(AdminViewTest, DeactivatedUsersAreShownAsInactive) {
    const int id = ctx.auth().login("student1", kDemoPassword)->id();
    ctx.admin().deactivateUser(*admin, id);
    const auto rows = ctx.adminView().users(*admin);
    EXPECT_FALSE(rows[3].active);
    EXPECT_TRUE(rows[4].active);
}

TEST_F(AdminViewTest, DepartmentsAreListed) {
    const auto departments = ctx.adminView().departments(*admin);
    ASSERT_EQ(departments.size(), 1u);
    EXPECT_EQ(departments[0].name, "Computer Science");
}

TEST_F(AdminViewTest, CoursesShowDepartmentLecturersAndStudentCount) {
    const auto courses = ctx.adminView().courses(*admin);
    ASSERT_EQ(courses.size(), 2u);
    EXPECT_EQ(courses[0].code, "CS3300");
    EXPECT_EQ(courses[0].department, "Computer Science");
    ASSERT_EQ(courses[0].lecturers.size(), 1u);
    EXPECT_EQ(courses[0].lecturers[0].name, "Lecturer One");
    EXPECT_EQ(courses[0].studentCount, 1);
    EXPECT_EQ(courses[1].code, "CS4433");
    EXPECT_EQ(courses[1].studentCount, 2);
}

TEST_F(AdminViewTest, AuditLogListsEveryMarkChangeWithNames) {
    auto student = ctx.auth().login("student1", kDemoPassword);
    auto lecturer = ctx.auth().login("lecturer1", kDemoPassword);
    auto hod = ctx.auth().login("hod1", kDemoPassword);
    EXPECT_TRUE(ctx.adminView().auditLog(*admin).empty());

    const auto courses = ctx.studentView().myCourses(*student);  // CS3300, CS4433
    const int assignment = ctx.studentView().myAssignments(*student, courses[1].id())[0].assignmentId;
    const int id = ctx.submissions().submit(*student, assignment, "work.pdf").submission.id();
    ctx.submissions().mark(*lecturer, id, 80.0, "Good work");
    ctx.moderation().changeMark(*hod, id, 75.0, "Marking was generous");

    const auto log = ctx.adminView().auditLog(*admin);
    ASSERT_EQ(log.size(), 2u);
    EXPECT_EQ(log[0].changedBy, "Lecturer One");
    EXPECT_EQ(log[0].submissionId, id);
    EXPECT_FALSE(log[0].oldValue.has_value());
    EXPECT_DOUBLE_EQ(log[0].newValue, 80.0);
    EXPECT_EQ(log[1].changedBy, "Head One");
    EXPECT_DOUBLE_EQ(*log[1].oldValue, 80.0);
    EXPECT_DOUBLE_EQ(log[1].newValue, 75.0);
    EXPECT_EQ(log[1].reason, "Marking was generous");
}

TEST_F(AdminViewTest, OnlyAnActiveAdministratorMaySeeTheLists) {
    auto lecturer = ctx.auth().login("lecturer1", kDemoPassword);
    auto hod = ctx.auth().login("hod1", kDemoPassword);
    auto student = ctx.auth().login("student1", kDemoPassword);
    for (const User* user : {lecturer.get(), hod.get(), student.get()}) {
        EXPECT_THROW(ctx.adminView().users(*user), AccessDeniedError);
        EXPECT_THROW(ctx.adminView().departments(*user), AccessDeniedError);
        EXPECT_THROW(ctx.adminView().courses(*user), AccessDeniedError);
        EXPECT_THROW(ctx.adminView().auditLog(*user), AccessDeniedError);
    }
    UserData d;
    d.id = admin->id();
    d.username = "admin";
    d.fullName = "System Admin";
    d.active = false;
    auto inactive = UserFactory::create(Role::Admin, d);
    EXPECT_THROW(ctx.adminView().users(*inactive), AccessDeniedError);
}
