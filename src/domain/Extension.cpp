#include "Extension.h"

#include <stdexcept>
#include <utility>

namespace lms::domain {

Extension::Extension(int id, int assignmentId, int studentId, TimePoint newDeadline,
                     std::string reason)
    : id_(id),
      assignmentId_(assignmentId),
      studentId_(studentId),
      newDeadline_(newDeadline),
      reason_(std::move(reason)) {
    if (id_ < 0) {
        throw std::invalid_argument("id must not be negative");
    }
    if (assignmentId_ <= 0 || studentId_ <= 0) {
        throw std::invalid_argument("an extension needs an assignment and a student");
    }
    if (reason_.find_first_not_of(" \t\r\n") == std::string::npos) {
        throw std::invalid_argument("an extension needs a reason");
    }
}

}  // namespace lms::domain
