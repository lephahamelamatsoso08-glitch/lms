#include "ModerationService.h"

#include <algorithm>
#include <chrono>
#include <exception>
#include <optional>
#include <stdexcept>
#include <utility>

#include "Assignment.h"
#include "Course.h"
#include "SubmissionStatus.h"
#include "UserRoles.h"

namespace lms::application {

using lms::domain::Action;
using lms::domain::Assignment;
using lms::domain::Course;
using lms::domain::HoD;
using lms::domain::Submission;
using lms::domain::SubmissionStatus;
using lms::domain::TimePoint;
using lms::domain::User;

namespace {
bool isBlank(const std::string& text) {
    return text.find_first_not_of(" \t\r\n") == std::string::npos;
}
}  // namespace

ModerationService::ModerationService(lms::domain::ISubmissionRepository& submissions,
                                     const lms::domain::IAssignmentRepository& assignments,
                                     const lms::domain::ICourseRepository& courses,
                                     AuditService& audit,
                                     lms::domain::ITransactionRunner& transactions, Clock clock)
    : submissions_(submissions),
      assignments_(assignments),
      courses_(courses),
      audit_(audit),
      transactions_(transactions),
      clock_(std::move(clock)) {
    if (!clock_) {
        clock_ = &ModerationService::systemNow;
    }
}

TimePoint ModerationService::systemNow() { return std::chrono::system_clock::now(); }

void ModerationService::attach(lms::domain::IReleaseObserver& observer) {
    if (std::find(observers_.begin(), observers_.end(), &observer) == observers_.end()) {
        observers_.push_back(&observer);
    }
}

void ModerationService::requireHoDOf(const User& user, Action action, int courseId) const {
    const HoD* hod = dynamic_cast<const HoD*>(&user);
    if (hod == nullptr || !user.canPerform(action)) {
        throw AccessDeniedError("Only the head of department can moderate marks.");
    }
    const std::optional<Course> course = courses_.findById(courseId);
    if (!course || course->departmentId() != hod->departmentId()) {
        throw AccessDeniedError("This course is not in your department.");
    }
}

Assignment ModerationService::requireAssignment(int assignmentId) const {
    const std::optional<Assignment> assignment = assignments_.findById(assignmentId);
    if (!assignment) {
        throw std::invalid_argument("Assignment not found.");
    }
    return *assignment;
}

std::vector<Submission> ModerationService::listMarked(const User& hod, int assignmentId) const {
    const Assignment assignment = requireAssignment(assignmentId);
    requireHoDOf(hod, Action::ModerateMarks, assignment.courseId());

    std::vector<Submission> result;
    for (const Submission& s : submissions_.findByAssignment(assignmentId)) {
        if (s.status() == SubmissionStatus::Marked || s.status() == SubmissionStatus::Moderated ||
            s.status() == SubmissionStatus::Released) {
            result.push_back(s);
        }
    }
    return result;
}

void ModerationService::approve(const User& hod, int submissionId) {
    const std::optional<Submission> found = submissions_.findById(submissionId);
    if (!found) {
        throw std::invalid_argument("Submission not found.");
    }
    const Assignment assignment = requireAssignment(found->assignmentId());
    requireHoDOf(hod, Action::ModerateMarks, assignment.courseId());

    Submission updated = *found;
    updated.transitionTo(SubmissionStatus::Moderated);  // only Marked can be approved
    submissions_.save(updated);
}

void ModerationService::changeMark(const User& hod, int submissionId, double newMark,
                                   const std::string& reason) {
    const std::optional<Submission> found = submissions_.findById(submissionId);
    if (!found) {
        throw std::invalid_argument("Submission not found.");
    }
    const Assignment assignment = requireAssignment(found->assignmentId());
    requireHoDOf(hod, Action::ModerateMarks, assignment.courseId());

    if (isBlank(reason)) {
        throw std::invalid_argument("A mark change needs a reason.");
    }
    if (newMark < 0.0 || newMark > assignment.maxMark()) {
        throw std::invalid_argument("The mark must be between 0 and the maximum mark.");
    }

    Submission updated = *found;
    const std::optional<double> oldValue = updated.finalMark();
    updated.changeFinalMark(newMark);  // only while Marked

    transactions_.runInTransaction([&]() {
        submissions_.save(updated);
        audit_.recordMarkChange(updated.id(), hod.id(), oldValue, newMark, reason);
    });
}

ReleaseResult ModerationService::release(const User& hod, int assignmentId) {
    const Assignment assignment = requireAssignment(assignmentId);
    requireHoDOf(hod, Action::ReleaseMarks, assignment.courseId());

    std::vector<Submission> toRelease;
    for (const Submission& s : submissions_.findByAssignment(assignmentId)) {
        if (s.status() == SubmissionStatus::Moderated) {
            toRelease.push_back(s);
        }
    }
    if (toRelease.empty()) {
        throw std::logic_error("There are no moderated submissions to release.");
    }

    const TimePoint now = clock_();
    std::vector<int> studentIds;
    transactions_.runInTransaction([&]() {
        for (Submission& s : toRelease) {
            s.release(now);
            submissions_.save(s);
            studentIds.push_back(s.studentId());
        }
    });

    // The release is saved. Now tell the observers (FR18). One failing observer
    // must not stop the others, and must not undo the release.
    int failures = 0;
    for (lms::domain::IReleaseObserver* observer : observers_) {
        try {
            observer->onMarksReleased(assignmentId, studentIds);
        } catch (const std::exception&) {
            ++failures;
        }
    }
    return ReleaseResult{static_cast<int>(studentIds.size()), studentIds, failures};
}

}  // namespace lms::application
