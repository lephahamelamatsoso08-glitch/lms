#pragma once

#include <functional>
#include <string>
#include <vector>

#include "AccessDeniedError.h"
#include "Action.h"
#include "Assignment.h"
#include "Course.h"
#include "IAssignmentRepository.h"
#include "ICourseRepository.h"
#include "ISubmissionRepository.h"
#include "LateDays.h"
#include "Submission.h"
#include "User.h"

namespace lms::application {

// What a lecturer does with assignments (FR6, FR7). A lecturer can only act on
// the courses they teach (FR2).
class AssignmentService {
public:
    using Clock = std::function<lms::domain::TimePoint()>;

    AssignmentService(lms::domain::IAssignmentRepository& assignments,
                      const lms::domain::ICourseRepository& courses,
                      const lms::domain::ISubmissionRepository& submissions,
                      Clock clock = &systemNow);

    std::vector<lms::domain::Course> myCourses(const lms::domain::User& lecturer) const;
    std::vector<lms::domain::Assignment> listAssignments(const lms::domain::User& lecturer,
                                                         int courseId) const;

    // All submissions of an assignment, with their marks and feedback. A lecturer
    // sees the marks they gave straight away (the lecturer's wish in the interviews).
    std::vector<lms::domain::Submission> listSubmissions(const lms::domain::User& lecturer,
                                                         int assignmentId) const;

    // FR6. The deadline must be in the future. Returns the new assignment id.
    //  - AccessDeniedError: not an active lecturer, or not teaching this course.
    //  - std::invalid_argument: bad title or maximum mark, or a deadline not in the future.
    int createAssignment(const lms::domain::User& lecturer, int courseId, const std::string& title,
                         lms::domain::TimePoint deadline, double maxMark);

    // FR7. Gives one student a later deadline, with a reason. A second extension for the
    // same student replaces the first. Returns the extension id.
    //  - AccessDeniedError: not an active lecturer, or not teaching this course.
    //  - std::invalid_argument: unknown assignment, student not enrolled, new deadline not
    //    later than the normal deadline, or no reason.
    //  - std::logic_error: the student has already submitted late or been marked.
    int grantExtension(const lms::domain::User& lecturer, int assignmentId, int studentId,
                       lms::domain::TimePoint newDeadline, const std::string& reason);

private:
    static lms::domain::TimePoint systemNow();
    void requireTeaches(const lms::domain::User& lecturer, lms::domain::Action action,
                        int courseId) const;

    lms::domain::IAssignmentRepository& assignments_;
    const lms::domain::ICourseRepository& courses_;
    const lms::domain::ISubmissionRepository& submissions_;
    Clock clock_;
};

}  // namespace lms::application
