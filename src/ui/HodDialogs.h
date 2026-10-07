#pragma once

#include <vector>

#include <QDialog>
#include <QString>

#include "HodViewService.h"

class QDoubleSpinBox;
class QLabel;
class QLineEdit;

namespace lms::ui {

// The form for changing a mark during moderation (FR13). The reason is required (C7).
class ChangeMarkDialog : public QDialog {
    Q_OBJECT

public:
    ChangeMarkDialog(const QString& studentName, double maxMark, double currentMark,
                     QWidget* parent = nullptr);
    double newMark() const;
    QString reason() const;
    void accept() override;

private:
    QDoubleSpinBox* markEdit_;
    QLineEdit* reasonEdit_;
    QLabel* errorLabel_;
};

// A read-only list of the changes of one mark (FR15, FR19).
class AuditHistoryDialog : public QDialog {
    Q_OBJECT

public:
    AuditHistoryDialog(const QString& studentName,
                       const std::vector<lms::application::HodAuditRow>& rows,
                       QWidget* parent = nullptr);
};

}  // namespace lms::ui
