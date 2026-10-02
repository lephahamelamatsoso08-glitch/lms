#include "MarkAuditEntry.h"

#include <stdexcept>
#include <utility>

namespace lms::domain {

MarkAuditEntry::MarkAuditEntry(int id, int submissionId, int changedBy, TimePoint changedAt,
                               std::optional<double> oldValue, double newValue,
                               std::string reason)
    : id_(id),
      submissionId_(submissionId),
      changedBy_(changedBy),
      changedAt_(changedAt),
      oldValue_(oldValue),
      newValue_(newValue),
      reason_(std::move(reason)) {
    if (id_ < 0) {
        throw std::invalid_argument("id must not be negative");
    }
    if (submissionId_ <= 0 || changedBy_ <= 0) {
        throw std::invalid_argument("an audit entry needs a submission and a user");
    }
    if (newValue_ < 0.0 || (oldValue_ && *oldValue_ < 0.0)) {
        throw std::invalid_argument("marks must not be negative");
    }
    if (reason_.find_first_not_of(" \t\r\n") == std::string::npos) {
        throw std::invalid_argument("a mark change needs a reason");
    }
}

}  // namespace lms::domain
