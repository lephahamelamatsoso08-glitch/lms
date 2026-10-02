#pragma once

#include <string>

#include "IPasswordHasher.h"

namespace lms::data {

// Password hashing with Argon2id from libsodium (NFR3). Each hash has its
// own random salt and carries its own settings, so they can be upgraded later.
class Argon2Hasher : public lms::domain::IPasswordHasher {
public:
    Argon2Hasher();

    std::string hash(const std::string& password) const override;
    bool verify(const std::string& password, const std::string& storedHash) const override;
};

}  // namespace lms::data
