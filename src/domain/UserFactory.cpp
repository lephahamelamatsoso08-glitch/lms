#include "UserFactory.h"

#include <stdexcept>

#include "UserRoles.h"

namespace lms::domain {

std::unique_ptr<User> UserFactory::create(Role role, const UserData& data) {
    switch (role) {
        case Role::Admin:
            return std::make_unique<Admin>(data.id, data.username, data.fullName, data.active);
        case Role::Lecturer:
            return std::make_unique<Lecturer>(data.id, data.username, data.fullName, data.active);
        case Role::HoD:
            return std::make_unique<HoD>(data.id, data.username, data.fullName, data.active,
                                         data.departmentId);
        case Role::Student:
            return std::make_unique<Student>(data.id, data.username, data.fullName, data.active,
                                             data.studentNumber);
    }
    throw std::invalid_argument("unknown role");
}

}  // namespace lms::domain
