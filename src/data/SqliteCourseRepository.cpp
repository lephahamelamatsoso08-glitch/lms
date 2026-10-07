#include "SqliteCourseRepository.h"

#include <stdexcept>

#include "Statement.h"

namespace lms::data {

using lms::domain::Course;

namespace {
const char* const kSelectCourse = "SELECT c.id, c.code, c.title, c.department_id FROM course c ";
}

SqliteCourseRepository::SqliteCourseRepository(DatabaseConnection& db) : db_(db) {}

int SqliteCourseRepository::addDepartment(const std::string& name) {
    if (name.find_first_not_of(" \t\r\n") == std::string::npos) {
        throw std::invalid_argument("a department needs a name");
    }
    Statement s = db_.prepare("INSERT INTO department(name) VALUES (?)");
    s.bindText(1, name);
    s.step();
    return static_cast<int>(db_.lastInsertId());
}

int SqliteCourseRepository::add(const Course& course) {
    if (course.id() != 0) {
        throw std::invalid_argument("course is already saved");
    }
    Statement s =
        db_.prepare("INSERT INTO course(code, title, department_id) VALUES (?, ?, ?)");
    s.bindText(1, course.code());
    s.bindText(2, course.title());
    s.bindInt(3, course.departmentId());
    s.step();
    return static_cast<int>(db_.lastInsertId());
}

Course SqliteCourseRepository::readCourse(Statement& row) {
    return Course(static_cast<int>(row.columnInt(0)), row.columnText(1), row.columnText(2),
                  static_cast<int>(row.columnInt(3)));
}

std::vector<Course> SqliteCourseRepository::readAll(Statement& query) const {
    std::vector<Course> result;
    while (query.step()) {
        result.push_back(readCourse(query));
    }
    return result;
}

std::optional<Course> SqliteCourseRepository::findById(int id) const {
    Statement s = db_.prepare(std::string(kSelectCourse) + "WHERE c.id = ?");
    s.bindInt(1, id);
    if (!s.step()) {
        return std::nullopt;
    }
    return readCourse(s);
}

std::vector<Course> SqliteCourseRepository::findByDepartment(int departmentId) const {
    Statement s = db_.prepare(std::string(kSelectCourse) +
                              "WHERE c.department_id = ? ORDER BY c.code");
    s.bindInt(1, departmentId);
    return readAll(s);
}

void SqliteCourseRepository::enrolStudent(int courseId, int studentId) {
    // The foreign keys reject an unknown course and anyone who is not a student.
    Statement s =
        db_.prepare("INSERT OR IGNORE INTO enrolment(course_id, student_id) VALUES (?, ?)");
    s.bindInt(1, courseId);
    s.bindInt(2, studentId);
    s.step();
}

void SqliteCourseRepository::assignLecturer(int courseId, int lecturerId) {
    {
        Statement role = db_.prepare("SELECT role FROM user_account WHERE id = ?");
        role.bindInt(1, lecturerId);
        if (!role.step() || role.columnText(0) != "LECTURER") {
            throw std::invalid_argument("the user is not a lecturer");
        }
    }
    Statement s = db_.prepare(
        "INSERT OR IGNORE INTO course_lecturer(course_id, lecturer_id) VALUES (?, ?)");
    s.bindInt(1, courseId);
    s.bindInt(2, lecturerId);
    s.step();
}

bool SqliteCourseRepository::isStudentEnrolled(int studentId, int courseId) const {
    Statement s =
        db_.prepare("SELECT COUNT(*) FROM enrolment WHERE student_id = ? AND course_id = ?");
    s.bindInt(1, studentId);
    s.bindInt(2, courseId);
    s.step();
    return s.columnInt(0) > 0;
}

bool SqliteCourseRepository::isLecturerAssigned(int lecturerId, int courseId) const {
    Statement s = db_.prepare(
        "SELECT COUNT(*) FROM course_lecturer WHERE lecturer_id = ? AND course_id = ?");
    s.bindInt(1, lecturerId);
    s.bindInt(2, courseId);
    s.step();
    return s.columnInt(0) > 0;
}

std::vector<Course> SqliteCourseRepository::findForStudent(int studentId) const {
    Statement s = db_.prepare(std::string(kSelectCourse) +
                              "JOIN enrolment e ON e.course_id = c.id "
                              "WHERE e.student_id = ? ORDER BY c.code");
    s.bindInt(1, studentId);
    return readAll(s);
}

std::vector<Course> SqliteCourseRepository::findForLecturer(int lecturerId) const {
    Statement s = db_.prepare(std::string(kSelectCourse) +
                              "JOIN course_lecturer l ON l.course_id = c.id "
                              "WHERE l.lecturer_id = ? ORDER BY c.code");
    s.bindInt(1, lecturerId);
    return readAll(s);
}

std::vector<lms::domain::StudentInfo> SqliteCourseRepository::findStudents(int courseId) const {
    Statement s = db_.prepare(
        "SELECT u.id, u.full_name, st.student_number FROM enrolment e "
        "JOIN student st ON st.user_id = e.student_id "
        "JOIN user_account u ON u.id = st.user_id "
        "WHERE e.course_id = ? ORDER BY st.student_number");
    s.bindInt(1, courseId);
    std::vector<lms::domain::StudentInfo> result;
    while (s.step()) {
        lms::domain::StudentInfo info;
        info.id = static_cast<int>(s.columnInt(0));
        info.name = s.columnText(1);
        info.number = s.columnText(2);
        result.push_back(info);
    }
    return result;
}

}  // namespace lms::data
