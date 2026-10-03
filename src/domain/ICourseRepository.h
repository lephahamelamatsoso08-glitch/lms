#pragma once

#include <optional>
#include <string>
#include <vector>

#include "Course.h"

namespace lms::domain {

// Courses, enrolment and lecturer assignment (FR4, FR5, FR8).
class ICourseRepository {
public:
    virtual ~ICourseRepository() = default;

    // Departments only have a name here (the HoD link is stored with the user).
    virtual int addDepartment(const std::string& name) = 0;

    // Saves a new course (its id must be 0) and returns the new id.
    virtual int add(const Course& course) = 0;
    virtual std::optional<Course> findById(int id) const = 0;
    virtual std::vector<Course> findByDepartment(int departmentId) const = 0;  // ordered by code

    // FR5. Doing the same thing twice is harmless.
    virtual void enrolStudent(int courseId, int studentId) = 0;
    virtual void assignLecturer(int courseId, int lecturerId) = 0;

    virtual bool isStudentEnrolled(int studentId, int courseId) const = 0;
    virtual bool isLecturerAssigned(int lecturerId, int courseId) const = 0;

    // FR8: the courses a student is enrolled in, and a lecturer teaches.
    virtual std::vector<Course> findForStudent(int studentId) const = 0;
    virtual std::vector<Course> findForLecturer(int lecturerId) const = 0;
};

}  // namespace lms::domain
