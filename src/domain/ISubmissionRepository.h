#pragma once

#include <optional>
#include <vector>

#include "Submission.h"
#include "SubmissionVersion.h"

namespace lms::domain {

class ISubmissionRepository {
public:
    virtual ~ISubmissionRepository() = default;

    // Saves a new submission (its id must be 0) together with its first file,
    // all or nothing. Returns the new id.
    virtual int add(const Submission& submission, const SubmissionVersion& firstVersion) = 0;

    virtual std::optional<Submission> findById(int id) const = 0;
    virtual std::optional<Submission> findByStudentAndAssignment(int studentId,
                                                                 int assignmentId) const = 0;
    virtual std::vector<Submission> findByAssignment(int assignmentId) const = 0;

    // Saves the status, late days, marks, feedback and release time.
    virtual void save(const Submission& submission) = 0;

    // Adds a new file version (resubmission, FR10) and returns its number.
    virtual int addVersion(int submissionId, const SubmissionVersion& version) = 0;
    virtual std::vector<SubmissionVersion> findVersions(int submissionId) const = 0;  // oldest first
};

}  // namespace lms::domain
