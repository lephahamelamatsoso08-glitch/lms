#pragma once

#include <string>

namespace lms::domain {

// A person in a list (for example a lecturer of a course).
struct PersonInfo {
    int id = 0;
    std::string name;
};

}  // namespace lms::domain
