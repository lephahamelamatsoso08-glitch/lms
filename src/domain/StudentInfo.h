#pragma once

#include <string>

namespace lms::domain {

// A student as the lecturer sees them in a list.
struct StudentInfo {
    int id = 0;
    std::string name;
    std::string number;
};

}  // namespace lms::domain
