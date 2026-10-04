#pragma once

#include <memory>

#include <QWidget>

#include "AppContext.h"
#include "User.h"

class QLabel;
class QLineEdit;
class QPushButton;

namespace lms::ui {

// The login window (FR1). It contains no business rules: it calls AuthService and
// shows the message. Every error is caught here, so the program never crashes.
class LoginWindow : public QWidget {
    Q_OBJECT

public:
    explicit LoginWindow(lms::app::AppContext& context, QWidget* parent = nullptr);

    // Clears the fields and the message, and puts the cursor in the username field.
    void reset();

signals:
    void loggedIn(std::shared_ptr<lms::domain::User> user);

private slots:
    void attemptLogin();

private:
    lms::app::AppContext& context_;
    QLineEdit* usernameEdit_;
    QLineEdit* passwordEdit_;
    QPushButton* loginButton_;
    QLabel* errorLabel_;
};

}  // namespace lms::ui
