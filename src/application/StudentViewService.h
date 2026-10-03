#pragma once

#include <optional>
#include <string>
#include <vector>

#include "AccessDeniedError.h"
#include "Course.h"
#include "IAssignmentRepository.h"
#include "ICourseRepository.h"
#include "ISubmissionRepository.h"
#include "LateDays.h"
#include "User.h"

namespace lms::application {

// One line of the student's assignment list (FR8).
struct StudentAssignmentItem {
    int assignmentId = 0;
    std::string title;
    double maxMark = 0.0;
    lms::domain::TimePoint deadline{};  // this student's own deadline (rule C2)
    std::string statusLabel;            // only text the student is allowed to see
};

// What a student sees about one of their submissions (FR17).
// Rule C1: rawMark, finalMark, feedback and releasedAt stay EMPTY until the
// submission is Released, even if they are already stored.
struct StudentSubmissionView {
    int assignmentId = 0;
    std::string assignmentTitle;
    double maxMark = 0.0;
    lms::domain::TimePoint deadline{};
    bool submitted = false;
    std::string statusLabel;
    int lateDays = 0;
    int versionCount = 0;

    std::optional<double> rawMark;
    std::optional<double> finalMark;
    std::optional<std::string> feedback;
    std::optional<lms::domain::TimePoint> releasedAt;
};

// Read-only pages for students (FR8, FR17). A student only ever sees their own work.
class StudentViewService {
public:
    StudentViewService(const lms::domain::ISubmissionRepository& submissions,
                       const lms::domain::IAssignmentRepository& assignments,
                       const lms::domain::ICourseRepository& courses);

    // All three throw AccessDeniedError for anyone who is not an active student,
    // and for a course the student is not enrolled in.
    std::vector<lms::domain::Course> myCourses(const lms::domain::User& student) const;
    std::vector<StudentAssignmentItem> myAssignments(const lms::domain::User& student,
                                                     int courseId) const;

    // std::invalid_argument if the assignment does not exist.
    StudentSubmissionView mySubmission(const lms::domain::User& student, int assignmentId) const;

private:
    static void requireStudent(const lms::domain::User& student);
    void requireEnrolled(const lms::domain::User& student, int courseId) const;

    const lms::domain::ISubmissionRepository& submissions_;
    const lms::domain::IAssignmentRepository& assignments_;
    const lms::domain::ICourseRepository& courses_;
};

}  // namespace lms::application
