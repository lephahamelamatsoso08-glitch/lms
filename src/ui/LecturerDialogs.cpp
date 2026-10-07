#include "LecturerDialogs.h"

#include <QDateTime>
#include <QDateTimeEdit>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QVBoxLayout>

#include "UiFormat.h"

namespace lms::ui {

namespace {

QLabel* makeErrorLabel(QWidget* parent) {
    auto* label = new QLabel(parent);
    label->setObjectName("errorLabel");
    label->setWordWrap(true);
    label->setStyleSheet("color: #b00020;");
    return label;
}

QDialogButtonBox* makeButtons(QDialog* dialog) {
    auto* buttons =
        new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, dialog);
    buttons->setObjectName("buttons");
    QObject::connect(buttons, &QDialogButtonBox::accepted, dialog, &QDialog::accept);
    QObject::connect(buttons, &QDialogButtonBox::rejected, dialog, &QDialog::reject);
    return buttons;
}

}  // namespace

// ---- New assignment (FR6) ----
AssignmentDialog::AssignmentDialog(QWidget* parent) : QDialog(parent) {
    setWindowTitle("New assignment");

    titleEdit_ = new QLineEdit(this);
    titleEdit_->setObjectName("titleEdit");

    QDateTime suggested = QDateTime::currentDateTime().addDays(7);
    suggested.setTime(QTime(suggested.time().hour(), suggested.time().minute()));
    deadlineEdit_ = new QDateTimeEdit(suggested, this);
    deadlineEdit_->setObjectName("deadlineEdit");
    deadlineEdit_->setCalendarPopup(true);
    deadlineEdit_->setDisplayFormat("ddd d MMM yyyy, HH:mm");

    maxMarkEdit_ = new QDoubleSpinBox(this);
    maxMarkEdit_->setObjectName("maxMarkEdit");
    maxMarkEdit_->setRange(1.0, 1000.0);
    maxMarkEdit_->setDecimals(1);
    maxMarkEdit_->setValue(100.0);

    errorLabel_ = makeErrorLabel(this);

    auto* form = new QFormLayout();
    form->addRow("&Title:", titleEdit_);
    form->addRow("&Deadline:", deadlineEdit_);
    form->addRow("&Maximum mark:", maxMarkEdit_);

    auto* layout = new QVBoxLayout(this);
    layout->addLayout(form);
    layout->addWidget(errorLabel_);
    layout->addWidget(makeButtons(this));
}

QString AssignmentDialog::title() const { return titleEdit_->text().trimmed(); }
lms::domain::TimePoint AssignmentDialog::deadline() const {
    return toTimePoint(deadlineEdit_->dateTime());
}
double AssignmentDialog::maxMark() const { return maxMarkEdit_->value(); }

void AssignmentDialog::accept() {
    if (title().isEmpty()) {
        errorLabel_->setText("Enter a title.");
        return;
    }
    QDialog::accept();
}

// ---- Mark or re-mark (FR12) ----
MarkDialog::MarkDialog(const QString& studentName, double maxMark,
                       std::optional<double> currentRaw, const QString& currentFeedback,
                       bool isReMark, QWidget* parent)
    : QDialog(parent), isReMark_(isReMark) {
    setWindowTitle(isReMark ? "Re-mark" : "Mark");

    markEdit_ = new QDoubleSpinBox(this);
    markEdit_->setObjectName("markEdit");
    markEdit_->setRange(0.0, maxMark);
    markEdit_->setDecimals(2);
    markEdit_->setSingleStep(0.5);
    markEdit_->setValue(currentRaw.value_or(0.0));

    feedbackEdit_ = new QPlainTextEdit(this);
    feedbackEdit_->setObjectName("feedbackEdit");
    feedbackEdit_->setPlainText(currentFeedback);

    reasonEdit_ = new QLineEdit(this);
    reasonEdit_->setObjectName("reasonEdit");
    reasonEdit_->setEnabled(isReMark);

    errorLabel_ = makeErrorLabel(this);

    auto* note = new QLabel(
        "The mark is your mark before any late penalty. The system applies the penalty.", this);
    note->setWordWrap(true);

    auto* form = new QFormLayout();
    form->addRow(new QLabel("Student: " + studentName, this));
    form->addRow("&Mark (0 to " + formatMark(maxMark) + "):", markEdit_);
    form->addRow("&Feedback:", feedbackEdit_);
    form->addRow("&Reason for the change:", reasonEdit_);

    auto* layout = new QVBoxLayout(this);
    layout->addLayout(form);
    layout->addWidget(note);
    layout->addWidget(errorLabel_);
    layout->addWidget(makeButtons(this));
    resize(460, 380);
}

double MarkDialog::rawMark() const { return markEdit_->value(); }
QString MarkDialog::feedback() const { return feedbackEdit_->toPlainText().trimmed(); }
QString MarkDialog::reason() const { return reasonEdit_->text().trimmed(); }

void MarkDialog::accept() {
    if (feedback().isEmpty()) {
        errorLabel_->setText("Write feedback for the student.");
        return;
    }
    if (isReMark_ && reason().isEmpty()) {
        errorLabel_->setText("Give a reason for the change.");
        return;
    }
    QDialog::accept();
}

// ---- Extension (FR7) ----
ExtensionDialog::ExtensionDialog(const QString& studentName,
                                 lms::domain::TimePoint normalDeadline, QWidget* parent)
    : QDialog(parent) {
    setWindowTitle("Grant an extension");

    const QDateTime normal = toQDateTime(normalDeadline);
    deadlineEdit_ = new QDateTimeEdit(normal.addDays(3), this);
    deadlineEdit_->setObjectName("deadlineEdit");
    deadlineEdit_->setCalendarPopup(true);
    deadlineEdit_->setDisplayFormat("ddd d MMM yyyy, HH:mm");
    deadlineEdit_->setMinimumDateTime(normal.addSecs(60));  // it must be later

    reasonEdit_ = new QLineEdit(this);
    reasonEdit_->setObjectName("reasonEdit");

    errorLabel_ = makeErrorLabel(this);

    auto* form = new QFormLayout();
    form->addRow(new QLabel("Student: " + studentName, this));
    form->addRow(new QLabel("Normal deadline: " + formatTime(normalDeadline), this));
    form->addRow("&New deadline:", deadlineEdit_);
    form->addRow("&Reason:", reasonEdit_);

    auto* layout = new QVBoxLayout(this);
    layout->addLayout(form);
    layout->addWidget(errorLabel_);
    layout->addWidget(makeButtons(this));
}

lms::domain::TimePoint ExtensionDialog::newDeadline() const {
    return toTimePoint(deadlineEdit_->dateTime());
}
QString ExtensionDialog::reason() const { return reasonEdit_->text().trimmed(); }

void ExtensionDialog::accept() {
    if (reason().isEmpty()) {
        errorLabel_->setText("Give a reason for the extension.");
        return;
    }
    QDialog::accept();
}

}  // namespace lms::ui
