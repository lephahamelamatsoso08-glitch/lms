#include "SqliteAssignmentRepository.h"

#include <stdexcept>

#include "Statement.h"
#include "TimeFormat.h"

namespace lms::data {

using lms::domain::Assignment;
using lms::domain::Extension;
using lms::domain::TimePoint;

namespace {
const char* const kSelectAssignment =
    "SELECT id, course_id, title, deadline, max_mark FROM assignment ";
}

SqliteAssignmentRepository::SqliteAssignmentRepository(DatabaseConnection& db) : db_(db) {}

int SqliteAssignmentRepository::add(const Assignment& assignment) {
    if (assignment.id() != 0) {
        throw std::invalid_argument("assignment is already saved");
    }
    Statement s = db_.prepare(
        "INSERT INTO assignment(course_id, title, deadline, max_mark) VALUES (?, ?, ?, ?)");
    s.bindInt(1, assignment.courseId());
    s.bindText(2, assignment.title());
    s.bindText(3, lms::domain::toIso8601(assignment.deadline()));
    s.bindDouble(4, assignment.maxMark());
    s.step();
    return static_cast<int>(db_.lastInsertId());
}

Assignment SqliteAssignmentRepository::readAssignment(Statement& row) {
    return Assignment(static_cast<int>(row.columnInt(0)), static_cast<int>(row.columnInt(1)),
                      row.columnText(2), lms::domain::fromIso8601(row.columnText(3)),
                      row.columnDouble(4));
}

std::optional<Assignment> SqliteAssignmentRepository::findById(int id) const {
    Statement s = db_.prepare(std::string(kSelectAssignment) + "WHERE id = ?");
    s.bindInt(1, id);
    if (!s.step()) {
        return std::nullopt;
    }
    return readAssignment(s);
}

std::vector<Assignment> SqliteAssignmentRepository::findByCourse(int courseId) const {
    Statement s =
        db_.prepare(std::string(kSelectAssignment) + "WHERE course_id = ? ORDER BY deadline, id");
    s.bindInt(1, courseId);
    std::vector<Assignment> result;
    while (s.step()) {
        result.push_back(readAssignment(s));
    }
    return result;
}

int SqliteAssignmentRepository::saveExtension(const Extension& extension) {
    if (extension.id() != 0) {
        throw std::invalid_argument("extension is already saved");
    }
    Statement save = db_.prepare(
        "INSERT INTO extension(assignment_id, student_id, new_deadline, reason) "
        "VALUES (?, ?, ?, ?) "
        "ON CONFLICT(assignment_id, student_id) DO UPDATE SET "
        "new_deadline = excluded.new_deadline, reason = excluded.reason");
    save.bindInt(1, extension.assignmentId());
    save.bindInt(2, extension.studentId());
    save.bindText(3, lms::domain::toIso8601(extension.newDeadline()));
    save.bindText(4, extension.reason());
    save.step();

    Statement read =
        db_.prepare("SELECT id FROM extension WHERE assignment_id = ? AND student_id = ?");
    read.bindInt(1, extension.assignmentId());
    read.bindInt(2, extension.studentId());
    read.step();
    return static_cast<int>(read.columnInt(0));
}

std::optional<Extension> SqliteAssignmentRepository::findExtension(int assignmentId,
                                                                   int studentId) const {
    Statement s = db_.prepare(
        "SELECT id, assignment_id, student_id, new_deadline, reason FROM extension "
        "WHERE assignment_id = ? AND student_id = ?");
    s.bindInt(1, assignmentId);
    s.bindInt(2, studentId);
    if (!s.step()) {
        return std::nullopt;
    }
    return Extension(static_cast<int>(s.columnInt(0)), static_cast<int>(s.columnInt(1)),
                     static_cast<int>(s.columnInt(2)), lms::domain::fromIso8601(s.columnText(3)),
                     s.columnText(4));
}

TimePoint SqliteAssignmentRepository::getDeadlineFor(int studentId, int assignmentId) const {
    const std::optional<Assignment> assignment = findById(assignmentId);
    if (!assignment) {
        throw std::invalid_argument("assignment not found");
    }
    return assignment->deadlineFor(findExtension(assignmentId, studentId));
}

}  // namespace lms::data
