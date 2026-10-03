#include "NotificationService.h"

#include <chrono>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>

#include "Assignment.h"
#include "Notification.h"

namespace lms::application {

using lms::domain::Assignment;
using lms::domain::Notification;
using lms::domain::TimePoint;
using lms::domain::User;

NotificationService::NotificationService(lms::domain::INotificationRepository& notifications,
                                         const lms::domain::IAssignmentRepository& assignments,
                                         lms::domain::ITransactionRunner& transactions,
                                         Clock clock)
    : notifications_(notifications),
      assignments_(assignments),
      transactions_(transactions),
      clock_(std::move(clock)) {
    if (!clock_) {
        clock_ = &NotificationService::systemNow;
    }
}

TimePoint NotificationService::systemNow() { return std::chrono::system_clock::now(); }

void NotificationService::requireActive(const User& user) {
    if (!user.isActive()) {
        throw AccessDeniedError("This account is deactivated.");
    }
}

void NotificationService::onMarksReleased(int assignmentId, const std::vector<int>& studentIds) {
    const std::optional<Assignment> assignment = assignments_.findById(assignmentId);
    if (!assignment) {
        throw std::invalid_argument("Assignment not found.");
    }
    const TimePoint now = clock_();
    const std::string message =
        "Your mark and feedback for \"" + assignment->title() + "\" have been released.";

    transactions_.runInTransaction([&]() {
        for (int studentId : studentIds) {
            notifications_.add(Notification(0, studentId, message, now, false));
        }
    });
}

std::vector<Notification> NotificationService::list(const User& user) const {
    requireActive(user);
    return notifications_.findForUser(user.id());
}

int NotificationService::unreadCount(const User& user) const {
    requireActive(user);
    return notifications_.countUnread(user.id());
}

void NotificationService::markRead(const User& user, int notificationId) {
    requireActive(user);
    if (!notifications_.markRead(user.id(), notificationId)) {
        throw std::invalid_argument("Notification not found.");
    }
}

}  // namespace lms::application
