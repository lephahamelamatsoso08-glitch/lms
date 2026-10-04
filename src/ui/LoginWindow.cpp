#include "LoginWindow.h"

#include <exception>
#include <utility>

#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>

#include "AuthService.h"

namespace lms::ui {

LoginWindow::LoginWindow(lms::app::AppContext& context, QWidget* parent)
    : QWidget(parent), context_(context) {
    setWindowTitle("Moderated LMS - Log in");

    auto* title = new QLabel("Moderated Learning Management System", this);
    QFont titleFont = title->font();
    titleFont.setPointSize(titleFont.pointSize() + 4);
    titleFont.setBold(true);
    title->setFont(titleFont);

    usernameEdit_ = new QLineEdit(this);
    usernameEdit_->setObjectName("usernameEdit");
    passwordEdit_ = new QLineEdit(this);
    passwordEdit_->setObjectName("passwordEdit");
    passwordEdit_->setEchoMode(QLineEdit::Password);

    loginButton_ = new QPushButton("Log in", this);
    loginButton_->setObjectName("loginButton");

    errorLabel_ = new QLabel(this);
    errorLabel_->setObjectName("errorLabel");
    errorLabel_->setWordWrap(true);
    errorLabel_->setStyleSheet("color: #b00020;");

    auto* form = new QFormLayout();
    form->addRow("&Username:", usernameEdit_);  // Alt+U jumps here (keyboard access)
    form->addRow("&Password:", passwordEdit_);  // Alt+P jumps here

    auto* layout = new QVBoxLayout(this);
    layout->addWidget(title);
    layout->addLayout(form);
    layout->addWidget(errorLabel_);
    layout->addWidget(loginButton_);

    setTabOrder(usernameEdit_, passwordEdit_);
    setTabOrder(passwordEdit_, loginButton_);

    connect(loginButton_, &QPushButton::clicked, this, &LoginWindow::attemptLogin);
    connect(usernameEdit_, &QLineEdit::returnPressed, this, &LoginWindow::attemptLogin);
    connect(passwordEdit_, &QLineEdit::returnPressed, this, &LoginWindow::attemptLogin);
}

void LoginWindow::reset() {
    usernameEdit_->clear();
    passwordEdit_->clear();
    errorLabel_->clear();
    usernameEdit_->setFocus();
}

void LoginWindow::attemptLogin() {
    errorLabel_->clear();
    try {
        std::unique_ptr<lms::domain::User> user = context_.auth().login(
            usernameEdit_->text().trimmed().toStdString(), passwordEdit_->text().toStdString());
        std::shared_ptr<lms::domain::User> shared(std::move(user));
        reset();
        emit loggedIn(shared);
    } catch (const lms::application::AuthenticationError& error) {
        errorLabel_->setText(QString::fromUtf8(error.what()));
        passwordEdit_->clear();
        passwordEdit_->setFocus();
    } catch (const std::exception&) {
        errorLabel_->setText("Something went wrong. Please try again.");
        passwordEdit_->clear();
    }
}

}  // namespace lms::ui
