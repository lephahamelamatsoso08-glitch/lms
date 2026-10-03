#pragma once

#include <string>

#include "LateDays.h"

namespace lms::domain {

// One uploaded file of a submission (FR10). Old versions are kept.
struct SubmissionVersion {
    int versionNo = 0;  // 0 means "not saved yet"; the repository numbers 1, 2, 3 ...
    std::string filePath;
    TimePoint submittedAt{};
};

}  // namespace lms::domain
