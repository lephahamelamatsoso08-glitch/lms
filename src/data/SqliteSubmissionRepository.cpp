#include "SqliteSubmissionRepository.h"

#include <stdexcept>
#include <string>

#include "Statement.h"
#include "TimeFormat.h"

namespace lms::data {

using lms::domain::Submission;
using lms::domain::SubmissionVersion;
using lms::domain::TimePoint;

namespace {

const char* const kSelectSubmission =
    "SELECT id, assignment_id, student_id, status, late_days, raw_mark, final_mark, feedback, "
    "released_at FROM submission ";

void bindOptionalDouble(Statement& s, int index, const std::optional<double>& value) {
    if (value) {
        s.bindDouble(index, *value);
    } else {
        s.bindNull(index);
    }
}

}  // namespace

SqliteSubmissionRepository::SqliteSubmissionRepository(DatabaseConnection& db) : db_(db) {}

void SqliteSubmissionRepository::checkNewVersion(const SubmissionVersion& version) {
    if (version.versionNo != 0) {
        throw std::invalid_argument("a new version must have versionNo 0");
    }
    if (version.filePath.empty()) {
        throw std::invalid_argument("a version needs a file path");
    }
}

int SqliteSubmissionRepository::insertVersion(int submissionId, const SubmissionVersion& version) {
    int next = 1;
    {
        Statement q = db_.prepare(
            "SELECT COALESCE(MAX(version_no), 0) + 1 FROM submission_version "
            "WHERE submission_id = ?");
        q.bindInt(1, submissionId);
        q.step();
        next = static_cast<int>(q.columnInt(0));
    }
    Statement insert = db_.prepare(
        "INSERT INTO submission_version(submission_id, version_no, file_path, submitted_at) "
        "VALUES (?, ?, ?, ?)");
    insert.bindInt(1, submissionId);
    insert.bindInt(2, next);
    insert.bindText(3, version.filePath);
    insert.bindText(4, lms::domain::toIso8601(version.submittedAt));
    insert.step();
    return next;
}

int SqliteSubmissionRepository::add(const Submission& submission,
                                    const SubmissionVersion& firstVersion) {
    if (submission.id() != 0) {
        throw std::invalid_argument("submission is already saved");
    }
    checkNewVersion(firstVersion);

    db_.beginTransaction();
    try {
        int newId = 0;
        {
            Statement insert = db_.prepare(
                "INSERT INTO submission(assignment_id, student_id, status, late_days, raw_mark, "
                "final_mark, feedback, released_at) VALUES (?, ?, ?, ?, ?, ?, ?, ?)");
            insert.bindInt(1, submission.assignmentId());
            insert.bindInt(2, submission.studentId());
            insert.bindText(3, lms::domain::toString(submission.status()));
            insert.bindInt(4, submission.lateDays());
            bindOptionalDouble(insert, 5, submission.rawMark());
            bindOptionalDouble(insert, 6, submission.finalMark());
            if (submission.feedback().empty()) {
                insert.bindNull(7);
            } else {
                insert.bindText(7, submission.feedback());
            }
            if (submission.releasedAt()) {
                insert.bindText(8, lms::domain::toIso8601(*submission.releasedAt()));
            } else {
                insert.bindNull(8);
            }
            insert.step();
            newId = static_cast<int>(db_.lastInsertId());
        }
        insertVersion(newId, firstVersion);
        db_.commit();
        return newId;
    } catch (...) {
        db_.rollback();
        throw;
    }
}

Submission SqliteSubmissionRepository::readSubmission(Statement& row) {
    std::optional<double> rawMark;
    if (!row.isNull(5)) {
        rawMark = row.columnDouble(5);
    }
    std::optional<double> finalMark;
    if (!row.isNull(6)) {
        finalMark = row.columnDouble(6);
    }
    std::optional<TimePoint> releasedAt;
    if (!row.isNull(8)) {
        releasedAt = lms::domain::fromIso8601(row.columnText(8));
    }
    return Submission(static_cast<int>(row.columnInt(0)), static_cast<int>(row.columnInt(1)),
                      static_cast<int>(row.columnInt(2)),
                      lms::domain::submissionStatusFromString(row.columnText(3)),
                      static_cast<int>(row.columnInt(4)), rawMark, finalMark, row.columnText(7),
                      releasedAt);
}

std::optional<Submission> SqliteSubmissionRepository::findById(int id) const {
    Statement s = db_.prepare(std::string(kSelectSubmission) + "WHERE id = ?");
    s.bindInt(1, id);
    if (!s.step()) {
        return std::nullopt;
    }
    return readSubmission(s);
}

std::optional<Submission> SqliteSubmissionRepository::findByStudentAndAssignment(
    int studentId, int assignmentId) const {
    Statement s = db_.prepare(std::string(kSelectSubmission) +
                              "WHERE student_id = ? AND assignment_id = ?");
    s.bindInt(1, studentId);
    s.bindInt(2, assignmentId);
    if (!s.step()) {
        return std::nullopt;
    }
    return readSubmission(s);
}

std::vector<Submission> SqliteSubmissionRepository::findByAssignment(int assignmentId) const {
    Statement s =
        db_.prepare(std::string(kSelectSubmission) + "WHERE assignment_id = ? ORDER BY id");
    s.bindInt(1, assignmentId);
    std::vector<Submission> result;
    while (s.step()) {
        result.push_back(readSubmission(s));
    }
    return result;
}

void SqliteSubmissionRepository::save(const Submission& submission) {
    if (submission.id() == 0) {
        throw std::invalid_argument("submission has not been added yet");
    }
    {
        Statement check = db_.prepare("SELECT COUNT(*) FROM submission WHERE id = ?");
        check.bindInt(1, submission.id());
        check.step();
        if (check.columnInt(0) == 0) {
            throw std::invalid_argument("submission not found");
        }
    }
    Statement update = db_.prepare(
        "UPDATE submission SET status = ?, late_days = ?, raw_mark = ?, final_mark = ?, "
        "feedback = ?, released_at = ? WHERE id = ?");
    update.bindText(1, lms::domain::toString(submission.status()));
    update.bindInt(2, submission.lateDays());
    bindOptionalDouble(update, 3, submission.rawMark());
    bindOptionalDouble(update, 4, submission.finalMark());
    if (submission.feedback().empty()) {
        update.bindNull(5);
    } else {
        update.bindText(5, submission.feedback());
    }
    if (submission.releasedAt()) {
        update.bindText(6, lms::domain::toIso8601(*submission.releasedAt()));
    } else {
        update.bindNull(6);
    }
    update.bindInt(7, submission.id());
    update.step();
}

int SqliteSubmissionRepository::addVersion(int submissionId, const SubmissionVersion& version) {
    checkNewVersion(version);
    db_.beginTransaction();
    try {
        const int number = insertVersion(submissionId, version);
        db_.commit();
        return number;
    } catch (...) {
        db_.rollback();
        throw;
    }
}

std::vector<SubmissionVersion> SqliteSubmissionRepository::findVersions(int submissionId) const {
    Statement s = db_.prepare(
        "SELECT version_no, file_path, submitted_at FROM submission_version "
        "WHERE submission_id = ? ORDER BY version_no");
    s.bindInt(1, submissionId);
    std::vector<SubmissionVersion> result;
    while (s.step()) {
        SubmissionVersion v;
        v.versionNo = static_cast<int>(s.columnInt(0));
        v.filePath = s.columnText(1);
        v.submittedAt = lms::domain::fromIso8601(s.columnText(2));
        result.push_back(v);
    }
    return result;
}

}  // namespace lms::data
