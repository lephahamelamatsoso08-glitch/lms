#pragma once

#include <string>

namespace lms::domain {

// A course in a department (FR4).
class Course {
public:
    Course(int id, std::string code, std::string title, int departmentId);

    int id() const { return id_; }  // 0 means "not saved yet"
    const std::string& code() const { return code_; }
    const std::string& title() const { return title_; }
    int departmentId() const { return departmentId_; }

private:
    int id_;
    std::string code_;
    std::string title_;
    int departmentId_;
};

}  // namespace lms::domain
