#include <gtest/gtest.h>

#include <chrono>
#include <memory>
#include <string>

#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QTabWidget>
#include <QTableWidget>

#include "AppContext.h"
#include "RoleWindow.h"
#include "StudentWindow.h"
#include "UiFormat.h"
#include "UserFactory.h"
#include "UserWindowFactory.h"

using lms::app::AppContext;
using namespace lms::domain;
using namespace lms::ui;

namespace {

const char* const kPassword = "Passw0rd1";

class StudentWindowTest : public ::testing::Test {
protected:
    StudentWindowTest() : ctx(":memory:") {
        ctx.createFirstAdmin("admin", "System Admin", kPassword);
        admin = ctx.auth().login("admin", kPassword);
        const int dept = ctx.admin().createDepartment(*admin, "Computer Science");
        const int course1 = ctx.admin().createCourse(*admin, "CS4433", "Software Engineering", dept);
        ctx.admin().createCourse(*admin, "CS4000", "Other Course", dept);
        const int lecturerId = addUser(Role::Lecturer, "lec1", "Lecturer One", dept);
        addUser(Role::HoD, "hod1", "Head One", dept);
        studentId = addUser(Role::Student, "stu1", "Student One", dept);
        ctx.admin().assignLecturer(*admin, course1, lecturerId);
        ctx.admin().enrolStudent(*admin, course1, studentId);

        lecturer = ctx.auth().login("lec1", kPassword);
        hod = ctx.auth().login("hod1", kPassword);
        student = ctx.auth().login("stu1", kPassword);

        const auto deadline = std::chrono::system_clock::now() + std::chrono::hours(48);
        assignment1 = ctx.assignments().createAssignment(*lecturer, course1, "Assignment 1",
                                                         deadline, 100.0);
        ctx.assignments().createAssignment(*lecturer, course1, "Assignment 2",
                                           deadline + std::chrono::hours(24), 50.0);
        window = std::make_unique<StudentWindow>(ctx, student);
    }

    int addUser(Role role, const std::string& username, const std::string& name, int dept) {
        UserData d;
        d.username = username;
        d.fullName = name;
        d.departmentId = dept;
        d.studentNumber = "S-" + username;
        return ctx.admin().createUser(*admin, role, d, kPassword);
    }

    QTableWidget* table() { return window->findChild<QTableWidget*>("assignmentTable"); }
    QListWidget* courses() { return window->findChild<QListWidget*>("courseList"); }
    QListWidget* notices() { return window->findChild<QListWidget*>("notificationList"); }
    QTabWidget* tabs() { return window->findChild<QTabWidget*>("tabs"); }
    QString detail() { return window->findChild<QLabel*>("detailLabel")->text(); }
    QString message() { return window->findChild<QLabel*>("messageLabel")->text(); }
    QPushButton* button(const char* name) { return window->findChild<QPushButton*>(name); }

    int submissionId() { return ctx.assignments().listSubmissions(*lecturer, assignment1)[0].id(); }

    AppContext ctx;
    std::unique_ptr<User> admin;
    std::unique_ptr<User> lecturer;
    std::unique_ptr<User> hod;
    std::shared_ptr<User> student;
    int studentId = 0;
    int assignment1 = 0;
    std::unique_ptr<StudentWindow> window;  // last: destroyed before the context
};

}  // namespace

// ---- FR8: courses and assignments ----
TEST_F(StudentWindowTest, ShowsOnlyTheCoursesTheStudentIsEnrolledIn) {
    ASSERT_EQ(courses()->count(), 1);
    EXPECT_TRUE(courses()->item(0)->text().contains("CS4433"));
}

TEST_F(StudentWindowTest, ListsTheAssignmentsInDeadlineOrderWithTheirStatus) {
    ASSERT_EQ(table()->rowCount(), 2);
    EXPECT_EQ(table()->item(0, 0)->text(), "Assignment 1");
    EXPECT_EQ(table()->item(0, 2)->text(), "100");
    EXPECT_EQ(table()->item(0, 3)->text(), "Not submitted");
    EXPECT_EQ(table()->item(1, 0)->text(), "Assignment 2");
    EXPECT_EQ(table()->item(1, 2)->text(), "50");
}

TEST_F(StudentWindowTest, WithoutASelectionThereIsAHintAndSubmitIsDisabled) {
    EXPECT_TRUE(detail().contains("Select an assignment"));
    EXPECT_FALSE(button("submitButton")->isEnabled());
}

TEST_F(StudentWindowTest, SelectingAnAssignmentShowsTheDetailsAndEnablesSubmit) {
    table()->selectRow(0);
    EXPECT_TRUE(detail().contains("Assignment 1"));
    EXPECT_TRUE(detail().contains("Maximum mark: 100"));
    EXPECT_TRUE(detail().contains("Not submitted"));
    EXPECT_TRUE(button("submitButton")->isEnabled());
}

TEST_F(StudentWindowTest, ShowsTheStudentsOwnDeadlineWhenThereIsAnExtension) {
    const auto extended = std::chrono::system_clock::now() + std::chrono::hours(24 * 10);
    ctx.assignments().grantExtension(*lecturer, assignment1, studentId, extended, "Power outage");
    window->refresh();
    EXPECT_EQ(table()->item(0, 1)->text(), formatTime(extended));
}

// ---- FR9, FR10: submit and resubmit ----
TEST_F(StudentWindowTest, SubmitShowsAConfirmationAndUpdatesTheStatus) {
    table()->selectRow(0);
    window->submitFile("work.pdf");
    EXPECT_TRUE(message().contains("Submitted."));
    EXPECT_TRUE(message().contains("version 1"));
    EXPECT_EQ(table()->item(0, 3)->text(), "Submitted");
    EXPECT_TRUE(detail().contains("Files uploaded: 1"));
}

TEST_F(StudentWindowTest, ResubmitKeepsTheStatusAndCountsTheVersions) {
    table()->selectRow(0);
    window->submitFile("v1.pdf");
    window->submitFile("v2.pdf");
    EXPECT_TRUE(message().contains("Resubmitted."));
    EXPECT_TRUE(message().contains("version 2"));
    EXPECT_EQ(table()->item(0, 3)->text(), "Submitted");
    EXPECT_TRUE(detail().contains("Files uploaded: 2"));
}

TEST_F(StudentWindowTest, UnsuitableFileShowsAMessageAndSavesNothing) {
    table()->selectRow(0);
    window->submitFile("virus.exe");
    EXPECT_TRUE(message().contains("pdf"));
    EXPECT_EQ(table()->item(0, 3)->text(), "Not submitted");
}

TEST_F(StudentWindowTest, SubmitWithoutASelectionShowsAHint) {
    window->submitFile("work.pdf");
    EXPECT_TRUE(message().contains("Select an assignment first"));
}

TEST_F(StudentWindowTest, ResubmittingMarkedWorkIsRefusedWithAMessage) {
    table()->selectRow(0);
    window->submitFile("work.pdf");
    ctx.submissions().mark(*lecturer, submissionId(), 85.0, "Excellent work");
    window->submitFile("v2.pdf");
    EXPECT_TRUE(message().contains("already been marked"));
}

// ---- FR17 and rule C1: no mark before release ----
TEST_F(StudentWindowTest, MarkAndFeedbackStayHiddenUntilTheHoDReleasesThem) {
    table()->selectRow(0);
    window->submitFile("work.pdf");
    const int id = submissionId();

    ctx.submissions().mark(*lecturer, id, 85.0, "Excellent work");
    window->refresh();
    EXPECT_TRUE(detail().contains("Marked, awaiting moderation"));
    EXPECT_FALSE(detail().contains("Final mark"));
    EXPECT_FALSE(detail().contains("Excellent work"));

    ctx.moderation().approve(*hod, id);
    window->refresh();
    EXPECT_TRUE(detail().contains("Moderated, awaiting release"));
    EXPECT_FALSE(detail().contains("Final mark"));
    EXPECT_FALSE(detail().contains("Excellent work"));

    ctx.moderation().release(*hod, assignment1);
    window->refresh();
    EXPECT_TRUE(detail().contains("Released"));
    EXPECT_TRUE(detail().contains("Final mark: 85 out of 100"));
    EXPECT_TRUE(detail().contains("Excellent work"));
}

TEST_F(StudentWindowTest, DescribeShowsThePenaltyOfAReleasedLateMark) {
    lms::application::StudentSubmissionView view;
    view.assignmentTitle = "Assignment 1";
    view.maxMark = 100.0;
    view.submitted = true;
    view.statusLabel = "Released";
    view.lateDays = 2;
    view.versionCount = 1;
    view.rawMark = 80.0;
    view.finalMark = 60.0;
    view.feedback = std::string("Late but solid");
    const QString text = StudentWindow::describe(view);
    EXPECT_TRUE(text.contains("Final mark: 60 out of 100"));
    EXPECT_TRUE(text.contains("2 day(s) late"));
    EXPECT_TRUE(text.contains("before the penalty: 80"));
    EXPECT_TRUE(text.contains("late penalty: 20"));
}

// ---- FR18: notifications ----
TEST_F(StudentWindowTest, ReleaseCreatesANoticeThatCanBeMarkedAsRead) {
    EXPECT_EQ(notices()->count(), 0);
    EXPECT_EQ(tabs()->tabText(1), "Notifications");

    table()->selectRow(0);
    window->submitFile("work.pdf");
    const int id = submissionId();
    ctx.submissions().mark(*lecturer, id, 85.0, "Excellent work");
    ctx.moderation().approve(*hod, id);
    ctx.moderation().release(*hod, assignment1);
    window->refresh();

    ASSERT_EQ(notices()->count(), 1);
    EXPECT_TRUE(notices()->item(0)->text().contains("Assignment 1"));
    EXPECT_EQ(tabs()->tabText(1), "Notifications (1)");

    notices()->setCurrentRow(0);
    button("markReadButton")->click();
    EXPECT_EQ(ctx.notifications().unreadCount(*student), 0);
    EXPECT_EQ(tabs()->tabText(1), "Notifications");
    EXPECT_EQ(notices()->count(), 1);
}

TEST_F(StudentWindowTest, MarkReadWithoutASelectionShowsAHint) {
    button("markReadButton")->click();
    EXPECT_TRUE(message().contains("Select a notification first"));
}

// ---- Other ----
TEST_F(StudentWindowTest, LogOutButtonSendsTheSignal) {
    int count = 0;
    QObject::connect(window.get(), &UserWindow::loggedOut, [&count]() { ++count; });
    button("logoutButton")->click();
    EXPECT_EQ(count, 1);
}

TEST(UserWindowFactoryTest, StudentGetsTheStudentWindowAndOtherRolesGetTheTemporaryPage) {
    AppContext ctx(":memory:");
    UserData d;
    d.username = "someone";
    d.fullName = "Some One";
    d.studentNumber = "S001";
    d.departmentId = 1;
    std::shared_ptr<User> student = UserFactory::create(Role::Student, d);
    std::shared_ptr<User> admin = UserFactory::create(Role::Admin, d);

    auto studentWindow = makeUserWindow(ctx, student);
    auto adminWindow = makeUserWindow(ctx, admin);
    EXPECT_NE(dynamic_cast<StudentWindow*>(studentWindow.get()), nullptr);
    EXPECT_NE(dynamic_cast<RoleWindow*>(adminWindow.get()), nullptr);
}

TEST(UiFormatTest, MarksAreShownWithoutUselessZeros) {
    EXPECT_EQ(formatMark(85.0), "85");
    EXPECT_EQ(formatMark(62.5), "62.5");
    EXPECT_EQ(formatMark(62.25), "62.25");
    EXPECT_EQ(formatMark(0.0), "0");
    EXPECT_EQ(formatMark(100.0), "100");
}
