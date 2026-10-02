#include "Assignment.h"

#include <stdexcept>
#include <utility>

namespace lms::domain {

Assignment::Assignment(int id, int courseId, std::string title, TimePoint deadline,
                       double maxMark)
    : id_(id),
      courseId_(courseId),
      title_(std::move(title)),
      deadline_(deadline),
      maxMark_(maxMark) {
    if (id_ < 0) {
        throw std::invalid_argument("id must not be negative");
    }
    if (courseId_ <= 0) {
        throw std::invalid_argument("an assignment belongs to a course");
    }
    if (title_.empty()) {
        throw std::invalid_argument("title must not be empty");
    }
    if (maxMark_ <= 0.0) {
        throw std::invalid_argument("maxMark must be greater than 0");
    }
}

TimePoint Assignment::deadlineFor(const std::optional<Extension>& extension) const {
    if (!extension) {
        return deadline_;
    }
    if (extension->assignmentId() != id_) {
        throw std::invalid_argument("extension belongs to a different assignment");
    }
    return extension->newDeadline();
}

Extension Assignment::grantExtension(int studentId, TimePoint newDeadline,
                                     const std::string& reason) const {
    if (newDeadline <= deadline_) {
        throw std::invalid_argument("an extension must be later than the normal deadline");
    }
    return Extension(0, id_, studentId, newDeadline, reason);
}

}  // namespace lms::domain
