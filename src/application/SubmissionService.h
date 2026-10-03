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
#include "ITransactionRunner.h"
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
                      Clock clock = &systemNow,
                      lms::domain::ITransactionRunner* transactions = nullptr);

    // A student submits work, or resubmits before their deadline.
    //  - AccessDeniedError: not an active student, or not enrolled in the course.
    //  - std::invalid_argument: unknown assignment or an unsuitable file.
    //  - SubmissionNotAllowedError: resubmission after the deadline or after marking.
    // Late work is never rejected (rule C4): it is saved with status Late.
    SubmitResult submit(const lms::domain::User& student, int assignmentId,
                        const std::string& filePath);

    // FR12: a lecturer marks a submission (or re-marks it before moderation).
    // Applies the late penalty, saves the mark and feedback, and writes the audit
    // entry, all in one transaction when a transaction runner was given.
    // A re-mark needs a reason. Returns the final mark (after the penalty).
    //  - AccessDeniedError: not an active lecturer, or not teaching this course.
    //  - std::invalid_argument: unknown submission, mark outside 0..maximum,
    //    blank feedback, or a re-mark without a reason.
    //  - std::logic_error: the work is already Moderated or Released.
    // Nothing is saved when an error is thrown.
    double mark(const lms::domain::User& lecturer, int submissionId, double rawMark,
                const std::string& feedback, const std::string& reason = "");

private:
    static lms::domain::TimePoint systemNow();

    lms::domain::ISubmissionRepository& submissions_;
    const lms::domain::IAssignmentRepository& assignments_;
    const lms::domain::ICourseRepository& courses_;
    const lms::domain::IPenaltyStrategy& penalty_;
    AuditService& audit_;
    Clock clock_;
    lms::domain::ITransactionRunner* transactions_;
};

}  // namespace lms::application
