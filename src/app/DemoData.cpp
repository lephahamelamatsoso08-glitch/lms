#include "DemoData.h"

#include <chrono>
#include <string>

#include "UserFactory.h"

namespace lms::app {

using lms::domain::Role;
using lms::domain::UserData;

namespace {

UserData person(const std::string& username, const std::string& fullName) {
    UserData data;
    data.username = username;
    data.fullName = fullName;
    return data;
}

}  // namespace

void seedDemoData(AppContext& context) {
    context.createFirstAdmin("admin", "System Admin", kDemoPassword);  // refuses if users exist
    const auto admin = context.auth().login("admin", kDemoPassword);
    auto& service = context.admin();

    const int department = service.createDepartment(*admin, "Computer Science");
    const int software = service.createCourse(*admin, "CS4433", "Software Engineering", department);
    const int databases = service.createCourse(*admin, "CS3300", "Databases", department);

    const int lecturer =
        service.createUser(*admin, Role::Lecturer, person("lecturer1", "Lecturer One"), kDemoPassword);

    UserData head = person("hod1", "Head One");
    head.departmentId = department;
    service.createUser(*admin, Role::HoD, head, kDemoPassword);

    UserData first = person("student1", "Student One");
    first.studentNumber = "S001";
    const int student1 = service.createUser(*admin, Role::Student, first, kDemoPassword);
    UserData second = person("student2", "Student Two");
    second.studentNumber = "S002";
    const int student2 = service.createUser(*admin, Role::Student, second, kDemoPassword);

    service.assignLecturer(*admin, software, lecturer);
    service.assignLecturer(*admin, databases, lecturer);
    service.enrolStudent(*admin, software, student1);
    service.enrolStudent(*admin, software, student2);
    service.enrolStudent(*admin, databases, student1);

    const auto teacher = context.auth().login("lecturer1", kDemoPassword);
    const auto now = std::chrono::system_clock::now();
    using std::chrono::hours;
    context.assignments().createAssignment(*teacher, software, "Assignment 1", now + hours(24 * 5), 100.0);
    context.assignments().createAssignment(*teacher, software, "Assignment 2", now + hours(24 * 12), 50.0);
    context.assignments().createAssignment(*teacher, databases, "Lab 1", now + hours(24 * 3), 20.0);
}

}  // namespace lms::app
