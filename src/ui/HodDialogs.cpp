#include "HodDialogs.h"

#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QStringList>
#include <QTableWidget>
#include <QVBoxLayout>

#include "UiFormat.h"

namespace lms::ui {

ChangeMarkDialog::ChangeMarkDialog(const QString& studentName, double maxMark, double currentMark,
                                   QWidget* parent)
    : QDialog(parent) {
    setWindowTitle("Change a mark");

    markEdit_ = new QDoubleSpinBox(this);
    markEdit_->setObjectName("markEdit");
    markEdit_->setRange(0.0, maxMark);
    markEdit_->setDecimals(2);
    markEdit_->setSingleStep(0.5);
    markEdit_->setValue(currentMark);

    reasonEdit_ = new QLineEdit(this);
    reasonEdit_->setObjectName("reasonEdit");

    errorLabel_ = new QLabel(this);
    errorLabel_->setObjectName("errorLabel");
    errorLabel_->setWordWrap(true);
    errorLabel_->setStyleSheet("color: #b00020;");

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    buttons->setObjectName("buttons");
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto* form = new QFormLayout();
    form->addRow(new QLabel("Student: " + studentName, this));
    form->addRow(new QLabel("Current final mark: " + formatMark(currentMark), this));
    form->addRow("&New final mark (0 to " + formatMark(maxMark) + "):", markEdit_);
    form->addRow("&Reason:", reasonEdit_);

    auto* layout = new QVBoxLayout(this);
    layout->addLayout(form);
    layout->addWidget(new QLabel("The change is saved in the audit log with your name.", this));
    layout->addWidget(errorLabel_);
    layout->addWidget(buttons);
}

double ChangeMarkDialog::newMark() const { return markEdit_->value(); }
QString ChangeMarkDialog::reason() const { return reasonEdit_->text().trimmed(); }

void ChangeMarkDialog::accept() {
    if (reason().isEmpty()) {
        errorLabel_->setText("Give a reason for the change.");
        return;
    }
    QDialog::accept();
}

AuditHistoryDialog::AuditHistoryDialog(const QString& studentName,
                                       const std::vector<lms::application::HodAuditRow>& rows,
                                       QWidget* parent)
    : QDialog(parent) {
    setWindowTitle("Mark history");

    auto* table = new QTableWidget(static_cast<int>(rows.size()), 5, this);
    table->setObjectName("historyTable");
    table->setHorizontalHeaderLabels(
        QStringList{"When", "Changed by", "Old mark", "New mark", "Reason"});
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->setSelectionMode(QAbstractItemView::NoSelection);
    table->horizontalHeader()->setStretchLastSection(true);
    table->verticalHeader()->setVisible(false);
    for (int i = 0; i < static_cast<int>(rows.size()); ++i) {
        const lms::application::HodAuditRow& row = rows[static_cast<std::size_t>(i)];
        table->setItem(i, 0, new QTableWidgetItem(formatTime(row.changedAt)));
        table->setItem(i, 1, new QTableWidgetItem(QString::fromStdString(row.changedBy)));
        table->setItem(i, 2, new QTableWidgetItem(row.oldValue ? formatMark(*row.oldValue)
                                                               : QString("(first mark)")));
        table->setItem(i, 3, new QTableWidgetItem(formatMark(row.newValue)));
        table->setItem(i, 4, new QTableWidgetItem(QString::fromStdString(row.reason)));
    }

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
    buttons->setObjectName("buttons");
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto* layout = new QVBoxLayout(this);
    layout->addWidget(new QLabel("Changes of the mark of " + studentName, this));
    layout->addWidget(table);
    layout->addWidget(buttons);
    resize(720, 320);
}

}  // namespace lms::ui
