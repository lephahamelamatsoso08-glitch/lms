#include <gtest/gtest.h>

#include <chrono>
#include <memory>
#include <string>

#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QTableWidget>

#include "AppContext.h"
#include "HodDialogs.h"
#include "HodWindow.h"
#include "UserFactory.h"
#include "UserWindowFactory.h"

using lms::app::AppContext;
using namespace lms::domain;
using namespace lms::ui;

namespace {

const char* const kPassword = "Passw0rd1";

class HodWindowTest : public ::testing::Test {
protected:
    HodWindowTest() : ctx(":memory:") {
        ctx.createFirstAdmin("admin", "System Admin", kPassword);
        admin = ctx.auth().login("admin", kPassword);
        const int dept1 = ctx.admin().createDepartment(*admin, "Computer Science");
        const int dept2 = ctx.admin().createDepartment(*admin, "Mathematics");
        course1 = ctx.admin().createCourse(*admin, "CS4433", "Software Engineering", dept1);
        ctx.admin().createCourse(*admin, "MA1000", "Calculus", dept2);
        const int lec1 = addUser(Role::Lecturer, "lec1", "Lecturer One", dept1);
        addUser(Role::HoD, "hod1", "Head One", dept1);
        const int stu1 = addUser(Role::Student, "stu1", "Student One", dept1);
        const int stu2 = addUser(Role::Student, "stu2", "Student Two", dept1);
        ctx.admin().assignLecturer(*admin, course1, lec1);
        ctx.admin().enrolStudent(*admin, course1, stu1);
        ctx.admin().enrolStudent(*admin, course1, stu2);

        lecturer = ctx.auth().login("lec1", kPassword);
        hod = ctx.auth().login("hod1", kPassword);
        student1 = ctx.auth().login("stu1", kPassword);
        student2 = ctx.auth().login("stu2", kPassword);

        assignment1 = ctx.assignments().createAssignment(
            *lecturer, course1, "Assignment 1",
            std::chrono::system_clock::now() + std::chrono::hours(48), 100.0);
        id1 = ctx.submissions().submit(*student1, assignment1, "a.pdf").submission.id();
        id2 = ctx.submissions().submit(*student2, assignment1, "b.pdf").submission.id();
        window = std::make_unique<HodWindow>(ctx, hod);
    }

    int addUser(Role role, const std::string& username, const std::string& name, int dept) {
        UserData d;
        d.username = username;
        d.fullName = name;
        d.departmentId = dept;
        d.studentNumber = "S-" + username;
        return ctx.admin().createUser(*admin, role, d, kPassword);
    }

    void markFirst() { ctx.submissions().mark(*lecturer, id1, 80.0, "Good work"); }
    void markBoth() {
        markFirst();
        ctx.submissions().mark(*lecturer, id2, 70.0, "Fine");
    }
    void showAssignment() {
        window->refresh();
        window->findChild<QTableWidget*>("assignmentTable")->selectRow(0);
    }

    QTableWidget* work() { return window->findChild<QTableWidget*>("workTable"); }
    QString summary() { return window->findChild<QLabel*>("summaryLabel")->text(); }
    QString message() { return window->findChild<QLabel*>("messageLabel")->text(); }
    QPushButton* button(const char* name) { return window->findChild<QPushButton*>(name); }

    AppContext ctx;
    std::unique_ptr<User> admin, lecturer, student1, student2;
    std::shared_ptr<User> hod;
    int course1 = 0, assignment1 = 0, id1 = 0, id2 = 0;
    std::unique_ptr<HodWindow> window;  // last: destroyed before the context
};

QPushButton* okButton(QDialog& dialog) {
    return dialog.findChild<QDialogButtonBox*>("buttons")->button(QDialogButtonBox::Ok);
}

}  // namespace

// ---- Lists ----
TEST_F(HodWindowTest, ShowsOnlyTheCoursesOfTheHoDsDepartment) {
    auto* courses = window->findChild<QListWidget*>("courseList");
    ASSERT_EQ(courses->count(), 1);
    EXPECT_TRUE(courses->item(0)->text().contains("CS4433"));
}

TEST_F(HodWindowTest, ListsTheAssignmentsOfTheCourse) {
    auto* table = window->findChild<QTableWidget*>("assignmentTable");
    ASSERT_EQ(table->rowCount(), 1);
    EXPECT_EQ(table->item(0, 0)->text(), "Assignment 1");
    EXPECT_EQ(table->item(0, 2)->text(), "100");
}

TEST_F(HodWindowTest, WithoutASelectionThereIsAHintAndTheButtonsAreDisabled) {
    EXPECT_TRUE(summary().contains("Select an assignment"));
    for (const char* name : {"approveButton", "approveAllButton", "changeButton", "releaseButton",
                             "historyButton"}) {
        EXPECT_FALSE(button(name)->isEnabled()) << name;
    }
}

TEST_F(HodWindowTest, ShowsOnlyWorkThatHasAMark) {
    markFirst();  // the second student's work is submitted but not marked
    showAssignment();
    ASSERT_EQ(work()->rowCount(), 1);
    EXPECT_EQ(work()->item(0, 0)->text(), "S-stu1");
    EXPECT_EQ(work()->item(0, 1)->text(), "Student One");
    EXPECT_EQ(work()->item(0, 2)->text(), "Marked");
    EXPECT_EQ(work()->item(0, 4)->text(), "80");
    EXPECT_EQ(summary(), "1 marked, 0 moderated, 0 released");
}

// ---- FR13: approve ----
TEST_F(HodWindowTest, ApproveMovesTheWorkToModerated) {
    markFirst();
    showAssignment();
    window->approveSubmission(id1);
    EXPECT_TRUE(message().contains("Approved"));
    EXPECT_EQ(work()->item(0, 2)->text(), "Moderated");
    EXPECT_EQ(summary(), "0 marked, 1 moderated, 0 released");
}

TEST_F(HodWindowTest, ApproveAllApprovesEveryMarkedSubmission) {
    markBoth();
    showAssignment();
    window->approveAllMarked();
    EXPECT_TRUE(message().contains("Approved 2"));
    EXPECT_EQ(summary(), "0 marked, 2 moderated, 0 released");
}

TEST_F(HodWindowTest, ApproveAllWithNothingToApproveShowsAMessage) {
    showAssignment();
    window->approveAllMarked();
    EXPECT_TRUE(message().contains("no marked work"));
}

TEST_F(HodWindowTest, ApprovingTwiceShowsAMessageAndKeepsTheStatus) {
    markFirst();
    showAssignment();
    window->approveSubmission(id1);
    window->approveSubmission(id1);
    EXPECT_FALSE(message().isEmpty());
    EXPECT_TRUE(message().contains("cannot move") || message().contains("Moderated"));
    EXPECT_EQ(work()->item(0, 2)->text(), "Moderated");
}

// ---- FR13, FR15: change a mark ----
TEST_F(HodWindowTest, ChangeMarkShowsTheNewMarkAndWritesTheAuditLog) {
    markFirst();
    showAssignment();
    window->changeMark(id1, 75.0, "Marking was generous");
    EXPECT_TRUE(message().contains("Mark changed to 75"));
    EXPECT_TRUE(work()->item(0, 4)->text().contains("75"));
    EXPECT_EQ(ctx.audit().history(*hod, id1).size(), 2u);  // the lecturer's mark and this change
}

TEST_F(HodWindowTest, ChangeMarkNeedsAReason) {
    markFirst();
    showAssignment();
    window->changeMark(id1, 75.0, "");
    EXPECT_TRUE(message().contains("reason"));
    EXPECT_EQ(work()->item(0, 4)->text(), "80");
}

TEST_F(HodWindowTest, ChangeMarkAboveTheMaximumIsRefused) {
    markFirst();
    showAssignment();
    window->changeMark(id1, 150.0, "Reason");
    EXPECT_TRUE(message().contains("between 0 and"));
    EXPECT_EQ(work()->item(0, 4)->text(), "80");
}

TEST_F(HodWindowTest, ChangeMarkAfterApprovalIsRefused) {
    markFirst();
    showAssignment();
    window->approveSubmission(id1);
    window->changeMark(id1, 60.0, "Reason");
    EXPECT_FALSE(message().isEmpty());
    EXPECT_EQ(work()->item(0, 4)->text(), "80");
}

// ---- FR14: release ----
TEST_F(HodWindowTest, ReleaseMakesTheMarksVisibleToTheStudentsAndNotifiesThem) {
    markBoth();
    showAssignment();
    window->approveAllMarked();
    EXPECT_FALSE(ctx.studentView().mySubmission(*student1, assignment1).finalMark.has_value());

    window->releaseMarks();
    EXPECT_TRUE(message().contains("Released 2"));
    EXPECT_EQ(summary(), "0 marked, 0 moderated, 2 released");
    const auto view = ctx.studentView().mySubmission(*student1, assignment1);
    ASSERT_TRUE(view.finalMark.has_value());
    EXPECT_DOUBLE_EQ(*view.finalMark, 80.0);
    EXPECT_EQ(ctx.notifications().unreadCount(*student1), 1);
    EXPECT_EQ(ctx.notifications().unreadCount(*student2), 1);
}

TEST_F(HodWindowTest, ReleaseOnlyReleasesTheApprovedWork) {
    markBoth();
    showAssignment();
    window->approveSubmission(id1);
    window->releaseMarks();
    EXPECT_TRUE(message().contains("Released 1"));
    EXPECT_EQ(summary(), "1 marked, 0 moderated, 1 released");
    EXPECT_FALSE(ctx.studentView().mySubmission(*student2, assignment1).finalMark.has_value());
}

TEST_F(HodWindowTest, ReleaseWithNothingModeratedShowsAMessage) {
    markFirst();
    showAssignment();
    window->releaseMarks();
    EXPECT_TRUE(message().contains("no moderated"));
}

TEST_F(HodWindowTest, ReleaseWithoutAnAssignmentShowsAHint) {
    window->releaseMarks();
    EXPECT_TRUE(message().contains("Select an assignment first"));
}

// ---- Buttons follow the selection ----
TEST_F(HodWindowTest, ButtonsFollowTheStatusOfTheSelectedWork) {
    markFirst();
    showAssignment();
    EXPECT_FALSE(button("approveButton")->isEnabled());  // nothing selected yet
    EXPECT_TRUE(button("approveAllButton")->isEnabled());
    EXPECT_FALSE(button("releaseButton")->isEnabled());

    work()->selectRow(0);
    EXPECT_TRUE(button("approveButton")->isEnabled());
    EXPECT_TRUE(button("changeButton")->isEnabled());
    EXPECT_TRUE(button("historyButton")->isEnabled());

    window->approveSubmission(id1);  // the selected row stays selected
    EXPECT_FALSE(button("approveButton")->isEnabled());
    EXPECT_FALSE(button("changeButton")->isEnabled());
    EXPECT_FALSE(button("approveAllButton")->isEnabled());
    EXPECT_TRUE(button("releaseButton")->isEnabled());
    EXPECT_TRUE(button("historyButton")->isEnabled());
}

TEST_F(HodWindowTest, LogOutButtonSendsTheSignal) {
    int count = 0;
    QObject::connect(window.get(), &UserWindow::loggedOut, [&count]() { ++count; });
    button("logoutButton")->click();
    EXPECT_EQ(count, 1);
}

// ---- The forms ----
TEST_F(HodWindowTest, HistoryDialogListsTheChangesWithNames) {
    markFirst();
    ctx.moderation().changeMark(*hod, id1, 75.0, "Marking was generous");
    AuditHistoryDialog dialog("Student One", ctx.hodView().history(*hod, id1));
    auto* table = dialog.findChild<QTableWidget*>("historyTable");
    ASSERT_EQ(table->rowCount(), 2);
    EXPECT_EQ(table->item(0, 1)->text(), "Lecturer One");
    EXPECT_EQ(table->item(0, 2)->text(), "(first mark)");
    EXPECT_EQ(table->item(0, 3)->text(), "80");
    EXPECT_EQ(table->item(0, 4)->text(), "Initial mark");
    EXPECT_EQ(table->item(1, 1)->text(), "Head One");
    EXPECT_EQ(table->item(1, 2)->text(), "80");
    EXPECT_EQ(table->item(1, 3)->text(), "75");
    EXPECT_EQ(table->item(1, 4)->text(), "Marking was generous");
}

TEST(HodDialogsTest, ChangeMarkFormNeedsAReasonAndStartsAtTheCurrentMark) {
    ChangeMarkDialog dialog("Student One", 100.0, 80.0);
    EXPECT_DOUBLE_EQ(dialog.newMark(), 80.0);
    okButton(dialog)->click();
    EXPECT_NE(dialog.result(), QDialog::Accepted);
    EXPECT_FALSE(dialog.findChild<QLabel*>("errorLabel")->text().isEmpty());

    dialog.findChild<QLineEdit*>("reasonEdit")->setText("  Marking was generous  ");
    okButton(dialog)->click();
    EXPECT_EQ(dialog.result(), QDialog::Accepted);
    EXPECT_EQ(dialog.reason(), "Marking was generous");
}

TEST(HodDialogsTest, ChangeMarkCannotBeAboveTheMaximum) {
    ChangeMarkDialog dialog("Student One", 50.0, 40.0);
    dialog.findChild<QDoubleSpinBox*>("markEdit")->setValue(75.0);
    EXPECT_DOUBLE_EQ(dialog.newMark(), 50.0);
}

TEST(UserWindowFactoryTest, HoDGetsTheHoDWindow) {
    AppContext ctx(":memory:");
    UserData d;
    d.username = "head";
    d.fullName = "Some Head";
    d.departmentId = 1;
    std::shared_ptr<User> head = UserFactory::create(Role::HoD, d);
    auto window = makeUserWindow(ctx, head);
    EXPECT_NE(dynamic_cast<HodWindow*>(window.get()), nullptr);
}
