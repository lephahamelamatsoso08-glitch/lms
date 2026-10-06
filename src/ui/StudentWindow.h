#pragma once

#include <functional>
#include <memory>

#include <QString>

#include "AppContext.h"
#include "StudentViewService.h"
#include "User.h"
#include "UserWindow.h"

class QLabel;
class QListWidget;
class QPushButton;
class QTableWidget;
class QTabWidget;

namespace lms::ui {

// The student's pages (FR8, FR9, FR10, FR17, FR18). The window contains no business
// rules: it calls the services and shows the result. Every error becomes a message.
class StudentWindow : public UserWindow {
    Q_OBJECT

public:
    StudentWindow(lms::app::AppContext& context, std::shared_ptr<lms::domain::User> user,
                  QWidget* parent = nullptr);

    // The text for one submission. Marks and feedback only appear when the service
    // has put them in the view, and it does so only after release (rule C1).
    static QString describe(const lms::application::StudentSubmissionView& view);

    // Submits (or resubmits) a file for the selected assignment. The "Submit" button
    // asks for a file and then calls this. It is public so it can be tested.
    void submitFile(const QString& path);

public slots:
    void refresh();

private slots:
    void courseChanged();
    void assignmentChanged();
    void chooseAndSubmit();
    void markSelectedRead();

private:
    void guarded(const std::function<void()>& action);
    void showMessage(const QString& text, bool isError);
    int currentCourseId() const;
    int currentAssignmentId() const;
    void loadCourses();
    void loadAssignments();
    void showDetail();
    void loadNotifications();

    lms::app::AppContext& context_;
    std::shared_ptr<lms::domain::User> user_;
    QTabWidget* tabs_;
    QListWidget* courseList_;
    QTableWidget* assignmentTable_;
    QLabel* detailLabel_;
    QPushButton* submitButton_;
    QLabel* messageLabel_;
    QListWidget* notificationList_;
    QPushButton* markReadButton_;
};

}  // namespace lms::ui
