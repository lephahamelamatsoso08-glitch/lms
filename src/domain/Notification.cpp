#include "Notification.h"

#include <stdexcept>
#include <utility>

namespace lms::domain {

Notification::Notification(int id, int userId, std::string message, TimePoint createdAt,
                           bool isRead)
    : id_(id), userId_(userId), message_(std::move(message)), createdAt_(createdAt),
      isRead_(isRead) {
    if (id_ < 0) {
        throw std::invalid_argument("id must not be negative");
    }
    if (userId_ <= 0) {
        throw std::invalid_argument("a notification needs a user");
    }
    if (message_.find_first_not_of(" \t\r\n") == std::string::npos) {
        throw std::invalid_argument("a notification needs a message");
    }
}

}  // namespace lms::domain
