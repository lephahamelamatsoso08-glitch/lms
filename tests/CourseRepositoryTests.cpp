#include <gtest/gtest.h>

#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>

#include "DatabaseConnection.h"
#include "DatabaseError.h"
#include "SqliteCourseRepository.h"

using namespace lms::domain;
using lms::data::DatabaseConnection;
using lms::data::DatabaseError;
using lms::data::SqliteCourseRepository;
using lms::data::Statement;

namespace {

std::string readFile(const std::string& path) {
    std::ifstream in(path);
    if (!in) {
        throw std::runtime_error("cannot read " + path);
    }
    std::ostringstream text;
    text << in.rdbuf();
    return text.str();
}

class CourseRepoTest : public ::testing::Test {
protected:
    CourseRepoTest() : db(":memory:"), repo(db) {
        db.runScript(readFile(LMS_SCHEMA_PATH));
        db.runScript(R"sql(
INSERT INTO user_account(id, username, full_name, password_hash, role) VALUES
  (1, 'lec1', 'Lecturer One', 'x', 'LECTURER'),
  (2, 'stu1', 'Student One', 'x', 'STUDENT'),
  (3, 'stu2', 'Student Two', 'x', 'STUDENT'),
  (4, 'lec2', 'Lecturer Two', 'x', 'LECTURER');
INSERT INTO student(user_id, student_number) VALUES (2, 'S001'), (3, 'S002');
INSERT INTO department(id, name) VALUES (1, 'Computer Science');
)sql");
    }

    int addCourse(const std::string& code, int departmentId = 1) {
        return repo.add(Course(0, code, "Title of " + code, departmentId));
    }

    long long count(const std::string& table) {
        Statement s = db.prepare("SELECT COUNT(*) FROM " + table);
        s.step();
        return s.columnInt(0);
    }

    DatabaseConnection db;
    SqliteCourseRepository repo;
};

}  // namespace

// ---- Departments and courses (FR4) ----
TEST_F(CourseRepoTest, AddDepartmentReturnsANewId) {
    EXPECT_GT(repo.addDepartment("Mathematics"), 1);
}

TEST_F(CourseRepoTest, DuplicateDepartmentNameIsRejected) {
    EXPECT_THROW(repo.addDepartment("Computer Science"), DatabaseError);
}

TEST_F(CourseRepoTest, BlankDepartmentNameIsRejected) {
    EXPECT_THROW(repo.addDepartment(""), std::invalid_argument);
    EXPECT_THROW(repo.addDepartment("   "), std::invalid_argument);
}

TEST_F(CourseRepoTest, AddAndFindById) {
    const int id = repo.add(Course(0, "CS4433", "Software Engineering", 1));
    EXPECT_GT(id, 0);
    auto found = repo.findById(id);
    ASSERT_TRUE(found.has_value());
    EXPECT_EQ(found->id(), id);
    EXPECT_EQ(found->code(), "CS4433");
    EXPECT_EQ(found->title(), "Software Engineering");
    EXPECT_EQ(found->departmentId(), 1);
}

TEST_F(CourseRepoTest, MissingCourseIsEmpty) {
    EXPECT_FALSE(repo.findById(99).has_value());
}

TEST_F(CourseRepoTest, DuplicateCodeIsRejected) {
    addCourse("CS4433");
    EXPECT_THROW(addCourse("CS4433"), DatabaseError);
}

TEST_F(CourseRepoTest, UnknownDepartmentIsRejected) {
    EXPECT_THROW(addCourse("CS1", 99), DatabaseError);
}

TEST_F(CourseRepoTest, SavedCourseCannotBeAddedAgain) {
    EXPECT_THROW(repo.add(Course(5, "CS1", "Title", 1)), std::invalid_argument);
}

TEST_F(CourseRepoTest, FindByDepartmentIsOrderedByCodeAndFiltered) {
    const int maths = repo.addDepartment("Mathematics");
    addCourse("CS2000");
    addCourse("CS1000");
    addCourse("MA1000", maths);
    auto list = repo.findByDepartment(1);
    ASSERT_EQ(list.size(), 2u);
    EXPECT_EQ(list[0].code(), "CS1000");
    EXPECT_EQ(list[1].code(), "CS2000");
    EXPECT_EQ(repo.findByDepartment(maths).size(), 1u);
}

TEST_F(CourseRepoTest, CodeWithQuotesIsStoredAsData) {
    const std::string code = "x'); DROP TABLE course;--";
    const int id = addCourse(code);
    EXPECT_EQ(repo.findById(id)->code(), code);
}

// ---- Enrolment (FR5) ----
TEST_F(CourseRepoTest, EnrolStudentThenIsEnrolled) {
    const int course = addCourse("CS1");
    repo.enrolStudent(course, 2);
    EXPECT_TRUE(repo.isStudentEnrolled(2, course));
    EXPECT_FALSE(repo.isStudentEnrolled(3, course));
}

TEST_F(CourseRepoTest, EnrolmentIsPerCourse) {
    const int a = addCourse("CS1");
    const int b = addCourse("CS2");
    repo.enrolStudent(a, 2);
    EXPECT_FALSE(repo.isStudentEnrolled(2, b));
}

TEST_F(CourseRepoTest, EnrolTwiceIsHarmless) {
    const int course = addCourse("CS1");
    repo.enrolStudent(course, 2);
    EXPECT_NO_THROW(repo.enrolStudent(course, 2));
    EXPECT_EQ(count("enrolment"), 1);
}

TEST_F(CourseRepoTest, EnrolUnknownCourseIsRejected) {
    EXPECT_THROW(repo.enrolStudent(99, 2), DatabaseError);
}

TEST_F(CourseRepoTest, OnlyStudentsCanBeEnrolled) {
    const int course = addCourse("CS1");
    EXPECT_THROW(repo.enrolStudent(course, 1), DatabaseError);   // a lecturer
    EXPECT_THROW(repo.enrolStudent(course, 99), DatabaseError);  // nobody
    EXPECT_EQ(count("enrolment"), 0);
}

// ---- Lecturer assignment (FR5) ----
TEST_F(CourseRepoTest, AssignLecturerThenIsAssigned) {
    const int course = addCourse("CS1");
    repo.assignLecturer(course, 1);
    EXPECT_TRUE(repo.isLecturerAssigned(1, course));
    EXPECT_FALSE(repo.isLecturerAssigned(4, course));
}

TEST_F(CourseRepoTest, AssignTwiceIsHarmless) {
    const int course = addCourse("CS1");
    repo.assignLecturer(course, 1);
    EXPECT_NO_THROW(repo.assignLecturer(course, 1));
    EXPECT_EQ(count("course_lecturer"), 1);
}

TEST_F(CourseRepoTest, OnlyLecturersCanBeAssigned) {
    const int course = addCourse("CS1");
    EXPECT_THROW(repo.assignLecturer(course, 2), std::invalid_argument);   // a student
    EXPECT_THROW(repo.assignLecturer(course, 99), std::invalid_argument);  // nobody
    EXPECT_EQ(count("course_lecturer"), 0);
}

TEST_F(CourseRepoTest, AssignToUnknownCourseIsRejected) {
    EXPECT_THROW(repo.assignLecturer(99, 1), DatabaseError);
}

TEST_F(CourseRepoTest, TwoLecturersCanTeachTheSameCourse) {
    const int course = addCourse("CS1");
    repo.assignLecturer(course, 1);
    repo.assignLecturer(course, 4);
    EXPECT_TRUE(repo.isLecturerAssigned(1, course));
    EXPECT_TRUE(repo.isLecturerAssigned(4, course));
}

// ---- Lists (FR8) ----
TEST_F(CourseRepoTest, FindForStudentListsOnlyEnrolledCoursesOrderedByCode) {
    const int a = addCourse("CS2000");
    const int b = addCourse("CS1000");
    addCourse("CS3000");
    repo.enrolStudent(a, 2);
    repo.enrolStudent(b, 2);
    repo.enrolStudent(a, 3);
    auto list = repo.findForStudent(2);
    ASSERT_EQ(list.size(), 2u);
    EXPECT_EQ(list[0].code(), "CS1000");
    EXPECT_EQ(list[1].code(), "CS2000");
    EXPECT_EQ(repo.findForStudent(3).size(), 1u);
    EXPECT_TRUE(repo.findForStudent(99).empty());
}

TEST_F(CourseRepoTest, FindForLecturerListsOnlyTheirCourses) {
    const int a = addCourse("CS1000");
    const int b = addCourse("CS2000");
    repo.assignLecturer(a, 1);
    repo.assignLecturer(b, 4);
    auto list = repo.findForLecturer(1);
    ASSERT_EQ(list.size(), 1u);
    EXPECT_EQ(list[0].code(), "CS1000");
    EXPECT_TRUE(repo.findForLecturer(99).empty());
}

TEST_F(CourseRepoTest, FindStudentsListsTheEnrolledStudentsOrderedByStudentNumber) {
    const int course = addCourse("CS1");
    const int other = addCourse("CS2");
    repo.enrolStudent(course, 3);  // S002
    repo.enrolStudent(course, 2);  // S001
    repo.enrolStudent(other, 2);
    auto list = repo.findStudents(course);
    ASSERT_EQ(list.size(), 2u);
    EXPECT_EQ(list[0].id, 2);
    EXPECT_EQ(list[0].name, "Student One");
    EXPECT_EQ(list[0].number, "S001");
    EXPECT_EQ(list[1].id, 3);
    EXPECT_EQ(list[1].number, "S002");
    EXPECT_EQ(repo.findStudents(other).size(), 1u);
    EXPECT_TRUE(repo.findStudents(99).empty());
}
