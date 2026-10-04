#include <gtest/gtest.h>

#include <chrono>
#include <cstdio>
#include <filesystem>
#include <stdexcept>
#include <string>

#include "AppContext.h"
#include "DatabaseError.h"
#include "UserFactory.h"

using namespace lms::domain;
using lms::app::AppContext;
using lms::data::DatabaseError;
namespace fs = std::filesystem;

namespace {

UserData person(const std::string& username, const std::string& name) {
    UserData d;
    d.username = username;
    d.fullName = name;
    return d;
}

// A temporary database file that is removed when the test ends.
class TempDbFile {
public:
    TempDbFile() {
        const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
        path_ = (fs::temp_directory_path() / ("lms_test_" + std::to_string(stamp) + ".db")).string();
    }
    ~TempDbFile() {
        std::error_code ignored;
        fs::remove(path_, ignored);
        fs::remove(path_ + "-journal", ignored);
    }
    const std::string& path() const { return path_; }

private:
    std::string path_;
};

}  // namespace

// ---- First run ----
TEST(AppContextTest, NewDatabaseHasTablesAndNeedsAFirstAdmin) {
    AppContext ctx(":memory:");
    EXPECT_TRUE(ctx.needsFirstAdmin());
}

TEST(AppContextTest, FirstAdminCanLogIn) {
    AppContext ctx(":memory:");
    const int id = ctx.createFirstAdmin("admin", "System Admin", "Passw0rd1");
    EXPECT_GT(id, 0);
    EXPECT_FALSE(ctx.needsFirstAdmin());
    auto user = ctx.auth().login("admin", "Passw0rd1");
    ASSERT_NE(user, nullptr);
    EXPECT_EQ(user->getRole(), Role::Admin);
    EXPECT_EQ(user->id(), id);
}

TEST(AppContextTest, SecondFirstAdminIsRefused) {
    AppContext ctx(":memory:");
    ctx.createFirstAdmin("admin", "System Admin", "Passw0rd1");
    EXPECT_THROW(ctx.createFirstAdmin("other", "Other Admin", "Passw0rd1"), std::logic_error);
}

TEST(AppContextTest, WeakPasswordOrBadNameIsRefusedAndTheSystemStillNeedsAnAdmin) {
    AppContext ctx(":memory:");
    EXPECT_THROW(ctx.createFirstAdmin("admin", "System Admin", "weak"), std::invalid_argument);
    EXPECT_THROW(ctx.createFirstAdmin("", "System Admin", "Passw0rd1"), std::invalid_argument);
    EXPECT_THROW(ctx.createFirstAdmin("admin", "", "Passw0rd1"), std::invalid_argument);
    EXPECT_TRUE(ctx.needsFirstAdmin());
}

// ---- Persistence ----
TEST(AppContextTest, DataSurvivesClosingAndReopeningTheDatabaseFile) {
    TempDbFile file;
    {
        AppContext first(file.path());
        EXPECT_TRUE(first.needsFirstAdmin());
        first.createFirstAdmin("admin", "System Admin", "Passw0rd1");
    }
    AppContext second(file.path());  // must not create the tables again
    EXPECT_FALSE(second.needsFirstAdmin());
    EXPECT_NE(second.auth().login("admin", "Passw0rd1"), nullptr);
}

TEST(AppContextTest, CannotOpenADatabaseInAMissingFolder) {
    EXPECT_THROW(AppContext bad("/no/such/folder/lms.db"), DatabaseError);
}

// ---- Everything is connected: a whole course, using only the context ----
TEST(AppContextTest, WholeJourneyFromFirstAdminToReleasedMark) {
    AppContext ctx(":memory:");
    ctx.createFirstAdmin("admin", "System Admin", "Passw0rd1");
    auto admin = ctx.auth().login("admin", "Passw0rd1");
    ASSERT_NE(admin, nullptr);

    const int dept = ctx.admin().createDepartment(*admin, "Computer Science");
    const int course = ctx.admin().createCourse(*admin, "CS4433", "Software Engineering", dept);

    const int lecturerId =
        ctx.admin().createUser(*admin, Role::Lecturer, person("lec1", "Lecturer One"), "Passw0rd1");
    UserData hodData = person("hod1", "Head One");
    hodData.departmentId = dept;
    ctx.admin().createUser(*admin, Role::HoD, hodData, "Passw0rd1");
    UserData studentData = person("stu1", "Student One");
    studentData.studentNumber = "S001";
    const int studentId = ctx.admin().createUser(*admin, Role::Student, studentData, "Passw0rd1");

    ctx.admin().assignLecturer(*admin, course, lecturerId);
    ctx.admin().enrolStudent(*admin, course, studentId);

    auto lecturer = ctx.auth().login("lec1", "Passw0rd1");
    auto hod = ctx.auth().login("hod1", "Passw0rd1");
    auto student = ctx.auth().login("stu1", "Passw0rd1");
    ASSERT_NE(lecturer, nullptr);
    ASSERT_NE(hod, nullptr);
    ASSERT_NE(student, nullptr);

    const auto deadline = std::chrono::system_clock::now() + std::chrono::hours(48);
    const int assignmentId =
        ctx.assignments().createAssignment(*lecturer, course, "Assignment 1", deadline, 100.0);

    const auto result = ctx.submissions().submit(*student, assignmentId, "work.pdf");
    EXPECT_EQ(result.submission.status(), SubmissionStatus::Submitted);

    ctx.submissions().mark(*lecturer, result.submission.id(), 85.0, "Well done");
    EXPECT_FALSE(ctx.studentView().mySubmission(*student, assignmentId).finalMark.has_value());

    ctx.moderation().approve(*hod, result.submission.id());
    ctx.moderation().release(*hod, assignmentId);

    const auto view = ctx.studentView().mySubmission(*student, assignmentId);
    ASSERT_TRUE(view.finalMark.has_value());
    EXPECT_DOUBLE_EQ(*view.finalMark, 85.0);
    EXPECT_EQ(*view.feedback, "Well done");

    // The Observer is connected: releasing the marks notified the student.
    EXPECT_EQ(ctx.notifications().unreadCount(*student), 1);

    // The audit log recorded the mark, and the head of department can read it.
    EXPECT_EQ(ctx.audit().history(*hod, result.submission.id()).size(), 1u);
}
