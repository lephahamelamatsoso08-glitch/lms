#include "SubmissionStatus.h"

#include <stdexcept>

namespace lms::domain {

std::string toString(SubmissionStatus status) {
    switch (status) {
        case SubmissionStatus::Submitted: return "SUBMITTED";
        case SubmissionStatus::Late:      return "LATE";
        case SubmissionStatus::Marked:    return "MARKED";
        case SubmissionStatus::Moderated: return "MODERATED";
        case SubmissionStatus::Released:  return "RELEASED";
    }
    throw std::invalid_argument("unknown submission status");
}

SubmissionStatus submissionStatusFromString(const std::string& text) {
    if (text == "SUBMITTED") return SubmissionStatus::Submitted;
    if (text == "LATE")      return SubmissionStatus::Late;
    if (text == "MARKED")    return SubmissionStatus::Marked;
    if (text == "MODERATED") return SubmissionStatus::Moderated;
    if (text == "RELEASED")  return SubmissionStatus::Released;
    throw std::invalid_argument("unknown submission status: " + text);
}

}  // namespace lms::domain
