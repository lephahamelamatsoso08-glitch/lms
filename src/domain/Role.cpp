#include "Role.h"

#include <stdexcept>

namespace lms::domain {

std::string toString(Role role) {
    switch (role) {
        case Role::Admin:    return "ADMIN";
        case Role::Lecturer: return "LECTURER";
        case Role::HoD:      return "HOD";
        case Role::Student:  return "STUDENT";
    }
    throw std::invalid_argument("unknown role");
}

Role roleFromString(const std::string& text) {
    if (text == "ADMIN")    return Role::Admin;
    if (text == "LECTURER") return Role::Lecturer;
    if (text == "HOD")      return Role::HoD;
    if (text == "STUDENT")  return Role::Student;
    throw std::invalid_argument("unknown role: " + text);
}

}  // namespace lms::domain
