#pragma once

#include <optional>

#include <QDialog>
#include <QString>

#include "LateDays.h"

class QDateTimeEdit;
class QDialogButtonBox;
class QDoubleSpinBox;
class QLabel;
class QLineEdit;
class QPlainTextEdit;

namespace lms::ui {

// The three small forms of the lecturer's window. They only collect and check that the
// fields are filled in. The business rules are checked by the services.

class AssignmentDialog : public QDialog {
    Q_OBJECT

public:
    explicit AssignmentDialog(QWidget* parent = nullptr);
    QString title() const;
    lms::domain::TimePoint deadline() const;
    double maxMark() const;
    void accept() override;

private:
    QLineEdit* titleEdit_;
    QDateTimeEdit* deadlineEdit_;
    QDoubleSpinBox* maxMarkEdit_;
    QLabel* errorLabel_;
};

class MarkDialog : public QDialog {
    Q_OBJECT

public:
    MarkDialog(const QString& studentName, double maxMark, std::optional<double> currentRaw,
               const QString& currentFeedback, bool isReMark, QWidget* parent = nullptr);
    double rawMark() const;
    QString feedback() const;
    QString reason() const;
    void accept() override;

private:
    bool isReMark_;
    QDoubleSpinBox* markEdit_;
    QPlainTextEdit* feedbackEdit_;
    QLineEdit* reasonEdit_;
    QLabel* errorLabel_;
};

class ExtensionDialog : public QDialog {
    Q_OBJECT

public:
    ExtensionDialog(const QString& studentName, lms::domain::TimePoint normalDeadline,
                    QWidget* parent = nullptr);
    lms::domain::TimePoint newDeadline() const;
    QString reason() const;
    void accept() override;

private:
    QDateTimeEdit* deadlineEdit_;
    QLineEdit* reasonEdit_;
    QLabel* errorLabel_;
};

}  // namespace lms::ui
