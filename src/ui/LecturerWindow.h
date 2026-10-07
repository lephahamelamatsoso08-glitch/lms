#pragma once

#include <functional>
#include <memory>
#include <vector>

#include <QString>

#include "AppContext.h"
#include "Assignment.h"
#include "LecturerViewService.h"
#include "User.h"
#include "UserWindow.h"

class QLabel;
class QListWidget;
class QPushButton;
class QTableWidget;

namespace lms::ui {

// The lecturer's pages (FR6, FR7, FR12). The window contains no business rules: it calls
// the services and shows the result. Every error becomes a message.
class LecturerWindow : public UserWindow {
    Q_OBJECT

public:
    LecturerWindow(lms::app::AppContext& context, std::shared_ptr<lms::domain::User> user,
                   QWidget* parent = nullptr);

    // The buttons open a form and then call these. They are public so they can be tested.
    void createAssignment(const QString& title, lms::domain::TimePoint deadline, double maxMark);
    void markSubmission(int submissionId, double rawMark, const QString& feedback,
                        const QString& reason);
    void grantExtension(int studentId, lms::domain::TimePoint newDeadline, const QString& reason);

public slots:
    void refresh();

private slots:
    void courseChanged();
    void assignmentChanged();
    void submissionChanged();
    void newAssignment();
    void chooseMark();
    void chooseExtension();
    void openLatestFile();

private:
    void guarded(const std::function<void()>& action);
    void showMessage(const QString& text, bool isError);
    int currentCourseId() const;
    const lms::domain::Assignment* currentAssignment() const;
    int currentRowIndex() const;
    const lms::application::LecturerSubmissionRow* currentRow() const;
    void loadCourses();
    void loadAssignments();
    void loadSubmissions();
    void updateButtons();

    lms::app::AppContext& context_;
    std::shared_ptr<lms::domain::User> user_;
    std::vector<lms::domain::Assignment> assignments_;
    std::vector<lms::application::LecturerSubmissionRow> rows_;

    QListWidget* courseList_;
    QTableWidget* assignmentTable_;
    QTableWidget* submissionTable_;
    QLabel* summaryLabel_;
    QLabel* messageLabel_;
    QPushButton* newAssignmentButton_;
    QPushButton* markButton_;
    QPushButton* extensionButton_;
};

}  // namespace lms::ui
