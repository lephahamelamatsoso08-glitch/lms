#pragma once

#include <functional>
#include <memory>
#include <vector>

#include <QString>

#include "AdminViewService.h"
#include "AppContext.h"
#include "Role.h"
#include "User.h"
#include "UserFactory.h"
#include "UserWindow.h"

class QLabel;
class QPushButton;
class QTableWidget;
class QTabWidget;

namespace lms::ui {

// The administrator's pages (FR3, FR4, FR5, FR19, FR20). The window contains no business
// rules: it calls the services and shows the result. Every error becomes a message.
class AdminWindow : public UserWindow {
    Q_OBJECT

public:
    AdminWindow(lms::app::AppContext& context, std::shared_ptr<lms::domain::User> user,
                QWidget* parent = nullptr);

    // The buttons open a form and then call these. They are public so they can be tested.
    void createUser(lms::domain::Role role, const lms::domain::UserData& data,
                    const QString& password);
    void setUserActive(int userId, bool active);
    void resetPassword(int userId, const QString& newPassword);
    void createDepartment(const QString& name);
    void createCourse(const QString& code, const QString& title, int departmentId);
    void assignLecturer(int courseId, int lecturerId);
    void enrolStudent(int courseId, int studentId);

public slots:
    void refresh();

private slots:
    void userSelectionChanged();
    void courseSelectionChanged();
    void newUser();
    void toggleActive();
    void chooseResetPassword();
    void newDepartment();
    void newCourse();
    void chooseLecturer();
    void chooseStudent();

private:
    void guarded(const std::function<void()>& action);
    void showMessage(const QString& text, bool isError);
    const lms::application::AdminUserRow* currentUser() const;
    const lms::application::AdminCourseRow* currentCourse() const;
    void loadAll();
    void loadUsers();
    void loadCourses();
    void loadAudit();
    void updateButtons();

    lms::app::AppContext& context_;
    std::shared_ptr<lms::domain::User> user_;
    std::vector<lms::application::AdminUserRow> users_;
    std::vector<lms::application::AdminCourseRow> courses_;
    std::vector<lms::domain::DepartmentInfo> departments_;

    QTabWidget* tabs_;
    QTableWidget* userTable_;
    QTableWidget* courseTable_;
    QTableWidget* auditTable_;
    QLabel* messageLabel_;
    QPushButton* toggleButton_;
    QPushButton* resetButton_;
    QPushButton* assignButton_;
    QPushButton* enrolButton_;
};

}  // namespace lms::ui
