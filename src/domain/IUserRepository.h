#pragma once

#include <memory>
#include <string>

#include "User.h"

namespace lms::domain {

// Interface for storing users (design pattern: Repository). The domain does
// not know about SQLite. The password hash is passed in and out as text: the
// User class never holds a password or a hash.
class IUserRepository {
public:
    virtual ~IUserRepository() = default;

    // Saves a new user (its id must be 0) and returns the new id.
    virtual int add(const User& user, const std::string& passwordHash) = 0;

    // Return nullptr when no such user exists.
    virtual std::unique_ptr<User> findById(int id) const = 0;
    virtual std::unique_ptr<User> findByUsername(const std::string& username) const = 0;

    // Returns "" when no such user exists.
    virtual std::string findPasswordHash(const std::string& username) const = 0;

    virtual void setPasswordHash(int userId, const std::string& passwordHash) = 0;  // FR20
    virtual void setActive(int userId, bool active) = 0;                            // FR3
};

}  // namespace lms::domain
