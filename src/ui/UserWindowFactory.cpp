#include "UserWindowFactory.h"

#include <utility>

#include "LecturerWindow.h"
#include "RoleWindow.h"
#include "StudentWindow.h"

namespace lms::ui {

std::unique_ptr<UserWindow> makeUserWindow(lms::app::AppContext& context,
                                           std::shared_ptr<lms::domain::User> user) {
    switch (user->getRole()) {
        case lms::domain::Role::Student:
            return std::make_unique<StudentWindow>(context, std::move(user));
        case lms::domain::Role::Lecturer:
            return std::make_unique<LecturerWindow>(context, std::move(user));
        // The other roles get their own windows in the next steps.
        case lms::domain::Role::Admin:
        case lms::domain::Role::HoD:
            break;
    }
    return std::make_unique<RoleWindow>(context, std::move(user));
}

}  // namespace lms::ui
