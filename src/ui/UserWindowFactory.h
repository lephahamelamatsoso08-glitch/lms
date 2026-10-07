#pragma once

#include <memory>

#include "AppContext.h"
#include "User.h"
#include "UserWindow.h"

namespace lms::ui {

// Chooses the window for the role of the logged-in user.
std::unique_ptr<UserWindow> makeUserWindow(lms::app::AppContext& context,
                                           std::shared_ptr<lms::domain::User> user);

}  // namespace lms::ui
