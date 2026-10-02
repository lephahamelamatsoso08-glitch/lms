#pragma once

#include <functional>
#include <optional>
#include <string>
#include <vector>

#include "AccessDeniedError.h"
#include "IAuditRepository.h"
#include "LateDays.h"
#include "User.h"

namespace lms::application {

// Records every mark change (FR15) and lets the HoD and Admin read the log (FR19).
class AuditService {
public:
    using Clock = std::function<lms::domain::TimePoint()>;

    // The clock is a parameter so tests can use a fixed time.
    explicit AuditService(lms::domain::IAuditRepository& repo, Clock clock = &systemNow);

    // oldValue is empty for the first mark. Throws std::invalid_argument if the
    // reason is empty or a mark is negative. Nothing is saved in that case.
    int recordMarkChange(int submissionId, int changedBy, std::optional<double> oldValue,
                         double newValue, const std::string& reason);

    // Throws AccessDeniedError unless the viewer may see the audit log.
    std::vector<lms::domain::MarkAuditEntry> history(const lms::domain::User& viewer,
                                                     int submissionId) const;
    std::vector<lms::domain::MarkAuditEntry> all(const lms::domain::User& viewer) const;

private:
    static lms::domain::TimePoint systemNow();
    static void requireViewer(const lms::domain::User& viewer);

    lms::domain::IAuditRepository& repo_;
    Clock clock_;
};

}  // namespace lms::application
