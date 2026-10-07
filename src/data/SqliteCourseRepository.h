#pragma once

#include <optional>
#include <string>
#include <vector>

#include "DatabaseConnection.h"
#include "ICourseRepository.h"

namespace lms::data {

class SqliteCourseRepository : public lms::domain::ICourseRepository {
public:
    explicit SqliteCourseRepository(DatabaseConnection& db);

    int addDepartment(const std::string& name) override;
    int add(const lms::domain::Course& course) override;
    std::optional<lms::domain::Course> findById(int id) const override;
    std::vector<lms::domain::Course> findByDepartment(int departmentId) const override;
    void enrolStudent(int courseId, int studentId) override;
    void assignLecturer(int courseId, int lecturerId) override;
    bool isStudentEnrolled(int studentId, int courseId) const override;
    bool isLecturerAssigned(int lecturerId, int courseId) const override;
    std::vector<lms::domain::Course> findForStudent(int studentId) const override;
    std::vector<lms::domain::Course> findForLecturer(int lecturerId) const override;
    std::vector<lms::domain::StudentInfo> findStudents(int courseId) const override;

private:
    static lms::domain::Course readCourse(Statement& row);
    std::vector<lms::domain::Course> readAll(Statement& query) const;

    DatabaseConnection& db_;
};

}  // namespace lms::data
