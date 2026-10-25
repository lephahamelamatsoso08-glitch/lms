#include "HodWindow.h"

#include <algorithm>
#include <algorithm>
#include <exception>
#include <stdexcept>
#include <utility>

#include <QAbstractItemView>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QStringList>
#include <QTableWidget>
#include <QVBoxLayout>

#include "AccessDeniedError.h"
#include "Course.h"
#include "HodDialogs.h"
#include "SubmissionStatus.h"
#include "UiFormat.h"

namespace lms::ui {

using lms::application::AccessDeniedError;
using lms::application::HodSubmissionRow;
using lms::domain::Assignment;
using lms::domain::SubmissionStatus;

namespace {

int selectedRow(const QTableWidget* table) {
    const QModelIndexList rows = table->selectionModel()->selectedRows();
    return rows.isEmpty() ? -1 : rows.first().row();
}

QString statusText(SubmissionStatus status) {
    switch (status) {
        case SubmissionStatus::Marked:    return "Marked";
        case SubmissionStatus::Moderated: return "Moderated";
        case SubmissionStatus::Released:  return "Released";
        case SubmissionStatus::Submitted: return "Submitted";
        case SubmissionStatus::Late:      return "Submitted late";
    }
    return "Unknown";
}

QString finalMarkText(const HodSubmissionRow& row) {
    if (!row.finalMark) {
        return QString();
    }
    QString text = formatMark(*row.finalMark);
    if (row.rawMark && *row.rawMark > *row.finalMark) {
        text += " (lecturer: " + formatMark(*row.rawMark) + ")";
    }
    return text;
}

}  // namespace

HodWindow::HodWindow(lms::app::AppContext& context, std::shared_ptr<lms::domain::User> user,
                     QWidget* parent)
    : UserWindow(parent), context_(context), user_(std::move(user)) {
    setWindowTitle("Moderated LMS - Head of Department");

    auto* welcome = new QLabel(
        "Logged in as " + QString::fromStdString(user_->fullName()) + " (Head of Department)", this);
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
    assignmentTable_->setHorizontalHeaderLabels(QStringList{"Assignment", "Deadline", "Maximum mark"});
    assignmentTable_->setSelectionBehavior(QAbstractItemView::SelectRows);
    assignmentTable_->setSelectionMode(QAbstractItemView::SingleSelection);
    assignmentTable_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    assignmentTable_->horizontalHeader()->setStretchLastSection(true);
    assignmentTable_->verticalHeader()->setVisible(false);
    assignmentTable_->setMaximumHeight(150);

    workTable_ = new QTableWidget(0, 7, this);
    workTable_->setObjectName("workTable");
    workTable_->setHorizontalHeaderLabels(QStringList{"Student number", "Student", "Status",
                                                      "Late days", "Final mark", "Marked on", "Feedback"});
    workTable_->setSelectionBehavior(QAbstractItemView::SelectRows);
    workTable_->setSelectionMode(QAbstractItemView::SingleSelection);
    workTable_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    workTable_->horizontalHeader()->setStretchLastSection(true);
    workTable_->verticalHeader()->setVisible(false);

    summaryLabel_ = new QLabel(this);
    summaryLabel_->setObjectName("summaryLabel");

    approveButton_ = new QPushButton("&Approve selected", this);
    approveButton_->setObjectName("approveButton");
    approveAllButton_ = new QPushButton("Approve &all marked", this);
    approveAllButton_->setObjectName("approveAllButton");
    changeButton_ = new QPushButton("&Change mark...", this);
    changeButton_->setObjectName("changeButton");
    releaseButton_ = new QPushButton("Re&lease marks...", this);
    releaseButton_->setObjectName("releaseButton");
    historyButton_ = new QPushButton("&History...", this);
    historyButton_->setObjectName("historyButton");

    auto* actions = new QHBoxLayout();
    actions->addWidget(approveButton_);
    actions->addWidget(approveAllButton_);
    actions->addWidget(changeButton_);
    actions->addWidget(historyButton_);
    actions->addStretch();
    actions->addWidget(releaseButton_);

    messageLabel_ = new QLabel(this);
    messageLabel_->setObjectName("messageLabel");
    messageLabel_->setWordWrap(true);

    auto* centre = new QWidget(this);
    auto* layout = new QVBoxLayout(centre);
    layout->addLayout(top);
    layout->addWidget(new QLabel("Courses of my department", this));
    layout->addWidget(courseList_);
    layout->addWidget(new QLabel("Assignments", this));
    layout->addWidget(assignmentTable_);
    layout->addWidget(new QLabel("Marked work", this));
    layout->addWidget(workTable_, 1);
    layout->addWidget(summaryLabel_);
    layout->addLayout(actions);
    layout->addWidget(messageLabel_);
    setCentralWidget(centre);
    resize(1000, 780);

    connect(courseList_, &QListWidget::currentItemChanged, this, &HodWindow::courseChanged);
    connect(assignmentTable_, &QTableWidget::itemSelectionChanged, this, &HodWindow::assignmentChanged);
    connect(workTable_, &QTableWidget::itemSelectionChanged, this, &HodWindow::rowChanged);
    connect(approveButton_, &QPushButton::clicked, this, &HodWindow::approveSelected);
    connect(approveAllButton_, &QPushButton::clicked, this, &HodWindow::approveAllMarked);
    connect(changeButton_, &QPushButton::clicked, this, &HodWindow::chooseChange);
    connect(releaseButton_, &QPushButton::clicked, this, &HodWindow::confirmRelease);
    connect(historyButton_, &QPushButton::clicked, this, &HodWindow::showHistory);
    connect(refreshButton, &QPushButton::clicked, this, &HodWindow::refresh);
    connect(logoutButton, &QPushButton::clicked, this, &UserWindow::loggedOut);

    refresh();
}

// ---- Messages and errors ----
void HodWindow::showMessage(const QString& text, bool isError) {
    messageLabel_->setStyleSheet(isError ? "color: #b00020;" : "color: #1b5e20;");
    messageLabel_->setText(text);
}

void HodWindow::guarded(const std::function<void()>& action) {
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
int HodWindow::currentCourseId() const {
    const QListWidgetItem* item = courseList_->currentItem();
    return item ? item->data(Qt::UserRole).toInt() : 0;
}

const Assignment* HodWindow::currentAssignment() const {
    const int row = selectedRow(assignmentTable_);
    if (row < 0 || row >= static_cast<int>(assignments_.size())) {
        return nullptr;
    }
    return &assignments_[static_cast<std::size_t>(row)];
}

const HodSubmissionRow* HodWindow::currentRow() const {
    const int row = selectedRow(workTable_);
    if (row < 0 || row >= static_cast<int>(rows_.size())) {
        return nullptr;
    }
    return &rows_[static_cast<std::size_t>(row)];
}

int HodWindow::countWithStatus(SubmissionStatus status) const {
    return static_cast<int>(std::count_if(
        rows_.begin(), rows_.end(),
        [status](const HodSubmissionRow& row) { return row.status == status; }));
}

// ---- Loading the lists ----
void HodWindow::loadCourses() {
    const int previous = currentCourseId();
    courseList_->blockSignals(true);
    courseList_->clear();
    for (const lms::domain::Course& course : context_.hodView().courses(*user_)) {
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

void HodWindow::loadAssignments() {
    const Assignment* selected = currentAssignment();
    const int previous = selected ? selected->id() : 0;

    assignmentTable_->blockSignals(true);
    assignmentTable_->setRowCount(0);
    assignments_.clear();
    const int courseId = currentCourseId();
    if (courseId != 0) {
        assignments_ = context_.hodView().assignments(*user_, courseId);
    }
    for (const Assignment& assignment : assignments_) {
        const int row = assignmentTable_->rowCount();
        assignmentTable_->insertRow(row);
        assignmentTable_->setItem(row, 0, new QTableWidgetItem(QString::fromStdString(assignment.title())));
        assignmentTable_->setItem(row, 1, new QTableWidgetItem(formatTime(assignment.deadline())));
        assignmentTable_->setItem(row, 2, new QTableWidgetItem(formatMark(assignment.maxMark())));
        if (assignment.id() == previous) {
            assignmentTable_->selectRow(row);
        }
    }
    assignmentTable_->blockSignals(false);
    loadRows();
}

void HodWindow::loadRows() {
    const HodSubmissionRow* selected = currentRow();
    const int previous = selected ? selected->submissionId : 0;
    const Assignment* assignment = currentAssignment();

    workTable_->blockSignals(true);
    workTable_->setRowCount(0);
    rows_.clear();
    if (assignment != nullptr) {
        rows_ = context_.hodView().rows(*user_, assignment->id());
    }
    for (const HodSubmissionRow& row : rows_) {
        const int index = workTable_->rowCount();
        workTable_->insertRow(index);
        workTable_->setItem(index, 0, new QTableWidgetItem(QString::fromStdString(row.studentNumber)));
        workTable_->setItem(index, 1, new QTableWidgetItem(QString::fromStdString(row.studentName)));
        workTable_->setItem(index, 2, new QTableWidgetItem(statusText(row.status)));
        workTable_->setItem(index, 3, new QTableWidgetItem(QString::number(row.lateDays)));
        workTable_->setItem(index, 4, new QTableWidgetItem(finalMarkText(row)));
        workTable_->setItem(index, 5, new QTableWidgetItem(row.rawMark ? formatMark(*row.rawMark) : ""));
        auto* feedback = new QTableWidgetItem(QString::fromStdString(row.feedback).simplified());
        feedback->setToolTip(QString::fromStdString(row.feedback));
        workTable_->setItem(index, 6, feedback);
        if (row.submissionId == previous) {
            workTable_->selectRow(index);
        }
    }
    workTable_->blockSignals(false);

    if (assignment == nullptr) {
        summaryLabel_->setText("Select an assignment to see its marked work.");
    } else {
        summaryLabel_->setText(QString("%1 marked, %2 moderated, %3 released")
                                   .arg(countWithStatus(SubmissionStatus::Marked))
                                   .arg(countWithStatus(SubmissionStatus::Moderated))
                                   .arg(countWithStatus(SubmissionStatus::Released)));
    }
    updateButtons();
}

void HodWindow::updateButtons() {
    const HodSubmissionRow* row = currentRow();
    const bool rowIsMarked = row != nullptr && row->status == SubmissionStatus::Marked;
    approveButton_->setEnabled(rowIsMarked);
    changeButton_->setEnabled(rowIsMarked);
    historyButton_->setEnabled(row != nullptr);
    approveAllButton_->setEnabled(countWithStatus(SubmissionStatus::Marked) > 0);
    releaseButton_->setEnabled(countWithStatus(SubmissionStatus::Moderated) > 0);
}

// ---- Slots ----
void HodWindow::refresh() {
    showMessage("", false);
    guarded([this]() {
        loadCourses();
        loadAssignments();
    });
}

void HodWindow::courseChanged() {
    guarded([this]() { loadAssignments(); });
}

void HodWindow::assignmentChanged() {
    guarded([this]() { loadRows(); });
}

void HodWindow::rowChanged() { updateButtons(); }

void HodWindow::approveSelected() {
    const HodSubmissionRow* row = currentRow();
    if (row == nullptr) {
        showMessage("Select a student first.", true);
        return;
    }
    approveSubmission(row->submissionId);
}

void HodWindow::chooseChange() {
    const HodSubmissionRow* row = currentRow();
    const Assignment* assignment = currentAssignment();
    if (row == nullptr || assignment == nullptr || !row->finalMark) {
        showMessage("Select a marked student first.", true);
        return;
    }
    const HodSubmissionRow copy = *row;  // the lists may be reloaded while the form is open
    ChangeMarkDialog dialog(QString::fromStdString(copy.studentName), assignment->maxMark(),
                            *copy.finalMark, this);
    if (dialog.exec() == QDialog::Accepted) {
        changeMark(copy.submissionId, dialog.newMark(), dialog.reason());
    }
}

void HodWindow::confirmRelease() {
    const Assignment* assignment = currentAssignment();
    if (assignment == nullptr) {
        showMessage("Select an assignment first.", true);
        return;
    }
    const int moderated = countWithStatus(SubmissionStatus::Moderated);
    if (moderated == 0) {
        showMessage("There are no moderated marks to release.", true);
        return;
    }
    const QString question =
        QString("Release %1 moderated mark(s) of \"%2\"?\nThe students will see their marks and feedback.")
            .arg(moderated)
            .arg(QString::fromStdString(assignment->title()));
    if (QMessageBox::question(this, "Release marks", question) == QMessageBox::Yes) {
        releaseMarks();
    }
}

void HodWindow::showHistory() {
    const HodSubmissionRow* row = currentRow();
    if (row == nullptr) {
        showMessage("Select a student first.", true);
        return;
    }
    const int id = row->submissionId;
    const QString name = QString::fromStdString(row->studentName);
    guarded([&]() {
        const auto history = context_.hodView().history(*user_, id);
        AuditHistoryDialog dialog(name, history, this);
        dialog.exec();
    });
}

// ---- Actions ----
void HodWindow::approveSubmission(int submissionId) {
    guarded([&]() {
        context_.moderation().approve(*user_, submissionId);
        loadRows();
        showMessage("Approved. The mark is now moderated.", false);
    });
}

void HodWindow::approveAllMarked() {
    std::vector<int> ids;
    for (const HodSubmissionRow& row : rows_) {
        if (row.status == SubmissionStatus::Marked) {
            ids.push_back(row.submissionId);
        }
    }
    if (ids.empty()) {
        showMessage("There is no marked work to approve.", true);
        return;
    }
    guarded([&]() {
        try {
            for (int id : ids) {
                context_.moderation().approve(*user_, id);
            }
        } catch (...) {
            loadRows();  // show what was approved before the problem
            throw;
        }
        loadRows();
        showMessage(QString("Approved %1 mark(s).").arg(ids.size()), false);
    });
}

void HodWindow::changeMark(int submissionId, double newMark, const QString& reason) {
    guarded([&]() {
        context_.moderation().changeMark(*user_, submissionId, newMark, reason.toStdString());
        loadRows();
        showMessage("Mark changed to " + formatMark(newMark) + ". The change is saved in the audit log.",
                    false);
    });
}

void HodWindow::releaseMarks() {
    const Assignment* assignment = currentAssignment();
    if (assignment == nullptr) {
        showMessage("Select an assignment first.", true);
        return;
    }
    const int assignmentId = assignment->id();
    guarded([&]() {
        const lms::application::ReleaseResult result =
            context_.moderation().release(*user_, assignmentId);
        loadRows();
        QString text = QString("Released %1 mark(s). The students were notified.").arg(result.releasedCount);
        if (result.notificationFailures > 0) {
            text += QString(" %1 notification step(s) failed, but the marks are released.")
                        .arg(result.notificationFailures);
        }
        showMessage(text, false);
    });
}

}  // namespace lms::ui
