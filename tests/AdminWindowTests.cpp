#include <gtest/gtest.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <QComboBox>
#include <QDialogButtonBox>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QTableWidget>

#include "AdminDialogs.h"
#include "AdminWindow.h"
#include "AppContext.h"
#include "DemoData.h"
#include "UserFactory.h"

using lms::app::AppContext;
using lms::app::kDemoPassword;
using lms::app::seedDemoData;
using namespace lms::domain;
using namespace lms::ui;

namespace {

class AdminWindowTest : public ::testing::Test {
protected:
    AdminWindowTest() : ctx(":memory:") {
        seedDemoData(ctx);
        admin = ctx.auth().login("admin", kDemoPassword);
        window = std::make_unique<AdminWindow>(ctx, admin);
    }

    int idOf(const std::string& username) { return ctx.auth().login(username, kDemoPassword)->id(); }
    int departmentId(const std::string& name) {
        for (const auto& department : ctx.adminView().departments(*admin)) {
            if (department.name == name) {
                return department.id;
            }
        }
        return 0;
    }

    QTableWidget* users() { return window->findChild<QTableWidget*>("userTable"); }
    QTableWidget* courses() { return window->findChild<QTableWidget*>("courseTable"); }
    QTableWidget* audit() { return window->findChild<QTableWidget*>("auditTable"); }
    QString message() { return window->findChild<QLabel*>("messageLabel")->text(); }
    QPushButton* button(const char* name) { return window->findChild<QPushButton*>(name); }

    AppContext ctx;
    std::shared_ptr<User> admin;
    std::unique_ptr<AdminWindow> window;  // last: destroyed before the context
};

UserData person(const std::string& username, const std::string& name) {
    UserData d;
    d.username = username;
    d.fullName = name;
    return d;
}

QPushButton* okButton(QDialog& dialog) {
    return dialog.findChild<QDialogButtonBox*>("buttons")->button(QDialogButtonBox::Ok);
}

QString dialogError(QDialog& dialog) { return dialog.findChild<QLabel*>("errorLabel")->text(); }

}  // namespace

// ---- Users (FR3) ----
TEST_F(AdminWindowTest, UsersTabListsEveryoneWithRoleDetailAndStatus) {
    ASSERT_EQ(users()->rowCount(), 5);
    EXPECT_EQ(users()->item(0, 0)->text(), "admin");
    EXPECT_EQ(users()->item(0, 2)->text(), "Administrator");
    EXPECT_EQ(users()->item(1, 3)->text(), "Computer Science");
    EXPECT_EQ(users()->item(3, 0)->text(), "student1");
    EXPECT_EQ(users()->item(3, 3)->text(), "S001");
    EXPECT_EQ(users()->item(3, 4)->text(), "Active");
}

TEST_F(AdminWindowTest, CreateUserAddsARowAndTheUserCanLogIn) {
    window->createUser(Role::Lecturer, person("lecturer2", "Lecturer Two"), "Passw0rd1");
    EXPECT_TRUE(message().contains("User created"));
    EXPECT_EQ(users()->rowCount(), 6);
    EXPECT_NE(ctx.auth().login("lecturer2", "Passw0rd1"), nullptr);
}

TEST_F(AdminWindowTest, WeakPasswordIsRefusedWithTheRule) {
    window->createUser(Role::Lecturer, person("lecturer2", "Lecturer Two"), "weak");
    EXPECT_TRUE(message().contains("8 characters"));
    EXPECT_EQ(users()->rowCount(), 5);
}

TEST_F(AdminWindowTest, DuplicateUsernameShowsAFriendlyMessage) {
    UserData d = person("student1", "Another Student");
    d.studentNumber = "S009";
    window->createUser(Role::Student, d, "Passw0rd1");
    EXPECT_TRUE(message().contains("already"));
    EXPECT_EQ(users()->rowCount(), 5);
}

TEST_F(AdminWindowTest, DeactivateAndReactivateChangeTheStatusAndTheLogin) {
    const int id = idOf("student1");
    window->setUserActive(id, false);
    EXPECT_TRUE(message().contains("deactivated"));
    EXPECT_EQ(users()->item(3, 4)->text(), "Inactive");
    EXPECT_THROW(ctx.auth().login("student1", kDemoPassword), lms::application::AuthenticationError);

    window->setUserActive(id, true);
    EXPECT_TRUE(message().contains("reactivated"));
    EXPECT_EQ(users()->item(3, 4)->text(), "Active");
    EXPECT_NE(ctx.auth().login("student1", kDemoPassword), nullptr);
}

TEST_F(AdminWindowTest, AdministratorCannotDeactivateTheirOwnAccount) {
    window->setUserActive(admin->id(), false);
    EXPECT_TRUE(message().contains("own account"));
    EXPECT_EQ(users()->item(0, 4)->text(), "Active");
}

TEST_F(AdminWindowTest, ResetPasswordReplacesTheOldPassword) {
    const int id = idOf("student1");
    window->resetPassword(id, "NewPassw0rd2");
    EXPECT_TRUE(message().contains("Password reset"));
    EXPECT_NE(ctx.auth().login("student1", "NewPassw0rd2"), nullptr);
    EXPECT_THROW(ctx.auth().login("student1", kDemoPassword), lms::application::AuthenticationError);
}

TEST_F(AdminWindowTest, ResetPasswordRefusesAWeakPasswordAndKeepsTheOldOne) {
    window->resetPassword(idOf("student1"), "weak");
    EXPECT_TRUE(message().contains("8 characters"));
    EXPECT_NE(ctx.auth().login("student1", kDemoPassword), nullptr);
}

// ---- Departments and courses (FR4) ----
TEST_F(AdminWindowTest, CoursesTabShowsDepartmentLecturersAndStudentCount) {
    ASSERT_EQ(courses()->rowCount(), 2);
    EXPECT_EQ(courses()->item(0, 0)->text(), "CS3300");
    EXPECT_EQ(courses()->item(0, 2)->text(), "Computer Science");
    EXPECT_EQ(courses()->item(0, 3)->text(), "Lecturer One");
    EXPECT_EQ(courses()->item(0, 4)->text(), "1");
    EXPECT_EQ(courses()->item(1, 4)->text(), "2");
}

TEST_F(AdminWindowTest, CreateDepartmentAndCourse) {
    window->createDepartment("Mathematics");
    EXPECT_TRUE(message().contains("Department created"));
    window->createCourse("MA1000", "Calculus", departmentId("Mathematics"));
    EXPECT_TRUE(message().contains("Course created"));
    ASSERT_EQ(courses()->rowCount(), 3);
    EXPECT_EQ(courses()->item(2, 0)->text(), "MA1000");
    EXPECT_EQ(courses()->item(2, 2)->text(), "Mathematics");
}

TEST_F(AdminWindowTest, DuplicatesAndUnknownDepartmentsShowAFriendlyMessage) {
    window->createDepartment("Computer Science");
    EXPECT_TRUE(message().contains("already"));
    window->createCourse("CS4433", "Again", departmentId("Computer Science"));
    EXPECT_TRUE(message().contains("already"));
    window->createCourse("XX1", "Title", 999);
    EXPECT_FALSE(message().isEmpty());
    EXPECT_EQ(courses()->rowCount(), 2);
}

TEST_F(AdminWindowTest, BlankCourseDataIsRefused) {
    window->createCourse("", "Title", departmentId("Computer Science"));
    EXPECT_FALSE(message().isEmpty());
    EXPECT_EQ(courses()->rowCount(), 2);
}

// ---- Assign and enrol (FR5) ----
TEST_F(AdminWindowTest, AssignALecturerAndEnrolAStudent) {
    window->createUser(Role::Lecturer, person("lecturer2", "Lecturer Two"), "Passw0rd1");
    const int cs3300 = ctx.adminView().courses(*admin)[0].id;

    window->assignLecturer(cs3300, idOf("lecturer2"));
    EXPECT_TRUE(message().contains("assigned"));
    EXPECT_EQ(courses()->item(0, 3)->text(), "Lecturer One, Lecturer Two");

    window->enrolStudent(cs3300, idOf("student2"));
    EXPECT_TRUE(message().contains("enrolled"));
    EXPECT_EQ(courses()->item(0, 4)->text(), "2");
}

TEST_F(AdminWindowTest, TheWrongKindOfUserCannotBeAssignedOrEnrolled) {
    const int cs3300 = ctx.adminView().courses(*admin)[0].id;
    window->assignLecturer(cs3300, idOf("student1"));
    EXPECT_TRUE(message().contains("lecturer"));
    window->enrolStudent(cs3300, idOf("lecturer1"));
    EXPECT_FALSE(message().isEmpty());
    EXPECT_EQ(courses()->item(0, 3)->text(), "Lecturer One");
    EXPECT_EQ(courses()->item(0, 4)->text(), "1");
}

// ---- Audit log (FR19) ----
TEST_F(AdminWindowTest, AuditTabShowsMarkChangesWithNames) {
    EXPECT_EQ(audit()->rowCount(), 0);
    auto student = ctx.auth().login("student1", kDemoPassword);
    auto lecturer = ctx.auth().login("lecturer1", kDemoPassword);
    const auto list = ctx.studentView().myCourses(*student);  // CS3300, CS4433
    const int assignment = ctx.studentView().myAssignments(*student, list[1].id())[0].assignmentId;
    const int id = ctx.submissions().submit(*student, assignment, "work.pdf").submission.id();
    ctx.submissions().mark(*lecturer, id, 80.0, "Good work");

    window->refresh();
    ASSERT_EQ(audit()->rowCount(), 1);
    EXPECT_EQ(audit()->item(0, 2)->text(), "Lecturer One");
    EXPECT_EQ(audit()->item(0, 3)->text(), "(first mark)");
    EXPECT_EQ(audit()->item(0, 4)->text(), "80");
    EXPECT_EQ(audit()->item(0, 5)->text(), "Initial mark");
}

// ---- Buttons follow the selection ----
TEST_F(AdminWindowTest, UserButtonsFollowTheSelection) {
    EXPECT_FALSE(button("toggleButton")->isEnabled());
    EXPECT_FALSE(button("resetButton")->isEnabled());
    users()->selectRow(0);
    EXPECT_TRUE(button("toggleButton")->isEnabled());
    EXPECT_TRUE(button("resetButton")->isEnabled());
    EXPECT_TRUE(button("toggleButton")->text().contains("Deactivate"));

    window->setUserActive(idOf("student1"), false);
    users()->selectRow(3);
    EXPECT_TRUE(button("toggleButton")->text().contains("Reactivate"));
}

TEST_F(AdminWindowTest, CourseButtonsNeedASelectedCourse) {
    EXPECT_TRUE(button("newDepartmentButton")->isEnabled());
    EXPECT_FALSE(button("assignButton")->isEnabled());
    EXPECT_FALSE(button("enrolButton")->isEnabled());
    courses()->selectRow(0);
    EXPECT_TRUE(button("assignButton")->isEnabled());
    EXPECT_TRUE(button("enrolButton")->isEnabled());
}

TEST_F(AdminWindowTest, LogOutButtonSendsTheSignal) {
    int count = 0;
    QObject::connect(window.get(), &UserWindow::loggedOut, [&count]() { ++count; });
    button("logoutButton")->click();
    EXPECT_EQ(count, 1);
}

// ---- The forms ----
TEST(AdminDialogsTest, NewUserFormChecksTheFieldsOfEachRole) {
    std::vector<DepartmentInfo> departments;
    DepartmentInfo cs;
    cs.id = 7;
    cs.name = "Computer Science";
    departments.push_back(cs);

    NewUserDialog student(departments);
    student.findChild<QLineEdit*>("usernameEdit")->setText("stu9");
    student.findChild<QLineEdit*>("fullNameEdit")->setText("Student Nine");
    student.findChild<QLineEdit*>("passwordEdit")->setText("Passw0rd1");
    student.findChild<QLineEdit*>("confirmEdit")->setText("Passw0rd1");
    okButton(student)->click();
    EXPECT_NE(student.result(), QDialog::Accepted);  // the student number is missing
    EXPECT_FALSE(dialogError(student).isEmpty());
    student.findChild<QLineEdit*>("studentNumberEdit")->setText("S009");
    okButton(student)->click();
    EXPECT_EQ(student.result(), QDialog::Accepted);
    EXPECT_EQ(student.role(), Role::Student);
    EXPECT_EQ(student.userData().studentNumber, "S009");
    EXPECT_EQ(student.password(), "Passw0rd1");

    NewUserDialog head(departments);
    head.findChild<QComboBox*>("roleCombo")->setCurrentIndex(2);  // Head of Department
    EXPECT_EQ(head.role(), Role::HoD);
    EXPECT_FALSE(head.findChild<QLineEdit*>("studentNumberEdit")->isEnabled());
    EXPECT_TRUE(head.findChild<QComboBox*>("departmentCombo")->isEnabled());
    head.findChild<QLineEdit*>("usernameEdit")->setText("hod9");
    head.findChild<QLineEdit*>("fullNameEdit")->setText("Head Nine");
    head.findChild<QLineEdit*>("passwordEdit")->setText("Passw0rd1");
    head.findChild<QLineEdit*>("confirmEdit")->setText("Passw0rd1");
    okButton(head)->click();
    EXPECT_EQ(head.result(), QDialog::Accepted);
    EXPECT_EQ(head.userData().departmentId, 7);
}

TEST(AdminDialogsTest, NewUserFormRefusesDifferentPasswordsAndMissingNames) {
    NewUserDialog dialog({});
    okButton(dialog)->click();
    EXPECT_NE(dialog.result(), QDialog::Accepted);

    dialog.findChild<QComboBox*>("roleCombo")->setCurrentIndex(3);  // Administrator
    dialog.findChild<QLineEdit*>("usernameEdit")->setText("boss");
    dialog.findChild<QLineEdit*>("fullNameEdit")->setText("The Boss");
    dialog.findChild<QLineEdit*>("passwordEdit")->setText("Passw0rd1");
    dialog.findChild<QLineEdit*>("confirmEdit")->setText("Different1");
    okButton(dialog)->click();
    EXPECT_TRUE(dialogError(dialog).contains("not the same"));
    EXPECT_NE(dialog.result(), QDialog::Accepted);
}

TEST(AdminDialogsTest, HeadOfDepartmentNeedsADepartment) {
    NewUserDialog dialog({});
    dialog.findChild<QComboBox*>("roleCombo")->setCurrentIndex(2);
    dialog.findChild<QLineEdit*>("usernameEdit")->setText("hod9");
    dialog.findChild<QLineEdit*>("fullNameEdit")->setText("Head Nine");
    okButton(dialog)->click();
    EXPECT_TRUE(dialogError(dialog).contains("department"));
    EXPECT_NE(dialog.result(), QDialog::Accepted);
}

TEST(AdminDialogsTest, PasswordFormNeedsTwoEqualPasswords) {
    PasswordDialog dialog("student1");
    okButton(dialog)->click();
    EXPECT_NE(dialog.result(), QDialog::Accepted);
    dialog.findChild<QLineEdit*>("passwordEdit")->setText("Passw0rd1");
    dialog.findChild<QLineEdit*>("confirmEdit")->setText("Passw0rd2");
    okButton(dialog)->click();
    EXPECT_TRUE(dialogError(dialog).contains("not the same"));
    dialog.findChild<QLineEdit*>("confirmEdit")->setText("Passw0rd1");
    okButton(dialog)->click();
    EXPECT_EQ(dialog.result(), QDialog::Accepted);
    EXPECT_EQ(dialog.password(), "Passw0rd1");
}

TEST(AdminDialogsTest, TextFormNeedsAValue) {
    TextDialog dialog("New department", "&Name:");
    okButton(dialog)->click();
    EXPECT_NE(dialog.result(), QDialog::Accepted);
    dialog.findChild<QLineEdit*>("textEdit")->setText("  Mathematics  ");
    okButton(dialog)->click();
    EXPECT_EQ(dialog.result(), QDialog::Accepted);
    EXPECT_EQ(dialog.text(), "Mathematics");
}

TEST(AdminDialogsTest, CourseFormNeedsCodeTitleAndADepartment) {
    CourseDialog none({});
    none.findChild<QLineEdit*>("codeEdit")->setText("MA1");
    none.findChild<QLineEdit*>("titleEdit")->setText("Calculus");
    okButton(none)->click();
    EXPECT_TRUE(dialogError(none).contains("department"));

    DepartmentInfo maths;
    maths.id = 3;
    maths.name = "Mathematics";
    CourseDialog dialog({maths});
    okButton(dialog)->click();
    EXPECT_NE(dialog.result(), QDialog::Accepted);
    dialog.findChild<QLineEdit*>("codeEdit")->setText("MA1000");
    dialog.findChild<QLineEdit*>("titleEdit")->setText("Calculus");
    okButton(dialog)->click();
    EXPECT_EQ(dialog.result(), QDialog::Accepted);
    EXPECT_EQ(dialog.code(), "MA1000");
    EXPECT_EQ(dialog.departmentId(), 3);
}

TEST(AdminDialogsTest, ChoiceFormNeedsSomethingToChoose) {
    ChoiceDialog empty("Assign a lecturer", "&Lecturer:", {});
    okButton(empty)->click();
    EXPECT_NE(empty.result(), QDialog::Accepted);
    EXPECT_FALSE(dialogError(empty).isEmpty());

    ChoiceDialog dialog("Assign a lecturer", "&Lecturer:", {{4, "Lecturer One"}, {9, "Lecturer Two"}});
    dialog.findChild<QComboBox*>("choiceCombo")->setCurrentIndex(1);
    okButton(dialog)->click();
    EXPECT_EQ(dialog.result(), QDialog::Accepted);
    EXPECT_EQ(dialog.selectedId(), 9);
}
