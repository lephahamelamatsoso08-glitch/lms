#pragma once

#include <functional>
#include <memory>
#include <vector>

#include <QString>

#include "AppContext.h"
#include "Assignment.h"
#include "HodViewService.h"
#include "User.h"
#include "UserWindow.h"

class QLabel;
class QListWidget;
class QPushButton;
class QTableWidget;

namespace lms::ui {

// The head of department's pages (FR13, FR14, FR19). The window contains no business rules:
// it calls the services and shows the result. Every error becomes a message.
class HodWindow : public UserWindow {
    Q_OBJECT

public:
    HodWindow(lms::app::AppContext& context, std::shared_ptr<lms::domain::User> user,
              QWidget* parent = nullptr);

    // The buttons call these (after a form or a question). They are public for testing.
    void approveSubmission(int submissionId);
    void approveAllMarked();
    void changeMark(int submissionId, double newMark, const QString& reason);
    void releaseMarks();

public slots:
    void refresh();

private slots:
    void courseChanged();
    void assignmentChanged();
    void rowChanged();
    void approveSelected();
    void chooseChange();
    void confirmRelease();
    void showHistory();

private:
    void guarded(const std::function<void()>& action);
    void showMessage(const QString& text, bool isError);
    int currentCourseId() const;
    const lms::domain::Assignment* currentAssignment() const;
    const lms::application::HodSubmissionRow* currentRow() const;
    int countWithStatus(lms::domain::SubmissionStatus status) const;
    void loadCourses();
    void loadAssignments();
    void loadRows();
    void updateButtons();

    lms::app::AppContext& context_;
    std::shared_ptr<lms::domain::User> user_;
    std::vector<lms::domain::Assignment> assignments_;
    std::vector<lms::application::HodSubmissionRow> rows_;

    QListWidget* courseList_;
    QTableWidget* assignmentTable_;
    QTableWidget* workTable_;
    QLabel* summaryLabel_;
    QLabel* messageLabel_;
    QPushButton* approveButton_;
    QPushButton* approveAllButton_;
    QPushButton* changeButton_;
    QPushButton* releaseButton_;
    QPushButton* historyButton_;
};

}  // namespace lms::ui
