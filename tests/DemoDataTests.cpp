#include <gtest/gtest.h>

#include <stdexcept>

#include "AppContext.h"
#include "DemoData.h"

using namespace lms::domain;
using lms::app::AppContext;
using lms::app::kDemoPassword;
using lms::app::seedDemoData;

TEST(DemoDataTest, AllFourRolesCanLogIn) {
    AppContext ctx(":memory:");
    seedDemoData(ctx);
    EXPECT_EQ(ctx.auth().login("admin", kDemoPassword)->getRole(), Role::Admin);
    EXPECT_EQ(ctx.auth().login("lecturer1", kDemoPassword)->getRole(), Role::Lecturer);
    EXPECT_EQ(ctx.auth().login("hod1", kDemoPassword)->getRole(), Role::HoD);
    EXPECT_EQ(ctx.auth().login("student1", kDemoPassword)->getRole(), Role::Student);
    EXPECT_EQ(ctx.auth().login("student2", kDemoPassword)->getRole(), Role::Student);
}

TEST(DemoDataTest, StudentsSeeTheirOwnCoursesAndAssignments) {
    AppContext ctx(":memory:");
    seedDemoData(ctx);
    auto student1 = ctx.auth().login("student1", kDemoPassword);
    auto student2 = ctx.auth().login("student2", kDemoPassword);

    const auto courses1 = ctx.studentView().myCourses(*student1);
    ASSERT_EQ(courses1.size(), 2u);
    EXPECT_EQ(courses1[0].code(), "CS3300");
    EXPECT_EQ(courses1[1].code(), "CS4433");
    EXPECT_EQ(ctx.studentView().myAssignments(*student1, courses1[1].id()).size(), 2u);
    EXPECT_EQ(ctx.studentView().myAssignments(*student1, courses1[0].id()).size(), 1u);

    const auto courses2 = ctx.studentView().myCourses(*student2);
    ASSERT_EQ(courses2.size(), 1u);
    EXPECT_EQ(courses2[0].code(), "CS4433");
}

TEST(DemoDataTest, TheLecturerTeachesBothCourses) {
    AppContext ctx(":memory:");
    seedDemoData(ctx);
    auto lecturer = ctx.auth().login("lecturer1", kDemoPassword);
    EXPECT_EQ(ctx.assignments().myCourses(*lecturer).size(), 2u);
}

TEST(DemoDataTest, SeedingAnAlreadyUsedSystemIsRefused) {
    AppContext ctx(":memory:");
    seedDemoData(ctx);
    EXPECT_THROW(seedDemoData(ctx), std::logic_error);

    AppContext other(":memory:");
    other.createFirstAdmin("boss", "The Boss", "Passw0rd1");
    EXPECT_THROW(seedDemoData(other), std::logic_error);
}
