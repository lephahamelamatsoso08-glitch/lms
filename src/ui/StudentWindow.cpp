#include "StudentWindow.h"

#include <exception>
#include <stdexcept>
#include <utility>

#include <QAbstractItemView>
#include <QFileDialog>
#include <QFont>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QStringList>
#include <QTableWidget>
#include <QTabWidget>
#include <QVBoxLayout>

#include "AccessDeniedError.h"
#include "Course.h"
#include "Notification.h"
#include "SubmissionService.h"
#include "UiFormat.h"

namespace lms::ui {

using lms::application::AccessDeniedError;
using lms::application::StudentAssignmentItem;
using lms::application::StudentSubmissionView;
using lms::application::SubmissionNotAllowedError;

StudentWindow::StudentWindow(lms::app::AppContext& context,
                             std::shared_ptr<lms::domain::User> user, QWidget* parent)
    : UserWindow(parent), context_(context), user_(std::move(user)) {
    setWindowTitle("Moderated LMS - Student");

    auto* welcome =
        new QLabel("Logged in as " + QString::fromStdString(user_->fullName()) + " (Student)", this);
    welcome->setObjectName("welcomeLabel");
    auto* refreshButton = new QPushButton("&Refresh", this);
    refreshButton->setObjectName("refreshButton");
    auto* logoutButton = new QPushButton("Log &out", this);
    logoutButton->setObjectName("logoutButton");

    auto* top = new QHBoxLayout();
    top->addWidget(welcome, 1);
    top->addWidget(refreshButton);
    top->addWidget(logoutButton);

    // ---- Tab 1: courses and assignments ----
    courseList_ = new QListWidget(this);
    courseList_->setObjectName("courseList");
    courseList_->setMaximumHeight(120);

    assignmentTable_ = new QTableWidget(0, 4, this);
    assignmentTable_->setObjectName("assignmentTable");
    assignmentTable_->setHorizontalHeaderLabels(
        QStringList{"Assignment", "Your deadline", "Maximum mark", "Status"});
    assignmentTable_->setSelectionBehavior(QAbstractItemView::SelectRows);
    assignmentTable_->setSelectionMode(QAbstractItemView::SingleSelection);
    assignmentTable_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    assignmentTable_->horizontalHeader()->setStretchLastSection(true);
    assignmentTable_->verticalHeader()->setVisible(false);

    detailLabel_ = new QLabel(this);
    detailLabel_->setObjectName("detailLabel");
    detailLabel_->setWordWrap(true);
    detailLabel_->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    detailLabel_->setTextInteractionFlags(Qt::TextSelectableByMouse);
    detailLabel_->setMinimumHeight(110);

    submitButton_ = new QPushButton("&Submit or resubmit a file...", this);
    submitButton_->setObjectName("submitButton");
    submitButton_->setEnabled(false);

    messageLabel_ = new QLabel(this);
    messageLabel_->setObjectName("messageLabel");
    messageLabel_->setWordWrap(true);

    auto* coursesPage = new QWidget(this);
    auto* coursesLayout = new QVBoxLayout(coursesPage);
    coursesLayout->addWidget(new QLabel("My courses", coursesPage));
    coursesLayout->addWidget(courseList_);
    coursesLayout->addWidget(new QLabel("Assignments", coursesPage));
    coursesLayout->addWidget(assignmentTable_, 1);
    coursesLayout->addWidget(detailLabel_);
    coursesLayout->addWidget(submitButton_);
    coursesLayout->addWidget(messageLabel_);

    // ---- Tab 2: notifications ----
    notificationList_ = new QListWidget(this);
    notificationList_->setObjectName("notificationList");
    markReadButton_ = new QPushButton("&Mark as read", this);
    markReadButton_->setObjectName("markReadButton");

    auto* notificationsPage = new QWidget(this);
    auto* notificationsLayout = new QVBoxLayout(notificationsPage);
    notificationsLayout->addWidget(notificationList_, 1);
    notificationsLayout->addWidget(markReadButton_);

    tabs_ = new QTabWidget(this);
    tabs_->setObjectName("tabs");
    tabs_->addTab(coursesPage, "My courses");
    tabs_->addTab(notificationsPage, "Notifications");

    auto* centre = new QWidget(this);
    auto* layout = new QVBoxLayout(centre);
    layout->addLayout(top);
    layout->addWidget(tabs_, 1);
    setCentralWidget(centre);
    resize(780, 620);

    connect(courseList_, &QListWidget::currentItemChanged, this, &StudentWindow::courseChanged);
    connect(assignmentTable_, &QTableWidget::itemSelectionChanged, this,
            &StudentWindow::assignmentChanged);
    connect(submitButton_, &QPushButton::clicked, this, &StudentWindow::chooseAndSubmit);
    connect(markReadButton_, &QPushButton::clicked, this, &StudentWindow::markSelectedRead);
    connect(refreshButton, &QPushButton::clicked, this, &StudentWindow::refresh);
    connect(logoutButton, &QPushButton::clicked, this, &UserWindow::loggedOut);

    refresh();
}

// ---- Messages and errors ----
void StudentWindow::showMessage(const QString& text, bool isError) {
    messageLabel_->setStyleSheet(isError ? "color: #b00020;" : "color: #1b5e20;");
    messageLabel_->setText(text);
}

void StudentWindow::guarded(const std::function<void()>& action) {
    try {
        action();
    } catch (const AccessDeniedError& error) {
        showMessage(QString::fromUtf8(error.what()), true);
    } catch (const SubmissionNotAllowedError& error) {
        showMessage(QString::fromUtf8(error.what()), true);
    } catch (const std::invalid_argument& error) {
        showMessage(QString::fromUtf8(error.what()), true);
    } catch (const std::exception&) {
        showMessage("Something went wrong. Please try again.", true);
    }
}

// ---- What the student sees ----
QString StudentWindow::describe(const StudentSubmissionView& view) {
    QStringList lines;
    lines << "Assignment: " + QString::fromStdString(view.assignmentTitle);
    lines << "Your deadline: " + formatTime(view.deadline);
    lines << "Maximum mark: " + formatMark(view.maxMark);
    lines << "Status: " + QString::fromStdString(view.statusLabel);
    if (view.submitted) {
        lines << "Files uploaded: " + QString::number(view.versionCount);
        if (view.lateDays > 0) {
            lines << QString("Submitted %1 day(s) late. A late penalty applies.").arg(view.lateDays);
        }
    }
    if (view.finalMark) {
        lines << "Final mark: " + formatMark(*view.finalMark) + " out of " +
                     formatMark(view.maxMark);
        if (view.rawMark && *view.rawMark > *view.finalMark) {
            lines << "(Lecturer's mark before the penalty: " + formatMark(*view.rawMark) +
                         ", late penalty: " + formatMark(*view.rawMark - *view.finalMark) + ")";
        }
    }
    if (view.feedback) {
        lines << "Feedback: " + QString::fromStdString(*view.feedback);
    }
    if (view.releasedAt) {
        lines << "Released: " + formatTime(*view.releasedAt);
    }
    return lines.join("\n");
}

int StudentWindow::currentCourseId() const {
    const QListWidgetItem* item = courseList_->currentItem();
    return item ? item->data(Qt::UserRole).toInt() : 0;
}

int StudentWindow::currentAssignmentId() const {
    const QModelIndexList rows = assignmentTable_->selectionModel()->selectedRows();
    if (rows.isEmpty()) {
        return 0;
    }
    return assignmentTable_->item(rows.first().row(), 0)->data(Qt::UserRole).toInt();
}

void StudentWindow::loadCourses() {
    const int previous = currentCourseId();
    courseList_->blockSignals(true);
    courseList_->clear();
    for (const lms::domain::Course& course : context_.studentView().myCourses(*user_)) {
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

void StudentWindow::loadAssignments() {
    const int courseId = currentCourseId();
    const int previous = currentAssignmentId();
    assignmentTable_->blockSignals(true);
    assignmentTable_->setRowCount(0);
    if (courseId != 0) {
        for (const StudentAssignmentItem& item :
             context_.studentView().myAssignments(*user_, courseId)) {
            const int row = assignmentTable_->rowCount();
            assignmentTable_->insertRow(row);
            auto* title = new QTableWidgetItem(QString::fromStdString(item.title));
            title->setData(Qt::UserRole, item.assignmentId);
            assignmentTable_->setItem(row, 0, title);
            assignmentTable_->setItem(row, 1, new QTableWidgetItem(formatTime(item.deadline)));
            assignmentTable_->setItem(row, 2, new QTableWidgetItem(formatMark(item.maxMark)));
            assignmentTable_->setItem(row, 3,
                                      new QTableWidgetItem(QString::fromStdString(item.statusLabel)));
            if (item.assignmentId == previous) {
                assignmentTable_->selectRow(row);
            }
        }
    }
    assignmentTable_->blockSignals(false);
    showDetail();
}

void StudentWindow::showDetail() {
    const int assignmentId = currentAssignmentId();
    if (assignmentId == 0) {
        detailLabel_->setText("Select an assignment to see the details.");
        submitButton_->setEnabled(false);
        return;
    }
    detailLabel_->setText(describe(context_.studentView().mySubmission(*user_, assignmentId)));
    submitButton_->setEnabled(true);
}

void StudentWindow::loadNotifications() {
    notificationList_->clear();
    int unread = 0;
    for (const lms::domain::Notification& note : context_.notifications().list(*user_)) {
        auto* item = new QListWidgetItem(QString::fromStdString(note.message()) + "  (" +
                                         formatTime(note.createdAt()) + ")");
        item->setData(Qt::UserRole, note.id());
        if (!note.isRead()) {
            QFont font = item->font();
            font.setBold(true);
            item->setFont(font);
            ++unread;
        }
        notificationList_->addItem(item);
    }
    tabs_->setTabText(1, unread > 0 ? QString("Notifications (%1)").arg(unread)
                                    : QString("Notifications"));
}

// ---- Slots ----
void StudentWindow::refresh() {
    showMessage("", false);
    guarded([this]() {
        loadCourses();
        loadAssignments();
        loadNotifications();
    });
}

void StudentWindow::courseChanged() {
    guarded([this]() { loadAssignments(); });
}

void StudentWindow::assignmentChanged() {
    guarded([this]() { showDetail(); });
}

void StudentWindow::chooseAndSubmit() {
    const QString path = QFileDialog::getOpenFileName(
        this, "Choose the file to submit", QString(), "Documents (*.pdf *.doc *.docx *.txt *.zip)");
    if (!path.isEmpty()) {
        submitFile(path);
    }
}

void StudentWindow::submitFile(const QString& path) {
    const int assignmentId = currentAssignmentId();
    if (assignmentId == 0) {
        showMessage("Select an assignment first.", true);
        return;
    }
    guarded([&]() {
        const lms::application::SubmitResult result =
            context_.submissions().submit(*user_, assignmentId, path.toStdString());
        QString text = result.isResubmission ? "Resubmitted." : "Submitted.";
        text += QString(" File version %1.").arg(result.versionNo);
        if (result.submission.lateDays() > 0) {
            text += QString(" This work is %1 day(s) late. The highest mark you can get is %2.")
                        .arg(result.submission.lateDays())
                        .arg(formatMark(result.highestPossibleMark));
        }
        loadAssignments();
        showMessage(text, false);
    });
}

void StudentWindow::markSelectedRead() {
    const QListWidgetItem* item = notificationList_->currentItem();
    if (item == nullptr) {
        showMessage("Select a notification first.", true);
        return;
    }
    const int id = item->data(Qt::UserRole).toInt();
    guarded([&]() {
        context_.notifications().markRead(*user_, id);
        loadNotifications();
        showMessage("", false);
    });
}

}  // namespace lms::ui
