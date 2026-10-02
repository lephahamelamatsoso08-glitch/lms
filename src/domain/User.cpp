#include "User.h"

#include <stdexcept>
#include <utility>

namespace lms::domain {

User::User(int id, std::string username, std::string fullName, bool active)
    : id_(id),
      username_(std::move(username)),
      fullName_(std::move(fullName)),
      active_(active) {
    if (id_ < 0) {
        throw std::invalid_argument("id must not be negative");
    }
    if (username_.empty()) {
        throw std::invalid_argument("username must not be empty");
    }
    if (fullName_.empty()) {
        throw std::invalid_argument("full name must not be empty");
    }
}

}  // namespace lms::domain
