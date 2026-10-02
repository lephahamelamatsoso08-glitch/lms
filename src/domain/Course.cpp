#include "Course.h"

#include <stdexcept>
#include <utility>

namespace lms::domain {

Course::Course(int id, std::string code, std::string title, int departmentId)
    : id_(id), code_(std::move(code)), title_(std::move(title)), departmentId_(departmentId) {
    if (id_ < 0) {
        throw std::invalid_argument("id must not be negative");
    }
    if (code_.empty() || title_.empty()) {
        throw std::invalid_argument("a course needs a code and a title");
    }
    if (departmentId_ <= 0) {
        throw std::invalid_argument("a course belongs to a department");
    }
}

}  // namespace lms::domain
