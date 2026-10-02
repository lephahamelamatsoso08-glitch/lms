#pragma once

#include <string>

#include "Action.h"
#include "Role.h"

namespace lms::domain {

// Abstract base class for all users (FR1, FR2). Each role decides what it
// may do (polymorphism). Copying is disabled so a user is never sliced.
class User {
public:
    User(int id, std::string username, std::string fullName, bool active);
    virtual ~User() = default;

    User(const User&) = delete;
    User& operator=(const User&) = delete;

    int id() const { return id_; }
    const std::string& username() const { return username_; }
    const std::string& fullName() const { return fullName_; }
    bool isActive() const { return active_; }

    virtual Role getRole() const = 0;
    virtual bool canPerform(Action action) const = 0;

protected:
    int id_;  // 0 means "not saved yet"
    std::string username_;
    std::string fullName_;
    bool active_;
};

}  // namespace lms::domain
