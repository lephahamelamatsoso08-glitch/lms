#pragma once

#include <vector>

#include "MarkAuditEntry.h"

namespace lms::domain {

// The audit log can only be added to. There is deliberately no update and no
// delete method, so history cannot be changed through the application.
class IAuditRepository {
public:
    virtual ~IAuditRepository() = default;

    // Saves a new entry (its id must be 0) and returns the new id.
    virtual int add(const MarkAuditEntry& entry) = 0;

    // Oldest first.
    virtual std::vector<MarkAuditEntry> findBySubmission(int submissionId) const = 0;
    virtual std::vector<MarkAuditEntry> findAll() const = 0;
};

}  // namespace lms::domain
