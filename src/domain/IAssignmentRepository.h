#pragma once

#include <optional>
#include <vector>

#include "Assignment.h"
#include "Extension.h"
#include "LateDays.h"

namespace lms::domain {

class IAssignmentRepository {
public:
    virtual ~IAssignmentRepository() = default;

    // Saves a new assignment (its id must be 0) and returns the new id.
    virtual int add(const Assignment& assignment) = 0;
    virtual std::optional<Assignment> findById(int id) const = 0;
    virtual std::vector<Assignment> findByCourse(int courseId) const = 0;  // earliest deadline first

    // Saves a new extension (its id must be 0). A student has one extension per
    // assignment: a second one replaces the first. Returns the extension id.
    virtual int saveExtension(const Extension& extension) = 0;
    virtual std::optional<Extension> findExtension(int assignmentId, int studentId) const = 0;

    // The deadline that applies to this student (rule C2).
    // Throws std::invalid_argument if the assignment does not exist.
    virtual TimePoint getDeadlineFor(int studentId, int assignmentId) const = 0;
};

}  // namespace lms::domain
