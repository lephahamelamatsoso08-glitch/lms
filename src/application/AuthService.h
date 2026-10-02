#pragma once

#include <memory>
#include <stdexcept>
#include <string>

#include "IPasswordHasher.h"
#include "IUserRepository.h"

namespace lms::application {

// Thrown when a login fails. The message is safe to show to the user.
class AuthenticationError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

// Login (FR1). Depends only on interfaces (dependency inversion).
class AuthService {
public:
    AuthService(const lms::domain::IUserRepository& users,
                const lms::domain::IPasswordHasher& hasher);

    // Returns the logged-in user, or throws AuthenticationError.
    std::unique_ptr<lms::domain::User> login(const std::string& username,
                                             const std::string& password) const;

private:
    const lms::domain::IUserRepository& users_;
    const lms::domain::IPasswordHasher& hasher_;
    std::string dummyHash_;  // checked when the username does not exist
};

}  // namespace lms::application
