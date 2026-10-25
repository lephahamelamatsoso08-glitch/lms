#include "Submission.h"

#include <utility>

namespace lms::domain {

namespace {
bool isBlank(const std::string& text) {
    return text.find_first_not_of(" \t\r\n") == std::string::npos;
}
}  // namespace

Submission::Submission(int id, int assignmentId, int studentId, SubmissionStatus status,
                       int lateDays)
    : Submission(id, assignmentId, studentId, status, lateDays, std::nullopt, std::nullopt,
                 std::string(), std::nullopt) {}

Submission::Submission(int id, int assignmentId, int studentId, SubmissionStatus status,
                       int lateDays, std::optional<double> rawMark,
                       std::optional<double> finalMark, std::string feedback,
                       std::optional<TimePoint> releasedAt)
    : id_(id),
      assignmentId_(assignmentId),
      studentId_(studentId),
      status_(status),
      lateDays_(lateDays),
      rawMark_(rawMark),
      finalMark_(finalMark),
      feedback_(std::move(feedback)),
      releasedAt_(releasedAt) {
    if (id_ < 0) {
        throw std::invalid_argument("id must not be negative");
    }
    if (assignmentId_ <= 0 || studentId_ <= 0) {
        throw std::invalid_argument("a submission needs an assignment and a student");
    }
    if (lateDays_ < 0) {
        throw std::invalid_argument("lateDays must not be negative");
    }
    if (status_ == SubmissionStatus::Submitted && lateDays_ != 0) {
        throw std::invalid_argument("an on-time submission has no late days");
    }
    if (status_ == SubmissionStatus::Late && lateDays_ < 1) {
        throw std::invalid_argument("a late submission has at least one late day");
    }
    if ((rawMark_ && *rawMark_ < 0.0) || (finalMark_ && *finalMark_ < 0.0)) {
        throw std::invalid_argument("marks must not be negative");
    }
}

bool Submission::isAllowed(SubmissionStatus from, SubmissionStatus to) {
    switch (from) {
        case SubmissionStatus::Submitted:
            return to == SubmissionStatus::Submitted || to == SubmissionStatus::Marked;
        case SubmissionStatus::Late:
            return to == SubmissionStatus::Marked;
        case SubmissionStatus::Marked:
            return to == SubmissionStatus::Marked || to == SubmissionStatus::Moderated;
        case SubmissionStatus::Moderated:
        case SubmissionStatus::Released:
            return to == SubmissionStatus::Released;
    }
    return false;
}

void Submission::transitionTo(SubmissionStatus next) {
    if (!isAllowed(status_, next)) {
        throw std::logic_error("cannot move a submission from " + toString(status_) + " to " +
                               toString(next));
    }
    status_ = next;
}

void Submission::applyMark(double newRawMark, double newFinalMark, const std::string& newFeedback) {
    if (newRawMark < 0.0 || newFinalMark < 0.0) {
        throw std::invalid_argument("marks must not be negative");
    }
    if (newFinalMark > newRawMark) {
        throw std::invalid_argument("the final mark cannot be higher than the raw mark");
    }
    if (isBlank(newFeedback)) {
        throw std::invalid_argument("feedback must not be empty");
    }
    transitionTo(SubmissionStatus::Marked);  // throws if marking is not allowed now
    rawMark_ = newRawMark;
    finalMark_ = newFinalMark;
    feedback_ = newFeedback;
}

void Submission::changeFinalMark(double newFinalMark) {
    if (status_ != SubmissionStatus::Marked) {
        throw std::logic_error("a mark can only be changed while the submission is Marked");
    }
    if (newFinalMark < 0.0) {
        throw std::invalid_argument("marks must not be negative");
    }
    finalMark_ = newFinalMark;
}

void Submission::release(TimePoint when) {
    if (status_ != SubmissionStatus::Moderated) {
        throw std::logic_error("only a moderated submission can be released");
    }
    transitionTo(SubmissionStatus::Released);
    releasedAt_ = when;
}

}  // namespace lms::domain
