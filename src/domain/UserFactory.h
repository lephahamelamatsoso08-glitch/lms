#pragma once

#include <memory>
#include <string>

#include "Role.h"
#include "User.h"

namespace lms::domain {

// Data needed to build any kind of user. Fields for one role only are
// ignored by the other roles.
struct UserData {
    int id = 0;
    std::string username;
    std::string fullName;
    bool active = true;
    int departmentId = 0;         // HoD only
    std::string studentNumber;    // Student only
};

// Design pattern: Factory. One place decides which subclass to build.
class UserFactory {
public:
    static std::unique_ptr<User> create(Role role, const UserData& data);
};

}  // namespace lms::domain
