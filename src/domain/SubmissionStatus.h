#pragma once

#include <string>

namespace lms::domain {

enum class SubmissionStatus { Submitted, Late, Marked, Moderated, Released };

// The text form is what the database stores in submission.status.
std::string toString(SubmissionStatus status);
SubmissionStatus submissionStatusFromString(const std::string& text);

}  // namespace lms::domain
