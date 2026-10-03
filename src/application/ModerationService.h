#pragma once

#include <functional>
#include <string>
#include <vector>

#include "AccessDeniedError.h"
#include "Action.h"
#include "AuditService.h"
#include "IAssignmentRepository.h"
#include "ICourseRepository.h"
#include "IReleaseObserver.h"
#include "ISubmissionRepository.h"
#include "ITransactionRunner.h"
#include "LateDays.h"
#include "Submission.h"
#include "User.h"

namespace lms::application {

struct ReleaseResult {
    int releasedCount;
    std::vector<int> studentIds;  // the students whose marks were released
    int notificationFailures;     // observers that failed (the release still stands)
};

// The use case "Moderate and release marks" (FR13, FR14).
// Only the head of the department that owns the course may act (rule C7).
class ModerationService {
public:
    using Clock = std::function<lms::domain::TimePoint()>;

    ModerationService(lms::domain::ISubmissionRepository& submissions,
                      const lms::domain::IAssignmentRepository& assignments,
                      const lms::domain::ICourseRepository& courses, AuditService& audit,
                      lms::domain::ITransactionRunner& transactions, Clock clock = &systemNow);

    // Observer pattern. Attaching the same observer twice has no effect.
    void attach(lms::domain::IReleaseObserver& observer);

    // All submissions of the assignment that have a mark (Marked, Moderated, Released).
    std::vector<lms::domain::Submission> listMarked(const lms::domain::User& hod,
                                                    int assignmentId) const;

    // Marked -> Moderated. std::logic_error if the work is not in the Marked status.
    void approve(const lms::domain::User& hod, int submissionId);

    // The HoD changes the final mark (only while Marked). A reason is required.
    // The change and its audit entry are saved in one transaction.
    void changeMark(const lms::domain::User& hod, int submissionId, double newMark,
                    const std::string& reason);

    // Releases every Moderated submission of the assignment together (rule C8).
    // std::logic_error if there is nothing to release. Observers are told after the
    // release is saved. A failing observer does not undo the release.
    ReleaseResult release(const lms::domain::User& hod, int assignmentId);

private:
    static lms::domain::TimePoint systemNow();
    void requireHoDOf(const lms::domain::User& user, lms::domain::Action action,
                      int courseId) const;
    lms::domain::Assignment requireAssignment(int assignmentId) const;

    lms::domain::ISubmissionRepository& submissions_;
    const lms::domain::IAssignmentRepository& assignments_;
    const lms::domain::ICourseRepository& courses_;
    AuditService& audit_;
    lms::domain::ITransactionRunner& transactions_;
    Clock clock_;
    std::vector<lms::domain::IReleaseObserver*> observers_;
};

}  // namespace lms::application
