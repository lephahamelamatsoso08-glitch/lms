#pragma once

#include <functional>
#include <stdexcept>
#include <string>

#include "AccessDeniedError.h"
#include "AuditService.h"
#include "IAssignmentRepository.h"
#include "ICourseRepository.h"
#include "IPenaltyStrategy.h"
#include "ISubmissionRepository.h"
#include "LateDays.h"
#include "Submission.h"
#include "User.h"

namespace lms::application {

// Thrown when a submission or resubmission is not allowed (rule C5).
class SubmissionNotAllowedError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

// What the student is told after submitting.
struct SubmitResult {
    lms::domain::Submission submission;
    int versionNo;               // 1 for the first file, 2 for the first resubmission ...
    bool isResubmission;
    double highestPossibleMark;  // after the late penalty (the student sees this, FR11)
};

// The use case "Submit assignment" (FR9, FR10, FR11).
// It depends only on interfaces (dependency inversion).
class SubmissionService {
public:
    using Clock = std::function<lms::domain::TimePoint()>;

    SubmissionService(lms::domain::ISubmissionRepository& submissions,
                      const lms::domain::IAssignmentRepository& assignments,
                      const lms::domain::ICourseRepository& courses,
                      const lms::domain::IPenaltyStrategy& penalty, AuditService& audit,
                      Clock clock = &systemNow);

    // A student submits work, or resubmits before their deadline.
    //  - AccessDeniedError: not an active student, or not enrolled in the course.
    //  - std::invalid_argument: unknown assignment or an unsuitable file.
    //  - SubmissionNotAllowedError: resubmission after the deadline or after marking.
    // Late work is never rejected (rule C4): it is saved with status Late.
    SubmitResult submit(const lms::domain::User& student, int assignmentId,
                        const std::string& filePath);

private:
    static lms::domain::TimePoint systemNow();

    lms::domain::ISubmissionRepository& submissions_;
    const lms::domain::IAssignmentRepository& assignments_;
    const lms::domain::ICourseRepository& courses_;
    const lms::domain::IPenaltyStrategy& penalty_;
    AuditService& audit_;
    Clock clock_;
};

}  // namespace lms::application
