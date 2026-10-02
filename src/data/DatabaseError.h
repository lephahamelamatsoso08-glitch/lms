#pragma once

#include <stdexcept>

namespace lms::data {

// Thrown for any database problem: cannot open, bad SQL, constraint failure.
class DatabaseError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

}  // namespace lms::data
