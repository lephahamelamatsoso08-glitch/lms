#pragma once

#include <string>

namespace lms::domain {

// Interface for password hashing, so the domain and services do not depend
// on a specific library (dependency inversion).
class IPasswordHasher {
public:
    virtual ~IPasswordHasher() = default;

    // Returns a salted hash. The salt is random and stored inside the result.
    virtual std::string hash(const std::string& password) const = 0;

    // True only if the password matches the stored hash.
    virtual bool verify(const std::string& password, const std::string& storedHash) const = 0;
};

}  // namespace lms::domain
