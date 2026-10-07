#include "LecturerWindow.h"

#include <exception>
#include <stdexcept>
#include <utility>

#include <QAbstractItemView>
#include <QDesktopServices>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QStringList>
#include <QTableWidget>
#include <QUrl>
#include <QVBoxLayout>

#include "AccessDeniedError.h"
#include "Course.h"
#include "LecturerDialogs.h"
#include "SubmissionStatus.h"
#include "UiFormat.h"

namespace lms::ui {

using lms::application::AccessDeniedError;
using lms::application::LecturerSubmissionRow;
using lms::domain::Assignment;
using lms::domain::SubmissionStatus;

namespace {

int selectedRow(const QTableWidget* table) {
    const QModelIndexList rows = table->selectionModel()->selectedRows();
    return rows.isEmpty() ? -1 : rows.first().row();
}

QString statusText(const LecturerSubmissionRow& row) {
    if (!row.submitted || !row.status) {
        return "Not submitted";
    }
    switch (*row.status) {
        case SubmissionStatus::Submitted: return "Submitted";
        case SubmissionStatus::Late:      return "Submitted late";
        case SubmissionStatus::Marked:    return "Marked";
        case SubmissionStatus::Moderated: return "Moderated";
        case SubmissionStatus::Released:  return "Released";
    }
    return "Unknown";
}

QString markText(const LecturerSubmissionRow& row) {
    if (!row.finalMark) {
        return QString();
    }
    QString text = formatMark(*row.finalMark);
    if (row.rawMark && *row.rawMark > *row.finalMark) {
        text += " (marked " + formatMark(*row.rawMark) + ")";
    }
    return text;
}

}  // namespace

LecturerWindow::LecturerWindow(lms::app::AppContext& context,
                               std::shared_ptr<lms::domain::User> user, QWidget* parent)
    : UserWindow(parent), context_(context), user_(std::move(user)) {
    setWindowTitle("Moderated LMS - Lecturer");

    auto* welcome = new QLabel(
        "Logged in as " + QString::fromStdString(user_->fullName()) + " (Lecturer)", this);
    welcome->setObjectName("welcomeLabel");
    auto* refreshButton = new QPushButton("&Refresh", this);
    refreshButton->setObjectName("refreshButton");
    auto* logoutButton = new QPushButton("Log &out", this);
    logoutButton->setObjectName("logoutButton");

    auto* top = new QHBoxLayout();
    top->addWidget(welcome, 1);
    top->addWidget(refreshButton);
    top->addWidget(logoutButton);

    courseList_ = new QListWidget(this);
    courseList_->setObjectName("courseList");
    courseList_->setMaximumHeight(100);

    assignmentTable_ = new QTableWidget(0, 3, this);
    assignmentTable_->setObjectName("assignmentTable");
    assignmentTable_->setHorizontalHeaderLabels(
        QStringList{"Assignment", "Deadline", "Maximum mark"});
    assignmentTable_->setSelectionBehavior(QAbstractItemView::SelectRows);
    assignmentTable_->setSelectionMode(QAbstractItemView::SingleSelection);
    assignmentTable_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    assignmentTable_->horizontalHeader()->setStretchLastSection(true);
    assignmentTable_->verticalHeader()->setVisible(false);
    assignmentTable_->setMaximumHeight(170);

    newAssignmentButton_ = new QPushButton("&New assignment...", this);
    newAssignmentButton_->setObjectName("newAssignmentButton");

    submissionTable_ = new QTableWidget(0, 7, this);
    submissionTable_->setObjectName("submissionTable");
    submissionTable_->setHorizontalHeaderLabels(QStringList{
        "Student number", "Student", "Status", "Late days", "Files", "Latest file", "Mark"});
    submissionTable_->setSelectionBehavior(QAbstractItemView::SelectRows);
    submissionTable_->setSelectionMode(QAbstractItemView::SingleSelection);
    submissionTable_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    submissionTable_->horizontalHeader()->setStretchLastSection(true);
    submissionTable_->verticalHeader()->setVisible(false);

    summaryLabel_ = new QLabel(this);
    summaryLabel_->setObjectName("summaryLabel");

    markButton_ = new QPushButton("&Mark or re-mark...", this);
    markButton_->setObjectName("markButton");
    extensionButton_ = new QPushButton("Grant &extension...", this);
    extensionButton_->setObjectName("extensionButton");
    auto* hint = new QLabel("Double-click a student to open the latest file.", this);

    auto* actions = new QHBoxLayout();
    actions->addWidget(markButton_);
    actions->addWidget(extensionButton_);
    actions->addWidget(hint, 1);

    messageLabel_ = new QLabel(this);
    messageLabel_->setObjectName("messageLabel");
    messageLabel_->setWordWrap(true);

    auto* centre = new QWidget(this);
    auto* layout = new QVBoxLayout(centre);
    layout->addLayout(top);
    layout->addWidget(new QLabel("My courses", this));
    layout->addWidget(courseList_);
    layout->addWidget(new QLabel("Assignments", this));
    layout->addWidget(assignmentTable_);
    layout->addWidget(newAssignmentButton_);
    layout->addWidget(new QLabel("Students", this));
    layout->addWidget(submissionTable_, 1);
    layout->addWidget(summaryLabel_);
    layout->addLayout(actions);
    layout->addWidget(messageLabel_);
    setCentralWidget(centre);
    resize(980, 800);

    connect(courseList_, &QListWidget::currentItemChanged, this, &LecturerWindow::courseChanged);
    connect(assignmentTable_, &QTableWidget::itemSelectionChanged, this,
            &LecturerWindow::assignmentChanged);
    connect(submissionTable_, &QTableWidget::itemSelectionChanged, this,
            &LecturerWindow::submissionChanged);
    connect(submissionTable_, &QTableWidget::cellDoubleClicked, this,
            &LecturerWindow::openLatestFile);
    connect(newAssignmentButton_, &QPushButton::clicked, this, &LecturerWindow::newAssignment);
    connect(markButton_, &QPushButton::clicked, this, &LecturerWindow::chooseMark);
    connect(extensionButton_, &QPushButton::clicked, this, &LecturerWindow::chooseExtension);
    connect(refreshButton, &QPushButton::clicked, this, &LecturerWindow::refresh);
    connect(logoutButton, &QPushButton::clicked, this, &UserWindow::loggedOut);

    refresh();
}

// ---- Messages and errors ----
void LecturerWindow::showMessage(const QString& text, bool isError) {
    messageLabel_->setStyleSheet(isError ? "color: #b00020;" : "color: #1b5e20;");
    messageLabel_->setText(text);
}

void LecturerWindow::guarded(const std::function<void()>& action) {
    try {
        action();
    } catch (const AccessDeniedError& error) {
        showMessage(QString::fromUtf8(error.what()), true);
    } catch (const std::invalid_argument& error) {
        showMessage(QString::fromUtf8(error.what()), true);
    } catch (const std::logic_error& error) {
        showMessage(QString::fromUtf8(error.what()), true);
    } catch (const std::exception&) {
        showMessage("Something went wrong. Please try again.", true);
    }
}

// ---- Current selection ----
int LecturerWindow::currentCourseId() const {
    const QListWidgetItem* item = courseList_->currentItem();
    return item ? item->data(Qt::UserRole).toInt() : 0;
}

const Assignment* LecturerWindow::currentAssignment() const {
    const int row = selectedRow(assignmentTable_);
    if (row < 0 || row >= static_cast<int>(assignments_.size())) {
        return nullptr;
    }
    return &assignments_[static_cast<std::size_t>(row)];
}

int LecturerWindow::currentRowIndex() const {
    const int row = selectedRow(submissionTable_);
    return (row >= 0 && row < static_cast<int>(rows_.size())) ? row : -1;
}

const LecturerSubmissionRow* LecturerWindow::currentRow() const {
    const int row = currentRowIndex();
    return row < 0 ? nullptr : &rows_[static_cast<std::size_t>(row)];
}

// ---- Loading the lists ----
void LecturerWindow::loadCourses() {
    const int previous = currentCourseId();
    courseList_->blockSignals(true);
    courseList_->clear();
    for (const lms::domain::Course& course : context_.assignments().myCourses(*user_)) {
        auto* item = new QListWidgetItem(QString::fromStdString(course.code()) + " - " +
                                         QString::fromStdString(course.title()));
        item->setData(Qt::UserRole, course.id());
        courseList_->addItem(item);
        if (course.id() == previous) {
            courseList_->setCurrentItem(item);
        }
    }
    if (courseList_->currentItem() == nullptr && courseList_->count() > 0) {
        courseList_->setCurrentRow(0);
    }
    courseList_->blockSignals(false);
}

void LecturerWindow::loadAssignments() {
    const Assignment* selected = currentAssignment();
    const int previous = selected ? selected->id() : 0;

    assignmentTable_->blockSignals(true);
    assignmentTable_->setRowCount(0);
    assignments_.clear();
    const int courseId = currentCourseId();
    if (courseId != 0) {
        assignments_ = context_.assignments().listAssignments(*user_, courseId);
    }
    for (const Assignment& assignment : assignments_) {
        const int row = assignmentTable_->rowCount();
        assignmentTable_->insertRow(row);
        assignmentTable_->setItem(row, 0,
                                  new QTableWidgetItem(QString::fromStdString(assignment.title())));
        assignmentTable_->setItem(row, 1, new QTableWidgetItem(formatTime(assignment.deadline())));
        assignmentTable_->setItem(row, 2, new QTableWidgetItem(formatMark(assignment.maxMark())));
        if (assignment.id() == previous) {
            assignmentTable_->selectRow(row);
        }
    }
    assignmentTable_->blockSignals(false);
    loadSubmissions();
}

void LecturerWindow::loadSubmissions() {
    const LecturerSubmissionRow* selected = currentRow();
    const int previousStudent = selected ? selected->studentId : 0;
    const Assignment* assignment = currentAssignment();

    submissionTable_->blockSignals(true);
    submissionTable_->setRowCount(0);
    rows_.clear();
    if (assignment != nullptr) {
        rows_ = context_.lecturerView().rows(*user_, assignment->id());
    }
    for (const LecturerSubmissionRow& row : rows_) {
        const int index = submissionTable_->rowCount();
        submissionTable_->insertRow(index);
        submissionTable_->setItem(index, 0,
                                  new QTableWidgetItem(QString::fromStdString(row.studentNumber)));
        submissionTable_->setItem(index, 1,
                                  new QTableWidgetItem(QString::fromStdString(row.studentName)));
        submissionTable_->setItem(index, 2, new QTableWidgetItem(statusText(row)));
        submissionTable_->setItem(
            index, 3, new QTableWidgetItem(row.submitted ? QString::number(row.lateDays) : ""));
        submissionTable_->setItem(
            index, 4, new QTableWidgetItem(row.submitted ? QString::number(row.versionCount) : ""));
        auto* file = new QTableWidgetItem(
            QFileInfo(QString::fromStdString(row.latestFile)).fileName());
        file->setToolTip(QString::fromStdString(row.latestFile));
        submissionTable_->setItem(index, 5, file);
        submissionTable_->setItem(index, 6, new QTableWidgetItem(markText(row)));
        if (row.studentId == previousStudent) {
            submissionTable_->selectRow(index);
        }
    }
    submissionTable_->blockSignals(false);

    if (assignment == nullptr) {
        summaryLabel_->setText("Select an assignment to see its students.");
    } else {
        int submitted = 0;
        int marked = 0;
        for (const LecturerSubmissionRow& row : rows_) {
            submitted += row.submitted ? 1 : 0;
            marked += row.finalMark.has_value() ? 1 : 0;
        }
        summaryLabel_->setText(QString("%1 students, %2 submitted, %3 marked")
                                   .arg(rows_.size())
                                   .arg(submitted)
                                   .arg(marked));
    }
    updateButtons();
}

void LecturerWindow::updateButtons() {
    newAssignmentButton_->setEnabled(currentCourseId() != 0);
    const LecturerSubmissionRow* row = currentRow();
    extensionButton_->setEnabled(row != nullptr);
    bool canMark = false;
    if (row != nullptr && row->submitted && row->status) {
        canMark = *row->status == SubmissionStatus::Submitted ||
                  *row->status == SubmissionStatus::Late || *row->status == SubmissionStatus::Marked;
    }
    markButton_->setEnabled(canMark);
}

// ---- Slots ----
void LecturerWindow::refresh() {
    showMessage("", false);
    guarded([this]() {
        loadCourses();
        loadAssignments();
    });
}

void LecturerWindow::courseChanged() {
    guarded([this]() { loadAssignments(); });
}

void LecturerWindow::assignmentChanged() {
    guarded([this]() { loadSubmissions(); });
}

void LecturerWindow::submissionChanged() { updateButtons(); }

void LecturerWindow::newAssignment() {
    AssignmentDialog dialog(this);
    if (dialog.exec() == QDialog::Accepted) {
        createAssignment(dialog.title(), dialog.deadline(), dialog.maxMark());
    }
}

void LecturerWindow::chooseMark() {
    const LecturerSubmissionRow* row = currentRow();
    const Assignment* assignment = currentAssignment();
    if (row == nullptr || assignment == nullptr) {
        showMessage("Select a student first.", true);
        return;
    }
    const LecturerSubmissionRow copy = *row;  // the lists may be reloaded while the form is open
    MarkDialog dialog(QString::fromStdString(copy.studentName), assignment->maxMark(), copy.rawMark,
                      QString::fromStdString(copy.feedback), copy.finalMark.has_value(), this);
    if (dialog.exec() == QDialog::Accepted) {
        markSubmission(copy.submissionId, dialog.rawMark(), dialog.feedback(), dialog.reason());
    }
}

void LecturerWindow::chooseExtension() {
    const LecturerSubmissionRow* row = currentRow();
    const Assignment* assignment = currentAssignment();
    if (row == nullptr || assignment == nullptr) {
        showMessage("Select a student first.", true);
        return;
    }
    const LecturerSubmissionRow copy = *row;
    ExtensionDialog dialog(QString::fromStdString(copy.studentName), assignment->deadline(), this);
    if (dialog.exec() == QDialog::Accepted) {
        grantExtension(copy.studentId, dialog.newDeadline(), dialog.reason());
    }
}

void LecturerWindow::openLatestFile() {
    const LecturerSubmissionRow* row = currentRow();
    if (row == nullptr) {
        return;
    }
    if (row->latestFile.empty()) {
        showMessage("This student has not submitted a file.", true);
        return;
    }
    const QString path = QString::fromStdString(row->latestFile);
    if (!QDesktopServices::openUrl(QUrl::fromLocalFile(path))) {
        showMessage("The file could not be opened. It may have been moved or deleted.", true);
    }
}

// ---- Actions ----
void LecturerWindow::createAssignment(const QString& title, lms::domain::TimePoint deadline,
                                      double maxMark) {
    const int courseId = currentCourseId();
    if (courseId == 0) {
        showMessage("Select a course first.", true);
        return;
    }
    guarded([&]() {
        context_.assignments().createAssignment(*user_, courseId, title.trimmed().toStdString(),
                                                deadline, maxMark);
        loadAssignments();
        showMessage("Assignment created.", false);
    });
}

void LecturerWindow::markSubmission(int submissionId, double rawMark, const QString& feedback,
                                    const QString& reason) {
    guarded([&]() {
        const double finalMark = context_.submissions().mark(
            *user_, submissionId, rawMark, feedback.toStdString(), reason.toStdString());
        loadSubmissions();
        QString text = "Mark saved. Final mark: " + formatMark(finalMark) + ".";
        if (finalMark < rawMark) {
            text += " The late penalty was applied.";
        }
        showMessage(text, false);
    });
}

void LecturerWindow::grantExtension(int studentId, lms::domain::TimePoint newDeadline,
                                    const QString& reason) {
    const Assignment* assignment = currentAssignment();
    if (assignment == nullptr) {
        showMessage("Select an assignment first.", true);
        return;
    }
    const int assignmentId = assignment->id();
    guarded([&]() {
        context_.assignments().grantExtension(*user_, assignmentId, studentId, newDeadline,
                                              reason.toStdString());
        loadSubmissions();
        showMessage("Extension granted until " + formatTime(newDeadline) + ".", false);
    });
}

}  // namespace lms::ui
