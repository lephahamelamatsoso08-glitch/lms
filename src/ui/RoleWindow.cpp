#include "RoleWindow.h"

#include <utility>

#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <QWidget>

namespace lms::ui {

using lms::domain::Role;

QString RoleWindow::roleLabel(Role role) {
    switch (role) {
        case Role::Admin:    return "Administrator";
        case Role::Lecturer: return "Lecturer";
        case Role::HoD:      return "Head of Department";
        case Role::Student:  return "Student";
    }
    return "User";
}

RoleWindow::RoleWindow(lms::app::AppContext& context, std::shared_ptr<lms::domain::User> user,
                       QWidget* parent)
    : UserWindow(parent), context_(context), user_(std::move(user)) {
    const QString role = roleLabel(user_->getRole());
    setWindowTitle("Moderated LMS - " + role);

    auto* welcome = new QLabel(
        "Logged in as " + QString::fromStdString(user_->fullName()) + " (" + role + ")", this);
    welcome->setObjectName("welcomeLabel");

    auto* logout = new QPushButton("Log out", this);
    logout->setObjectName("logoutButton");
    connect(logout, &QPushButton::clicked, this, &UserWindow::loggedOut);

    auto* centre = new QWidget(this);
    auto* layout = new QVBoxLayout(centre);
    layout->addWidget(welcome);
    layout->addStretch();
    layout->addWidget(logout);
    setCentralWidget(centre);
    resize(480, 240);
}

}  // namespace lms::ui
