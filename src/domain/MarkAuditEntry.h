#pragma once

#include <optional>
#include <string>

#include "LateDays.h"

namespace lms::domain {

// One line of the audit log (FR15): who changed a mark, when, from what
// value to what value, and why. oldValue is empty for the first mark.
class MarkAuditEntry {
public:
    MarkAuditEntry(int id, int submissionId, int changedBy, TimePoint changedAt,
                   std::optional<double> oldValue, double newValue, std::string reason);

    int id() const { return id_; }  // 0 means "not saved yet"
    int submissionId() const { return submissionId_; }
    int changedBy() const { return changedBy_; }
    TimePoint changedAt() const { return changedAt_; }
    const std::optional<double>& oldValue() const { return oldValue_; }
    double newValue() const { return newValue_; }
    const std::string& reason() const { return reason_; }

private:
    int id_;
    int submissionId_;
    int changedBy_;
    TimePoint changedAt_;
    std::optional<double> oldValue_;
    double newValue_;
    std::string reason_;
};

}  // namespace lms::domain
