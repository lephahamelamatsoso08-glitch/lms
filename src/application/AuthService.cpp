#include "AuthService.h"

namespace lms::application {

namespace {
const char* const kGenericMessage = "Invalid username or password.";
}

AuthService::AuthService(const lms::domain::IUserRepository& users,
                         const lms::domain::IPasswordHasher& hasher)
    : users_(users), hasher_(hasher), dummyHash_(hasher.hash("not-a-real-password")) {}

std::unique_ptr<lms::domain::User> AuthService::login(const std::string& username,
                                                      const std::string& password) const {
    if (username.empty() || password.empty()) {
        throw AuthenticationError("Enter your username and password.");
    }

    const std::string storedHash = users_.findPasswordHash(username);
    if (storedHash.empty()) {
        // Do the same amount of work for an unknown user, so the response
        // time does not show which usernames exist.
        hasher_.verify(password, dummyHash_);
        throw AuthenticationError(kGenericMessage);
    }
    if (!hasher_.verify(password, storedHash)) {
        throw AuthenticationError(kGenericMessage);
    }

    std::unique_ptr<lms::domain::User> user = users_.findByUsername(username);
    if (!user) {
        throw AuthenticationError(kGenericMessage);
    }
    // Only said after the password was correct, so it reveals nothing to a guesser.
    if (!user->isActive()) {
        throw AuthenticationError("This account is deactivated. Contact the administrator.");
    }
    return user;
}

}  // namespace lms::application
