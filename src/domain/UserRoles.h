#pragma once

#include <string>

#include "User.h"

namespace lms::domain {

class Admin : public User {
public:
    using User::User;
    Role getRole() const override;
    bool canPerform(Action action) const override;
};

class Lecturer : public User {
public:
    using User::User;
    Role getRole() const override;
    bool canPerform(Action action) const override;
};

class HoD : public User {
public:
    HoD(int id, std::string username, std::string fullName, bool active, int departmentId);
    Role getRole() const override;
    bool canPerform(Action action) const override;
    int departmentId() const { return departmentId_; }

private:
    int departmentId_;
};

class Student : public User {
public:
    Student(int id, std::string username, std::string fullName, bool active,
            std::string studentNumber);
    Role getRole() const override;
    bool canPerform(Action action) const override;
    const std::string& studentNumber() const { return studentNumber_; }

private:
    std::string studentNumber_;
};

}  // namespace lms::domain
