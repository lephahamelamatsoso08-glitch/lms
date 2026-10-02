#pragma once

#include <stdexcept>

namespace lms::application {

// Thrown when a user tries something their role does not allow (FR2).
class AccessDeniedError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

}  // namespace lms::application
