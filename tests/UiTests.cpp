#include <gtest/gtest.h>

#include <memory>

#include <QApplication>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>

#include "AppContext.h"
#include "FirstRunDialog.h"
#include "LoginWindow.h"
#include "RoleWindow.h"
#include "UserFactory.h"

using lms::app::AppContext;
using namespace lms::domain;
using namespace lms::ui;

namespace {

template <class T>
T* child(QWidget* parent, const char* name) {
    T* found = parent->findChild<T*>(name);
    EXPECT_NE(found, nullptr) << "missing widget: " << name;
    return found;
}

void type(QWidget* parent, const char* name, const QString& text) {
    child<QLineEdit>(parent, name)->setText(text);
}

void click(QWidget* parent, const char* name) { child<QPushButton>(parent, name)->click(); }

QString message(QWidget* parent) { return child<QLabel>(parent, "errorLabel")->text(); }

void fillFirstRun(QDialog& dialog, const QString& user, const QString& name, const QString& pw,
                  const QString& confirm) {
    type(&dialog, "usernameEdit", user);
    type(&dialog, "fullNameEdit", name);
    type(&dialog, "passwordEdit", pw);
    type(&dialog, "confirmEdit", confirm);
}

}  // namespace

// ---- First start ----
TEST(FirstRunDialogTest, CreatesTheAdministratorAndAccepts) {
    AppContext ctx(":memory:");
    FirstRunDialog dialog(ctx);
    fillFirstRun(dialog, "admin", "System Admin", "Passw0rd1", "Passw0rd1");
    click(&dialog, "createButton");
    EXPECT_EQ(dialog.result(), QDialog::Accepted);
    EXPECT_FALSE(ctx.needsFirstAdmin());
    EXPECT_NE(ctx.auth().login("admin", "Passw0rd1"), nullptr);
}

TEST(FirstRunDialogTest, PasswordsThatDoNotMatchAreRefused) {
    AppContext ctx(":memory:");
    FirstRunDialog dialog(ctx);
    fillFirstRun(dialog, "admin", "System Admin", "Passw0rd1", "Different1");
    click(&dialog, "createButton");
    EXPECT_NE(dialog.result(), QDialog::Accepted);
    EXPECT_TRUE(ctx.needsFirstAdmin());
    EXPECT_FALSE(message(&dialog).isEmpty());
}

TEST(FirstRunDialogTest, WeakPasswordShowsTheRule) {
    AppContext ctx(":memory:");
    FirstRunDialog dialog(ctx);
    fillFirstRun(dialog, "admin", "System Admin", "weak", "weak");
    click(&dialog, "createButton");
    EXPECT_TRUE(message(&dialog).contains("8 characters"));
    EXPECT_TRUE(ctx.needsFirstAdmin());
}

TEST(FirstRunDialogTest, MissingNamesAreRefused) {
    AppContext ctx(":memory:");
    FirstRunDialog dialog(ctx);
    fillFirstRun(dialog, "", "System Admin", "Passw0rd1", "Passw0rd1");
    click(&dialog, "createButton");
    EXPECT_FALSE(message(&dialog).isEmpty());
    fillFirstRun(dialog, "admin", "", "Passw0rd1", "Passw0rd1");
    click(&dialog, "createButton");
    EXPECT_FALSE(message(&dialog).isEmpty());
    EXPECT_TRUE(ctx.needsFirstAdmin());
}

TEST(FirstRunDialogTest, CanCorrectAMistakeAndTryAgain) {
    AppContext ctx(":memory:");
    FirstRunDialog dialog(ctx);
    fillFirstRun(dialog, "admin", "System Admin", "weak", "weak");
    click(&dialog, "createButton");
    EXPECT_FALSE(message(&dialog).isEmpty());
    fillFirstRun(dialog, "admin", "System Admin", "Passw0rd1", "Passw0rd1");
    click(&dialog, "createButton");
    EXPECT_EQ(dialog.result(), QDialog::Accepted);
}

TEST(FirstRunDialogTest, PasswordFieldsHideTheText) {
    AppContext ctx(":memory:");
    FirstRunDialog dialog(ctx);
    EXPECT_EQ(child<QLineEdit>(&dialog, "passwordEdit")->echoMode(), QLineEdit::Password);
    EXPECT_EQ(child<QLineEdit>(&dialog, "confirmEdit")->echoMode(), QLineEdit::Password);
}

// ---- Login ----
class LoginWindowTest : public ::testing::Test {
protected:
    LoginWindowTest() : ctx(":memory:"), window(ctx) {
        ctx.createFirstAdmin("admin", "System Admin", "Passw0rd1");
        QObject::connect(&window, &LoginWindow::loggedIn,
                         [this](std::shared_ptr<User> u) {
                             ++signalCount;
                             user = std::move(u);
                         });
    }

    void tryLogin(const QString& username, const QString& password) {
        type(&window, "usernameEdit", username);
        type(&window, "passwordEdit", password);
        click(&window, "loginButton");
    }

    AppContext ctx;
    LoginWindow window;
    std::shared_ptr<User> user;
    int signalCount = 0;
};

TEST_F(LoginWindowTest, CorrectLoginGivesTheUserAndClearsTheForm) {
    tryLogin("admin", "Passw0rd1");
    EXPECT_EQ(signalCount, 1);
    ASSERT_NE(user, nullptr);
    EXPECT_EQ(user->getRole(), Role::Admin);
    EXPECT_TRUE(child<QLineEdit>(&window, "passwordEdit")->text().isEmpty());
    EXPECT_TRUE(child<QLineEdit>(&window, "usernameEdit")->text().isEmpty());
    EXPECT_TRUE(message(&window).isEmpty());
}

TEST_F(LoginWindowTest, WrongPasswordShowsAMessageAndKeepsTheUsername) {
    tryLogin("admin", "Wrong123");
    EXPECT_EQ(signalCount, 0);
    EXPECT_EQ(message(&window), "Invalid username or password.");
    EXPECT_TRUE(child<QLineEdit>(&window, "passwordEdit")->text().isEmpty());
    EXPECT_EQ(child<QLineEdit>(&window, "usernameEdit")->text(), "admin");
}

TEST_F(LoginWindowTest, UnknownUserGetsTheSameMessage) {
    tryLogin("nobody", "Wrong123");
    EXPECT_EQ(signalCount, 0);
    EXPECT_EQ(message(&window), "Invalid username or password.");
}

TEST_F(LoginWindowTest, EmptyFieldsShowAMessage) {
    click(&window, "loginButton");
    EXPECT_EQ(signalCount, 0);
    EXPECT_FALSE(message(&window).isEmpty());
}

TEST_F(LoginWindowTest, SpacesAroundTheUsernameAreIgnored) {
    tryLogin("  admin  ", "Passw0rd1");
    EXPECT_EQ(signalCount, 1);
}

TEST_F(LoginWindowTest, DeactivatedAccountIsToldWhy) {
    auto admin = ctx.auth().login("admin", "Passw0rd1");
    UserData d;
    d.username = "stu1";
    d.fullName = "Student One";
    d.studentNumber = "S001";
    const int id = ctx.admin().createUser(*admin, Role::Student, d, "Passw0rd1");
    ctx.admin().deactivateUser(*admin, id);

    tryLogin("stu1", "Passw0rd1");
    EXPECT_EQ(signalCount, 0);
    EXPECT_TRUE(message(&window).contains("deactivated"));
}

TEST_F(LoginWindowTest, AFailedLoginCanBeFollowedByASuccessfulOne) {
    tryLogin("admin", "Wrong123");
    EXPECT_FALSE(message(&window).isEmpty());
    tryLogin("admin", "Passw0rd1");
    EXPECT_EQ(signalCount, 1);
    EXPECT_TRUE(message(&window).isEmpty());
}

TEST_F(LoginWindowTest, EnterKeyInThePasswordFieldLogsIn) {
    type(&window, "usernameEdit", "admin");
    type(&window, "passwordEdit", "Passw0rd1");
    emit child<QLineEdit>(&window, "passwordEdit")->returnPressed();
    EXPECT_EQ(signalCount, 1);
}

TEST_F(LoginWindowTest, PasswordFieldHidesTheText) {
    EXPECT_EQ(child<QLineEdit>(&window, "passwordEdit")->echoMode(), QLineEdit::Password);
}

// ---- Page after login ----
TEST(RoleWindowTest, ShowsTheNameAndTheRole) {
    AppContext ctx(":memory:");
    UserData d;
    d.id = 5;
    d.username = "stu1";
    d.fullName = "Student One";
    d.studentNumber = "S001";
    std::shared_ptr<User> user = UserFactory::create(Role::Student, d);
    RoleWindow window(ctx, user);
    const QString text = child<QLabel>(&window, "welcomeLabel")->text();
    EXPECT_TRUE(text.contains("Student One"));
    EXPECT_TRUE(text.contains("Student"));
}

TEST(RoleWindowTest, EveryRoleHasALabel) {
    EXPECT_EQ(RoleWindow::roleLabel(Role::Admin), "Administrator");
    EXPECT_EQ(RoleWindow::roleLabel(Role::Lecturer), "Lecturer");
    EXPECT_EQ(RoleWindow::roleLabel(Role::HoD), "Head of Department");
    EXPECT_EQ(RoleWindow::roleLabel(Role::Student), "Student");
}

TEST(RoleWindowTest, LogOutButtonSendsTheSignal) {
    AppContext ctx(":memory:");
    UserData d;
    d.username = "admin";
    d.fullName = "System Admin";
    std::shared_ptr<User> user = UserFactory::create(Role::Admin, d);
    RoleWindow window(ctx, user);
    int count = 0;
    QObject::connect(&window, &RoleWindow::loggedOut, [&count]() { ++count; });
    click(&window, "logoutButton");
    EXPECT_EQ(count, 1);
}

int main(int argc, char** argv) {
    qputenv("QT_QPA_PLATFORM", "offscreen");  // no screen needed, also works in CI
    QApplication app(argc, argv);
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
