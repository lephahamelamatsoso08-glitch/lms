#include "AuditService.h"

#include <chrono>
#include <utility>

#include "Action.h"
#include "MarkAuditEntry.h"

namespace lms::application {

using lms::domain::Action;
using lms::domain::MarkAuditEntry;
using lms::domain::TimePoint;
using lms::domain::User;

AuditService::AuditService(lms::domain::IAuditRepository& repo, Clock clock)
    : repo_(repo), clock_(std::move(clock)) {
    if (!clock_) {
        clock_ = &AuditService::systemNow;
    }
}

TimePoint AuditService::systemNow() { return std::chrono::system_clock::now(); }

void AuditService::requireViewer(const User& viewer) {
    if (!viewer.canPerform(Action::ViewAuditLog)) {
        throw AccessDeniedError("You are not allowed to view the audit log.");
    }
}

int AuditService::recordMarkChange(int submissionId, int changedBy, std::optional<double> oldValue,
                                   double newValue, const std::string& reason) {
    // The entry checks its own data, so an invalid change is never saved.
    const MarkAuditEntry entry(0, submissionId, changedBy, clock_(), oldValue, newValue, reason);
    return repo_.add(entry);
}

std::vector<MarkAuditEntry> AuditService::history(const User& viewer, int submissionId) const {
    requireViewer(viewer);
    return repo_.findBySubmission(submissionId);
}

std::vector<MarkAuditEntry> AuditService::all(const User& viewer) const {
    requireViewer(viewer);
    return repo_.findAll();
}

}  // namespace lms::application
