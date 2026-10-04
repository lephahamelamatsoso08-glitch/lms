#pragma once

#include <memory>

#include <QMainWindow>
#include <QString>

#include "AppContext.h"
#include "Role.h"
#include "User.h"

namespace lms::ui {

// A temporary page shown after login. The real pages for each role replace it
// in the next steps.
class RoleWindow : public QMainWindow {
    Q_OBJECT

public:
    RoleWindow(lms::app::AppContext& context, std::shared_ptr<lms::domain::User> user,
               QWidget* parent = nullptr);

    static QString roleLabel(lms::domain::Role role);

signals:
    void loggedOut();

private:
    lms::app::AppContext& context_;
    std::shared_ptr<lms::domain::User> user_;
};

}  // namespace lms::ui
