#include "AdminWindow.h"

#include <exception>
#include <stdexcept>
#include <utility>

#include <QAbstractItemView>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QStringList>
#include <QTableWidget>
#include <QTabWidget>
#include <QVBoxLayout>

#include "AccessDeniedError.h"
#include "AdminDialogs.h"
#include "DatabaseError.h"
#include "RoleWindow.h"
#include "UiFormat.h"

namespace lms::ui {

using lms::application::AccessDeniedError;
using lms::application::AdminCourseRow;
using lms::application::AdminUserRow;
using lms::domain::Role;

namespace {

int selectedRow(const QTableWidget* table) {
    const QModelIndexList rows = table->selectionModel()->selectedRows();
    return rows.isEmpty() ? -1 : rows.first().row();
}

QTableWidget* makeTable(QWidget* parent, const char* name, const QStringList& headers) {
    auto* table = new QTableWidget(0, headers.size(), parent);
    table->setObjectName(name);
    table->setHorizontalHeaderLabels(headers);
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->setSelectionMode(QAbstractItemView::SingleSelection);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->horizontalHeader()->setStretchLastSection(true);
    table->verticalHeader()->setVisible(false);
    return table;
}

QPushButton* makeButton(QWidget* parent, const char* name, const QString& text) {
    auto* button = new QPushButton(text, parent);
    button->setObjectName(name);
    return button;
}

const char* const kDatabaseRefused =
    "The database refused this change. A username, student number, department name or course "
    "code may already exist, or the department already has a head of department.";

}  // namespace

AdminWindow::AdminWindow(lms::app::AppContext& context, std::shared_ptr<lms::domain::User> user,
                         QWidget* parent)
    : UserWindow(parent), context_(context), user_(std::move(user)) {
    setWindowTitle("Moderated LMS - Administrator");

    auto* welcome = new QLabel(
        "Logged in as " + QString::fromStdString(user_->fullName()) + " (Administrator)", this);
    welcome->setObjectName("welcomeLabel");
    auto* refreshButton = makeButton(this, "refreshButton", "&Refresh");
    auto* logoutButton = makeButton(this, "logoutButton", "Log &out");
    auto* top = new QHBoxLayout();
    top->addWidget(welcome, 1);
    top->addWidget(refreshButton);
    top->addWidget(logoutButton);

    // ---- Users ----
    userTable_ = makeTable(this, "userTable",
                           QStringList{"Username", "Full name", "Role", "Details", "Status"});
    auto* newUserButton = makeButton(this, "newUserButton", "&New user...");
    toggleButton_ = makeButton(this, "toggleButton", "&Deactivate");
    resetButton_ = makeButton(this, "resetButton", "&Reset password...");
    auto* userActions = new QHBoxLayout();
    userActions->addWidget(newUserButton);
    userActions->addWidget(toggleButton_);
    userActions->addWidget(resetButton_);
    userActions->addStretch();
    auto* usersPage = new QWidget(this);
    auto* usersLayout = new QVBoxLayout(usersPage);
    usersLayout->addWidget(userTable_, 1);
    usersLayout->addLayout(userActions);

    // ---- Courses ----
    courseTable_ = makeTable(this, "courseTable",
                             QStringList{"Code", "Title", "Department", "Lecturers", "Students"});
    auto* newDepartmentButton = makeButton(this, "newDepartmentButton", "New &department...");
    auto* newCourseButton = makeButton(this, "newCourseButton", "New &course...");
    assignButton_ = makeButton(this, "assignButton", "Assign &lecturer...");
    enrolButton_ = makeButton(this, "enrolButton", "&Enrol student...");
    auto* courseActions = new QHBoxLayout();
    courseActions->addWidget(newDepartmentButton);
    courseActions->addWidget(newCourseButton);
    courseActions->addWidget(assignButton_);
    courseActions->addWidget(enrolButton_);
    courseActions->addStretch();
    auto* coursesPage = new QWidget(this);
    auto* coursesLayout = new QVBoxLayout(coursesPage);
    coursesLayout->addWidget(courseTable_, 1);
    coursesLayout->addLayout(courseActions);

    // ---- Audit log ----
    auditTable_ = makeTable(this, "auditTable",
                            QStringList{"When", "Submission", "Changed by", "Old mark", "New mark", "Reason"});
    auto* auditPage = new QWidget(this);
    auto* auditLayout = new QVBoxLayout(auditPage);
    auditLayout->addWidget(new QLabel("Every change of a mark in the system.", auditPage));
    auditLayout->addWidget(auditTable_, 1);

    tabs_ = new QTabWidget(this);
    tabs_->setObjectName("tabs");
    tabs_->addTab(usersPage, "Users");
    tabs_->addTab(coursesPage, "Courses");
    tabs_->addTab(auditPage, "Audit log");

    messageLabel_ = new QLabel(this);
    messageLabel_->setObjectName("messageLabel");
    messageLabel_->setWordWrap(true);

    auto* centre = new QWidget(this);
    auto* layout = new QVBoxLayout(centre);
    layout->addLayout(top);
    layout->addWidget(tabs_, 1);
    layout->addWidget(messageLabel_);
    setCentralWidget(centre);
    resize(980, 640);

    connect(userTable_, &QTableWidget::itemSelectionChanged, this, &AdminWindow::userSelectionChanged);
    connect(courseTable_, &QTableWidget::itemSelectionChanged, this, &AdminWindow::courseSelectionChanged);
    connect(newUserButton, &QPushButton::clicked, this, &AdminWindow::newUser);
    connect(toggleButton_, &QPushButton::clicked, this, &AdminWindow::toggleActive);
    connect(resetButton_, &QPushButton::clicked, this, &AdminWindow::chooseResetPassword);
    connect(newDepartmentButton, &QPushButton::clicked, this, &AdminWindow::newDepartment);
    connect(newCourseButton, &QPushButton::clicked, this, &AdminWindow::newCourse);
    connect(assignButton_, &QPushButton::clicked, this, &AdminWindow::chooseLecturer);
    connect(enrolButton_, &QPushButton::clicked, this, &AdminWindow::chooseStudent);
    connect(refreshButton, &QPushButton::clicked, this, &AdminWindow::refresh);
    connect(logoutButton, &QPushButton::clicked, this, &UserWindow::loggedOut);

    refresh();
}

// ---- Messages and errors ----
void AdminWindow::showMessage(const QString& text, bool isError) {
    messageLabel_->setStyleSheet(isError ? "color: #b00020;" : "color: #1b5e20;");
    messageLabel_->setText(text);
}

void AdminWindow::guarded(const std::function<void()>& action) {
    try {
        action();
    } catch (const AccessDeniedError& error) {
        showMessage(QString::fromUtf8(error.what()), true);
    } catch (const std::invalid_argument& error) {
        showMessage(QString::fromUtf8(error.what()), true);
    } catch (const lms::data::DatabaseError&) {
        showMessage(kDatabaseRefused, true);
    } catch (const std::logic_error& error) {
        showMessage(QString::fromUtf8(error.what()), true);
    } catch (const std::exception&) {
        showMessage("Something went wrong. Please try again.", true);
    }
}

// ---- Current selection ----
const AdminUserRow* AdminWindow::currentUser() const {
    const int row = selectedRow(userTable_);
    return (row >= 0 && row < static_cast<int>(users_.size())) ? &users_[static_cast<std::size_t>(row)]
                                                                : nullptr;
}

const AdminCourseRow* AdminWindow::currentCourse() const {
    const int row = selectedRow(courseTable_);
    return (row >= 0 && row < static_cast<int>(courses_.size())) ? &courses_[static_cast<std::size_t>(row)]
                                                                  : nullptr;
}

// ---- Loading the lists ----
void AdminWindow::loadUsers() {
    const AdminUserRow* selected = currentUser();
    const int previous = selected ? selected->id : 0;
    userTable_->blockSignals(true);
    userTable_->setRowCount(0);
    users_ = context_.adminView().users(*user_);
    for (const AdminUserRow& row : users_) {
        const int index = userTable_->rowCount();
        userTable_->insertRow(index);
        userTable_->setItem(index, 0, new QTableWidgetItem(QString::fromStdString(row.username)));
        userTable_->setItem(index, 1, new QTableWidgetItem(QString::fromStdString(row.fullName)));
        userTable_->setItem(index, 2, new QTableWidgetItem(RoleWindow::roleLabel(row.role)));
        userTable_->setItem(index, 3, new QTableWidgetItem(QString::fromStdString(row.detail)));
        userTable_->setItem(index, 4, new QTableWidgetItem(row.active ? "Active" : "Inactive"));
        if (row.id == previous) {
            userTable_->selectRow(index);
        }
    }
    userTable_->blockSignals(false);
}

void AdminWindow::loadCourses() {
    const AdminCourseRow* selected = currentCourse();
    const int previous = selected ? selected->id : 0;
    courseTable_->blockSignals(true);
    courseTable_->setRowCount(0);
    departments_ = context_.adminView().departments(*user_);
    courses_ = context_.adminView().courses(*user_);
    for (const AdminCourseRow& row : courses_) {
        const int index = courseTable_->rowCount();
        courseTable_->insertRow(index);
        QStringList lecturers;
        for (const lms::domain::PersonInfo& lecturer : row.lecturers) {
            lecturers << QString::fromStdString(lecturer.name);
        }
        courseTable_->setItem(index, 0, new QTableWidgetItem(QString::fromStdString(row.code)));
        courseTable_->setItem(index, 1, new QTableWidgetItem(QString::fromStdString(row.title)));
        courseTable_->setItem(index, 2, new QTableWidgetItem(QString::fromStdString(row.department)));
        courseTable_->setItem(index, 3, new QTableWidgetItem(lecturers.join(", ")));
        courseTable_->setItem(index, 4, new QTableWidgetItem(QString::number(row.studentCount)));
        if (row.id == previous) {
            courseTable_->selectRow(index);
        }
    }
    courseTable_->blockSignals(false);
}

void AdminWindow::loadAudit() {
    const auto log = context_.adminView().auditLog(*user_);
    auditTable_->setRowCount(0);
    for (const lms::application::AdminAuditRow& row : log) {
        const int index = auditTable_->rowCount();
        auditTable_->insertRow(index);
        auditTable_->setItem(index, 0, new QTableWidgetItem(formatTime(row.changedAt)));
        auditTable_->setItem(index, 1, new QTableWidgetItem(QString::number(row.submissionId)));
        auditTable_->setItem(index, 2, new QTableWidgetItem(QString::fromStdString(row.changedBy)));
        auditTable_->setItem(index, 3, new QTableWidgetItem(row.oldValue ? formatMark(*row.oldValue)
                                                                         : QString("(first mark)")));
        auditTable_->setItem(index, 4, new QTableWidgetItem(formatMark(row.newValue)));
        auditTable_->setItem(index, 5, new QTableWidgetItem(QString::fromStdString(row.reason)));
    }
}

void AdminWindow::loadAll() {
    loadUsers();
    loadCourses();
    loadAudit();
    updateButtons();
}

void AdminWindow::updateButtons() {
    const AdminUserRow* row = currentUser();
    resetButton_->setEnabled(row != nullptr);
    toggleButton_->setEnabled(row != nullptr);
    toggleButton_->setText(row != nullptr && !row->active ? "&Reactivate" : "&Deactivate");
    const bool hasCourse = currentCourse() != nullptr;
    assignButton_->setEnabled(hasCourse);
    enrolButton_->setEnabled(hasCourse);
}

// ---- Slots ----
void AdminWindow::refresh() {
    showMessage("", false);
    guarded([this]() { loadAll(); });
}

void AdminWindow::userSelectionChanged() { updateButtons(); }
void AdminWindow::courseSelectionChanged() { updateButtons(); }

void AdminWindow::newUser() {
    NewUserDialog dialog(departments_, this);
    if (dialog.exec() == QDialog::Accepted) {
        createUser(dialog.role(), dialog.userData(), dialog.password());
    }
}

void AdminWindow::toggleActive() {
    const AdminUserRow* row = currentUser();
    if (row == nullptr) {
        showMessage("Select a user first.", true);
        return;
    }
    setUserActive(row->id, !row->active);
}

void AdminWindow::chooseResetPassword() {
    const AdminUserRow* row = currentUser();
    if (row == nullptr) {
        showMessage("Select a user first.", true);
        return;
    }
    const int id = row->id;
    PasswordDialog dialog(QString::fromStdString(row->username), this);
    if (dialog.exec() == QDialog::Accepted) {
        resetPassword(id, dialog.password());
    }
}

void AdminWindow::newDepartment() {
    TextDialog dialog("New department", "&Department name:", this);
    if (dialog.exec() == QDialog::Accepted) {
        createDepartment(dialog.text());
    }
}

void AdminWindow::newCourse() {
    if (departments_.empty()) {
        showMessage("Create a department first.", true);
        return;
    }
    CourseDialog dialog(departments_, this);
    if (dialog.exec() == QDialog::Accepted) {
        createCourse(dialog.code(), dialog.title(), dialog.departmentId());
    }
}

void AdminWindow::chooseLecturer() {
    const AdminCourseRow* course = currentCourse();
    if (course == nullptr) {
        showMessage("Select a course first.", true);
        return;
    }
    const int courseId = course->id;
    std::vector<std::pair<int, QString>> items;
    for (const AdminUserRow& row : users_) {
        if (row.role == Role::Lecturer && row.active) {
            items.emplace_back(row.id, QString::fromStdString(row.fullName));
        }
    }
    ChoiceDialog dialog("Assign a lecturer", "&Lecturer:", items, this);
    if (dialog.exec() == QDialog::Accepted) {
        assignLecturer(courseId, dialog.selectedId());
    }
}

void AdminWindow::chooseStudent() {
    const AdminCourseRow* course = currentCourse();
    if (course == nullptr) {
        showMessage("Select a course first.", true);
        return;
    }
    const int courseId = course->id;
    std::vector<std::pair<int, QString>> items;
    for (const AdminUserRow& row : users_) {
        if (row.role == Role::Student && row.active) {
            items.emplace_back(row.id, QString::fromStdString(row.detail + " - " + row.fullName));
        }
    }
    ChoiceDialog dialog("Enrol a student", "&Student:", items, this);
    if (dialog.exec() == QDialog::Accepted) {
        enrolStudent(courseId, dialog.selectedId());
    }
}

// ---- Actions ----
void AdminWindow::createUser(Role role, const lms::domain::UserData& data, const QString& password) {
    guarded([&]() {
        context_.admin().createUser(*user_, role, data, password.toStdString());
        loadAll();
        showMessage("User created.", false);
    });
}

void AdminWindow::setUserActive(int userId, bool active) {
    guarded([&]() {
        if (active) {
            context_.admin().reactivateUser(*user_, userId);
        } else {
            context_.admin().deactivateUser(*user_, userId);
        }
        loadAll();
        showMessage(active ? "Account reactivated." : "Account deactivated.", false);
    });
}

void AdminWindow::resetPassword(int userId, const QString& newPassword) {
    guarded([&]() {
        context_.admin().resetPassword(*user_, userId, newPassword.toStdString());
        showMessage("Password reset.", false);
    });
}

void AdminWindow::createDepartment(const QString& name) {
    guarded([&]() {
        context_.admin().createDepartment(*user_, name.toStdString());
        loadAll();
        showMessage("Department created.", false);
    });
}

void AdminWindow::createCourse(const QString& code, const QString& title, int departmentId) {
    guarded([&]() {
        context_.admin().createCourse(*user_, code.toStdString(), title.toStdString(), departmentId);
        loadAll();
        showMessage("Course created.", false);
    });
}

void AdminWindow::assignLecturer(int courseId, int lecturerId) {
    guarded([&]() {
        context_.admin().assignLecturer(*user_, courseId, lecturerId);
        loadAll();
        showMessage("Lecturer assigned.", false);
    });
}

void AdminWindow::enrolStudent(int courseId, int studentId) {
    guarded([&]() {
        context_.admin().enrolStudent(*user_, courseId, studentId);
        loadAll();
        showMessage("Student enrolled.", false);
    });
}

}  // namespace lms::ui
