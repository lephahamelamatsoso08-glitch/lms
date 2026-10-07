#pragma once

#include <optional>
#include <string>
#include <vector>

#include "AccessDeniedError.h"
#include "IAssignmentRepository.h"
#include "ICourseRepository.h"
#include "ISubmissionRepository.h"
#include "SubmissionStatus.h"
#include "User.h"

namespace lms::application {

// One line of the lecturer's list: one enrolled student for one assignment. A student
// who has not submitted still has a line (submitted is false).
struct LecturerSubmissionRow {
    int studentId = 0;
    std::string studentName;
    std::string studentNumber;

    bool submitted = false;
    int submissionId = 0;  // 0 when not submitted
    std::optional<lms::domain::SubmissionStatus> status;
    int lateDays = 0;
    int versionCount = 0;
    std::string latestFile;
    std::optional<double> rawMark;
    std::optional<double> finalMark;
    std::string feedback;
};

// What a lecturer sees about the work for an assignment. Lecturers see the marks they
// gave straight away (the students do not, rule C1).
class LecturerViewService {
public:
    LecturerViewService(const lms::domain::ISubmissionRepository& submissions,
                        const lms::domain::IAssignmentRepository& assignments,
                        const lms::domain::ICourseRepository& courses);

    // One row per enrolled student, ordered by student number.
    //  - AccessDeniedError: not an active lecturer, or not teaching this course.
    //  - std::invalid_argument: unknown assignment.
    std::vector<LecturerSubmissionRow> rows(const lms::domain::User& lecturer,
                                            int assignmentId) const;

private:
    const lms::domain::ISubmissionRepository& submissions_;
    const lms::domain::IAssignmentRepository& assignments_;
    const lms::domain::ICourseRepository& courses_;
};

}  // namespace lms::application
