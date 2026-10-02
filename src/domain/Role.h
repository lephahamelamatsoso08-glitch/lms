#pragma once

#include <string>

namespace lms::domain {

enum class Role { Admin, Lecturer, HoD, Student };

// The text form is what the database stores in user_account.role.
std::string toString(Role role);
Role roleFromString(const std::string& text);

}  // namespace lms::domain
