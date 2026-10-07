#include <gtest/gtest.h>

#include <chrono>
#include <memory>
#include <optional>
#include <string>

#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QTableWidget>

#include "AppContext.h"
#include "LecturerDialogs.h"
#include "LecturerWindow.h"
#include "RoleWindow.h"
#include "UiFormat.h"
#include "UserFactory.h"
#include "UserWindowFactory.h"

using lms::app::AppContext;
using namespace lms::domain;
using namespace lms::ui;

namespace {

const char* const kPassword = "Passw0rd1";

TimePoint hoursFromNow(int hours) {
    return std::chrono::floor<std::chrono::seconds>(std::chrono::system_clock::now() +
                                                    std::chrono::hours(hours));
}

QPushButton* okButton(QDialog& dialog) {
    return dialog.findChild<QDialogButtonBox*>("buttons")->button(QDialogButtonBox::Ok);
}

QString dialogError(QDialog& dialog) { return dialog.findChild<QLabel*>("errorLabel")->text(); }

class LecturerWindowTest : public ::testing::Test {
protected:
    LecturerWindowTest() : ctx(":memory:") {
        ctx.createFirstAdmin("admin", "System Admin", kPassword);
        admin = ctx.auth().login("admin", kPassword);
        dept = ctx.admin().createDepartment(*admin, "Computer Science");
        course1 = ctx.admin().createCourse(*admin, "CS4433", "Software Engineering", dept);
        const int course2 = ctx.admin().createCourse(*admin, "CS4000", "Other Course", dept);
        const int lec1 = addUser(Role::Lecturer, "lec1", "Lecturer One");
        const int lec2 = addUser(Role::Lecturer, "lec2", "Lecturer Two");
        addUser(Role::HoD, "hod1", "Head One");
        student1Id = addUser(Role::Student, "stu1", "Student One");
        student2Id = addUser(Role::Student, "stu2", "Student Two");
        ctx.admin().assignLecturer(*admin, course1, lec1);
        ctx.admin().assignLecturer(*admin, course2, lec2);
        ctx.admin().enrolStudent(*admin, course1, student1Id);
        ctx.admin().enrolStudent(*admin, course1, student2Id);

        lecturer = ctx.auth().login("lec1", kPassword);
        hod = ctx.auth().login("hod1", kPassword);
        student1 = ctx.auth().login("stu1", kPassword);
        student2 = ctx.auth().login("stu2", kPassword);

        assignment1 = ctx.assignments().createAssignment(*lecturer, course1, "Assignment 1",
                                                         hoursFromNow(48), 100.0);
        ctx.assignments().createAssignment(*lecturer, course1, "Assignment 2", hoursFromNow(72),
                                           50.0);
        window = std::make_unique<LecturerWindow>(ctx, lecturer);
    }

    int addUser(Role role, const std::string& username, const std::string& name) {
        UserData d;
        d.username = username;
        d.fullName = name;
        d.departmentId = dept;
        d.studentNumber = "S-" + username;
        return ctx.admin().createUser(*admin, role, d, kPassword);
    }

    QTableWidget* assignmentTable() { return window->findChild<QTableWidget*>("assignmentTable"); }
    QTableWidget* submissionTable() { return window->findChild<QTableWidget*>("submissionTable"); }
    QListWidget* courses() { return window->findChild<QListWidget*>("courseList"); }
    QString summary() { return window->findChild<QLabel*>("summaryLabel")->text(); }
    QString message() { return window->findChild<QLabel*>("messageLabel")->text(); }
    QPushButton* button(const char* name) { return window->findChild<QPushButton*>(name); }

    void selectAssignment(int row) { assignmentTable()->selectRow(row); }
    int submissionIdOfFirstStudent() {
        return ctx.assignments().listSubmissions(*lecturer, assignment1)[0].id();
    }

    AppContext ctx;
    std::unique_ptr<User> admin;
    std::shared_ptr<User> lecturer;
    std::unique_ptr<User> hod;
    std::unique_ptr<User> student1;
    std::unique_ptr<User> student2;
    int dept = 0;
    int course1 = 0;
    int student1Id = 0;
    int student2Id = 0;
    int assignment1 = 0;
    std::unique_ptr<LecturerWindow> window;  // last: destroyed before the context
};

}  // namespace

// ---- Lists ----
TEST_F(LecturerWindowTest, ShowsOnlyTheCoursesTheLecturerTeaches) {
    ASSERT_EQ(courses()->count(), 1);
    EXPECT_TRUE(courses()->item(0)->text().contains("CS4433"));
}

TEST_F(LecturerWindowTest, ListsTheAssignmentsOfTheSelectedCourse) {
    ASSERT_EQ(assignmentTable()->rowCount(), 2);
    EXPECT_EQ(assignmentTable()->item(0, 0)->text(), "Assignment 1");
    EXPECT_EQ(assignmentTable()->item(0, 2)->text(), "100");
    EXPECT_EQ(assignmentTable()->item(1, 0)->text(), "Assignment 2");
}

TEST_F(LecturerWindowTest, WithoutASelectionThereIsAHintAndTheActionButtonsAreDisabled) {
    EXPECT_TRUE(summary().contains("Select an assignment"));
    EXPECT_TRUE(button("newAssignmentButton")->isEnabled());
    EXPECT_FALSE(button("markButton")->isEnabled());
    EXPECT_FALSE(button("extensionButton")->isEnabled());
}

TEST_F(LecturerWindowTest, SelectingAnAssignmentListsEveryEnrolledStudent) {
    selectAssignment(0);
    ASSERT_EQ(submissionTable()->rowCount(), 2);
    EXPECT_EQ(submissionTable()->item(0, 0)->text(), "S-stu1");
    EXPECT_EQ(submissionTable()->item(0, 1)->text(), "Student One");
    EXPECT_EQ(submissionTable()->item(0, 2)->text(), "Not submitted");
    EXPECT_EQ(submissionTable()->item(1, 1)->text(), "Student Two");
    EXPECT_EQ(summary(), "2 students, 0 submitted, 0 marked");
}

TEST_F(LecturerWindowTest, SubmittedWorkShowsStatusFilesAndTheLatestFile) {
    ctx.submissions().submit(*student1, assignment1, "v1.pdf");
    ctx.submissions().submit(*student1, assignment1, "v2.pdf");
    selectAssignment(0);
    window->refresh();
    EXPECT_EQ(submissionTable()->item(0, 2)->text(), "Submitted");
    EXPECT_EQ(submissionTable()->item(0, 4)->text(), "2");
    EXPECT_EQ(submissionTable()->item(0, 5)->text(), "v2.pdf");
    EXPECT_EQ(summary(), "2 students, 1 submitted, 0 marked");
}

// ---- FR6: create an assignment ----
TEST_F(LecturerWindowTest, CreateAssignmentIsListed) {
    window->createAssignment("Assignment 3", hoursFromNow(96), 40.0);
    EXPECT_TRUE(message().contains("created"));
    EXPECT_EQ(assignmentTable()->rowCount(), 3);
}

TEST_F(LecturerWindowTest, PastDeadlineIsRefusedWithAMessage) {
    window->createAssignment("Late one", hoursFromNow(-1), 40.0);
    EXPECT_TRUE(message().contains("future"));
    EXPECT_EQ(assignmentTable()->rowCount(), 2);
}

TEST_F(LecturerWindowTest, BlankTitleIsRefusedWithAMessage) {
    window->createAssignment("   ", hoursFromNow(96), 40.0);
    EXPECT_TRUE(message().contains("title"));
    EXPECT_EQ(assignmentTable()->rowCount(), 2);
}

// ---- FR12: mark and re-mark ----
TEST_F(LecturerWindowTest, MarkSavesTheMarkAndShowsItToTheLecturer) {
    ctx.submissions().submit(*student1, assignment1, "work.pdf");
    selectAssignment(0);
    window->refresh();
    window->markSubmission(submissionIdOfFirstStudent(), 80.0, "Good work", "");
    EXPECT_TRUE(message().contains("Mark saved"));
    EXPECT_EQ(submissionTable()->item(0, 2)->text(), "Marked");
    EXPECT_EQ(submissionTable()->item(0, 6)->text(), "80");
    EXPECT_EQ(summary(), "2 students, 1 submitted, 1 marked");
}

TEST_F(LecturerWindowTest, MarkErrorsAreShownAsMessagesAndNothingIsSaved) {
    ctx.submissions().submit(*student1, assignment1, "work.pdf");
    selectAssignment(0);
    window->refresh();
    const int id = submissionIdOfFirstStudent();

    window->markSubmission(id, 80.0, "", "");
    EXPECT_TRUE(message().contains("feedback"));
    window->markSubmission(id, 150.0, "Feedback", "");
    EXPECT_TRUE(message().contains("between 0 and"));
    EXPECT_EQ(submissionTable()->item(0, 2)->text(), "Submitted");
}

TEST_F(LecturerWindowTest, ReMarkNeedsAReason) {
    ctx.submissions().submit(*student1, assignment1, "work.pdf");
    selectAssignment(0);
    window->refresh();
    const int id = submissionIdOfFirstStudent();
    window->markSubmission(id, 80.0, "Good work", "");
    window->markSubmission(id, 85.0, "Better", "");
    EXPECT_TRUE(message().contains("reason"));
    EXPECT_EQ(submissionTable()->item(0, 6)->text(), "80");

    window->markSubmission(id, 85.0, "Better", "Re-read question 3");
    EXPECT_TRUE(message().contains("Mark saved"));
    EXPECT_EQ(submissionTable()->item(0, 6)->text(), "85");
}

TEST_F(LecturerWindowTest, ModeratedWorkCannotBeMarkedAgain) {
    ctx.submissions().submit(*student1, assignment1, "work.pdf");
    selectAssignment(0);
    const int id = submissionIdOfFirstStudent();
    ctx.submissions().mark(*lecturer, id, 80.0, "Good work");
    ctx.moderation().approve(*hod, id);
    window->refresh();

    submissionTable()->selectRow(0);
    EXPECT_FALSE(button("markButton")->isEnabled());
    window->markSubmission(id, 90.0, "Changed", "Reason");
    EXPECT_FALSE(message().isEmpty());
    EXPECT_EQ(submissionTable()->item(0, 2)->text(), "Moderated");
    EXPECT_EQ(submissionTable()->item(0, 6)->text(), "80");
}

// ---- FR7: extensions ----
TEST_F(LecturerWindowTest, GrantExtensionChangesTheStudentsDeadline) {
    selectAssignment(0);
    const TimePoint newDeadline = hoursFromNow(24 * 10);
    window->grantExtension(student2Id, newDeadline, "Power outage");
    EXPECT_TRUE(message().contains("Extension granted"));
    const auto items = ctx.studentView().myAssignments(*student2, course1);
    EXPECT_EQ(items[0].deadline, newDeadline);
    const auto others = ctx.studentView().myAssignments(*student1, course1);
    EXPECT_NE(others[0].deadline, newDeadline);
}

TEST_F(LecturerWindowTest, ExtensionErrorsAreShownAsMessages) {
    selectAssignment(0);
    window->grantExtension(student2Id, hoursFromNow(1), "Reason");  // earlier than the deadline
    EXPECT_TRUE(message().contains("later"));
    window->grantExtension(student2Id, hoursFromNow(24 * 10), "");
    EXPECT_TRUE(message().contains("reason"));
}

TEST_F(LecturerWindowTest, ExtensionWithoutASelectedAssignmentShowsAHint) {
    window->grantExtension(student2Id, hoursFromNow(24 * 10), "Reason");
    EXPECT_TRUE(message().contains("Select an assignment first"));
}

// ---- Buttons follow the selection ----
TEST_F(LecturerWindowTest, ButtonsFollowTheSelectedStudent) {
    selectAssignment(0);
    EXPECT_FALSE(button("extensionButton")->isEnabled());
    submissionTable()->selectRow(0);
    EXPECT_TRUE(button("extensionButton")->isEnabled());
    EXPECT_FALSE(button("markButton")->isEnabled());  // nothing submitted yet

    ctx.submissions().submit(*student1, assignment1, "work.pdf");
    window->refresh();  // the selected student stays selected
    EXPECT_TRUE(button("markButton")->isEnabled());
}

TEST_F(LecturerWindowTest, LogOutButtonSendsTheSignal) {
    int count = 0;
    QObject::connect(window.get(), &UserWindow::loggedOut, [&count]() { ++count; });
    button("logoutButton")->click();
    EXPECT_EQ(count, 1);
}

// ---- The forms ----
TEST(LecturerDialogsTest, AssignmentFormNeedsATitle) {
    AssignmentDialog dialog;
    okButton(dialog)->click();
    EXPECT_NE(dialog.result(), QDialog::Accepted);
    EXPECT_FALSE(dialogError(dialog).isEmpty());

    dialog.findChild<QLineEdit*>("titleEdit")->setText("  Assignment 3  ");
    okButton(dialog)->click();
    EXPECT_EQ(dialog.result(), QDialog::Accepted);
    EXPECT_EQ(dialog.title(), "Assignment 3");
    EXPECT_DOUBLE_EQ(dialog.maxMark(), 100.0);
    EXPECT_GT(dialog.deadline(), std::chrono::system_clock::now());
}

TEST(LecturerDialogsTest, MarkFormNeedsFeedback) {
    MarkDialog dialog("Student One", 100.0, std::nullopt, "", false);
    EXPECT_FALSE(dialog.findChild<QLineEdit*>("reasonEdit")->isEnabled());
    okButton(dialog)->click();
    EXPECT_NE(dialog.result(), QDialog::Accepted);

    dialog.findChild<QPlainTextEdit*>("feedbackEdit")->setPlainText("Good work");
    okButton(dialog)->click();
    EXPECT_EQ(dialog.result(), QDialog::Accepted);
    EXPECT_EQ(dialog.feedback(), "Good work");
    EXPECT_DOUBLE_EQ(dialog.rawMark(), 0.0);
}

TEST(LecturerDialogsTest, ReMarkFormAlsoNeedsAReasonAndShowsTheCurrentValues) {
    MarkDialog dialog("Student One", 100.0, 80.0, "Old feedback", true);
    EXPECT_DOUBLE_EQ(dialog.rawMark(), 80.0);
    EXPECT_EQ(dialog.feedback(), "Old feedback");
    EXPECT_TRUE(dialog.findChild<QLineEdit*>("reasonEdit")->isEnabled());

    okButton(dialog)->click();
    EXPECT_NE(dialog.result(), QDialog::Accepted);
    EXPECT_FALSE(dialogError(dialog).isEmpty());

    dialog.findChild<QLineEdit*>("reasonEdit")->setText("Re-read question 3");
    okButton(dialog)->click();
    EXPECT_EQ(dialog.result(), QDialog::Accepted);
    EXPECT_EQ(dialog.reason(), "Re-read question 3");
}

TEST(LecturerDialogsTest, MarkCannotBeAboveTheMaximum) {
    MarkDialog dialog("Student One", 50.0, std::nullopt, "", false);
    auto* mark = dialog.findChild<QDoubleSpinBox*>("markEdit");
    mark->setValue(75.0);
    EXPECT_DOUBLE_EQ(dialog.rawMark(), 50.0);
}

TEST(LecturerDialogsTest, ExtensionFormNeedsAReasonAndALaterDeadline) {
    const TimePoint normal = hoursFromNow(48);
    ExtensionDialog dialog("Student One", normal);
    okButton(dialog)->click();
    EXPECT_NE(dialog.result(), QDialog::Accepted);

    dialog.findChild<QLineEdit*>("reasonEdit")->setText("Power outage");
    okButton(dialog)->click();
    EXPECT_EQ(dialog.result(), QDialog::Accepted);
    EXPECT_EQ(dialog.reason(), "Power outage");
    EXPECT_GT(dialog.newDeadline(), normal);
}

TEST(UserWindowFactoryTest, LecturerGetsTheLecturerWindow) {
    AppContext ctx(":memory:");
    UserData d;
    d.username = "lec";
    d.fullName = "Some Lecturer";
    std::shared_ptr<User> lecturer = UserFactory::create(Role::Lecturer, d);
    auto window = makeUserWindow(ctx, lecturer);
    EXPECT_NE(dynamic_cast<LecturerWindow*>(window.get()), nullptr);
}
