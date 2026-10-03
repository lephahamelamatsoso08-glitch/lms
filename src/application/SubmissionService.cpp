#include "SubmissionService.h"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <optional>
#include <utility>

#include "Action.h"
#include "Assignment.h"
#include "SubmissionStatus.h"
#include "SubmissionVersion.h"

namespace lms::application {

using lms::domain::Action;
using lms::domain::Assignment;
using lms::domain::Submission;
using lms::domain::SubmissionStatus;
using lms::domain::SubmissionVersion;
using lms::domain::TimePoint;
using lms::domain::User;

namespace {

bool hasAllowedExtension(const std::string& path) {
    const std::size_t dot = path.rfind('.');
    if (dot == std::string::npos || dot + 1 >= path.size()) {
        return false;
    }
    std::string extension = path.substr(dot + 1);
    std::transform(extension.begin(), extension.end(), extension.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    static const char* const allowed[] = {"pdf", "doc", "docx", "txt", "zip"};
    return std::any_of(std::begin(allowed), std::end(allowed),
                       [&extension](const char* a) { return extension == a; });
}

bool isBlank(const std::string& text) {
    return text.find_first_not_of(" \t\r\n") == std::string::npos;
}

}  // namespace

SubmissionService::SubmissionService(lms::domain::ISubmissionRepository& submissions,
                                     const lms::domain::IAssignmentRepository& assignments,
                                     const lms::domain::ICourseRepository& courses,
                                     const lms::domain::IPenaltyStrategy& penalty,
                                     AuditService& audit, Clock clock,
                                     lms::domain::ITransactionRunner* transactions)
    : submissions_(submissions),
      assignments_(assignments),
      courses_(courses),
      penalty_(penalty),
      audit_(audit),
      clock_(std::move(clock)),
      transactions_(transactions) {
    if (!clock_) {
        clock_ = &SubmissionService::systemNow;
    }
}

TimePoint SubmissionService::systemNow() { return std::chrono::system_clock::now(); }

SubmitResult SubmissionService::submit(const User& student, int assignmentId,
                                       const std::string& filePath) {
    if (!student.canPerform(Action::SubmitWork)) {
        throw AccessDeniedError("Only active students can submit work.");
    }
    if (!hasAllowedExtension(filePath)) {
        throw std::invalid_argument("The file must be a pdf, doc, docx, txt or zip file.");
    }
    const std::optional<Assignment> assignment = assignments_.findById(assignmentId);
    if (!assignment) {
        throw std::invalid_argument("Assignment not found.");
    }
    if (!courses_.isStudentEnrolled(student.id(), assignment->courseId())) {
        throw AccessDeniedError("You are not enrolled in this course.");
    }

    // The student's own deadline: the extension if there is one (rule C2).
    const TimePoint now = clock_();
    const TimePoint deadline = assignments_.getDeadlineFor(student.id(), assignmentId);
    const int lateDays = lms::domain::calculateLateDays(deadline, now);
    const double highestPossible =
        penalty_.finalMark(assignment->maxMark(), assignment->maxMark(), lateDays);

    SubmissionVersion version;
    version.filePath = filePath;
    version.submittedAt = now;

    const std::optional<Submission> existing =
        submissions_.findByStudentAndAssignment(student.id(), assignmentId);

    if (existing) {
        // Resubmission (rule C5): only on time, and only before marking.
        switch (existing->status()) {
            case SubmissionStatus::Marked:
            case SubmissionStatus::Moderated:
            case SubmissionStatus::Released:
                throw SubmissionNotAllowedError("This work has already been marked.");
            case SubmissionStatus::Late:
                throw SubmissionNotAllowedError("Late work cannot be resubmitted.");
            case SubmissionStatus::Submitted:
                break;
        }
        if (lateDays > 0) {
            throw SubmissionNotAllowedError("You can only resubmit before the deadline.");
        }
        const int versionNo = submissions_.addVersion(existing->id(), version);
        return SubmitResult{*existing, versionNo, true, highestPossible};
    }

    // First submission. Late work is accepted and marked Late (rule C4).
    const SubmissionStatus status =
        (lateDays == 0) ? SubmissionStatus::Submitted : SubmissionStatus::Late;
    const int id = submissions_.add(Submission(0, assignmentId, student.id(), status, lateDays),
                                    version);
    return SubmitResult{*submissions_.findById(id), 1, false, highestPossible};
}

double SubmissionService::mark(const User& lecturer, int submissionId, double rawMark,
                               const std::string& feedback, const std::string& reason) {
    if (!lecturer.canPerform(Action::MarkSubmission)) {
        throw AccessDeniedError("Only lecturers can mark work.");
    }
    const std::optional<Submission> found = submissions_.findById(submissionId);
    if (!found) {
        throw std::invalid_argument("Submission not found.");
    }
    const std::optional<Assignment> assignment = assignments_.findById(found->assignmentId());
    if (!assignment) {
        throw std::invalid_argument("Assignment not found.");
    }
    if (!courses_.isLecturerAssigned(lecturer.id(), assignment->courseId())) {
        throw AccessDeniedError("You do not teach this course.");
    }
    if (rawMark < 0.0 || rawMark > assignment->maxMark()) {
        throw std::invalid_argument("The mark must be between 0 and the maximum mark.");
    }

    const std::optional<double> oldValue = found->finalMark();
    if (oldValue && isBlank(reason)) {
        throw std::invalid_argument("A re-mark needs a reason.");
    }
    const std::string auditReason = isBlank(reason) ? "Initial mark" : reason;

    // The late penalty is applied here (rule C3). The raw mark is kept as well.
    const double finalMark =
        penalty_.finalMark(rawMark, assignment->maxMark(), found->lateDays());

    Submission updated = *found;
    updated.applyMark(rawMark, finalMark, feedback);  // checks status and feedback

    const auto saveBoth = [&]() {
        submissions_.save(updated);
        audit_.recordMarkChange(updated.id(), lecturer.id(), oldValue, finalMark, auditReason);
    };
    if (transactions_ != nullptr) {
        transactions_->runInTransaction(saveBoth);
    } else {
        saveBoth();
    }
    return finalMark;
}

}  // namespace lms::application
