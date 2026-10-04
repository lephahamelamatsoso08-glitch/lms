#pragma once

#include <QDialog>

#include "AppContext.h"

class QLabel;
class QLineEdit;
class QPushButton;

namespace lms::ui {

// Shown on the first start, when the system has no users (nobody could log in).
class FirstRunDialog : public QDialog {
    Q_OBJECT

public:
    explicit FirstRunDialog(lms::app::AppContext& context, QWidget* parent = nullptr);

private slots:
    void createAdministrator();

private:
    lms::app::AppContext& context_;
    QLineEdit* usernameEdit_;
    QLineEdit* fullNameEdit_;
    QLineEdit* passwordEdit_;
    QLineEdit* confirmEdit_;
    QPushButton* createButton_;
    QLabel* errorLabel_;
};

}  // namespace lms::ui
