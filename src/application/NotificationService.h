#pragma once

#include <functional>
#include <vector>

#include "AccessDeniedError.h"
#include "IAssignmentRepository.h"
#include "INotificationRepository.h"
#include "IReleaseObserver.h"
#include "ITransactionRunner.h"
#include "LateDays.h"
#include "User.h"

namespace lms::application {

// The Observer that tells students their marks were released (FR18).
// It also lets a user read their own notifications.
class NotificationService : public lms::domain::IReleaseObserver {
public:
    using Clock = std::function<lms::domain::TimePoint()>;

    NotificationService(lms::domain::INotificationRepository& notifications,
                        const lms::domain::IAssignmentRepository& assignments,
                        lms::domain::ITransactionRunner& transactions,
                        Clock clock = &systemNow);

    // One notification per student, saved all together or not at all.
    // std::invalid_argument if the assignment does not exist.
    void onMarksReleased(int assignmentId, const std::vector<int>& studentIds) override;

    // A user sees only their own notifications. Inactive users are refused.
    std::vector<lms::domain::Notification> list(const lms::domain::User& user) const;
    int unreadCount(const lms::domain::User& user) const;

    // std::invalid_argument if the notification does not exist or is not theirs.
    void markRead(const lms::domain::User& user, int notificationId);

private:
    static lms::domain::TimePoint systemNow();
    static void requireActive(const lms::domain::User& user);

    lms::domain::INotificationRepository& notifications_;
    const lms::domain::IAssignmentRepository& assignments_;
    lms::domain::ITransactionRunner& transactions_;
    Clock clock_;
};

}  // namespace lms::application
