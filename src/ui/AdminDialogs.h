#pragma once

#include <utility>
#include <vector>

#include <QDialog>
#include <QString>

#include "DepartmentInfo.h"
#include "Role.h"
#include "UserFactory.h"

class QComboBox;
class QLabel;
class QLineEdit;

namespace lms::ui {

// The small forms of the administrator's window. They only check that the fields are filled
// in. The business rules (password rule, duplicates, who may be enrolled) are in the services.

class NewUserDialog : public QDialog {
    Q_OBJECT

public:
    explicit NewUserDialog(const std::vector<lms::domain::DepartmentInfo>& departments,
                           QWidget* parent = nullptr);
    lms::domain::Role role() const;
    lms::domain::UserData userData() const;
    QString password() const;
    void accept() override;

private:
    void updateFields();

    QComboBox* roleCombo_;
    QLineEdit* usernameEdit_;
    QLineEdit* fullNameEdit_;
    QLineEdit* studentNumberEdit_;
    QComboBox* departmentCombo_;
    QLineEdit* passwordEdit_;
    QLineEdit* confirmEdit_;
    QLabel* errorLabel_;
};

class PasswordDialog : public QDialog {
    Q_OBJECT

public:
    explicit PasswordDialog(const QString& userName, QWidget* parent = nullptr);
    QString password() const;
    void accept() override;

private:
    QLineEdit* passwordEdit_;
    QLineEdit* confirmEdit_;
    QLabel* errorLabel_;
};

class TextDialog : public QDialog {
    Q_OBJECT

public:
    TextDialog(const QString& title, const QString& label, QWidget* parent = nullptr);
    QString text() const;
    void accept() override;

private:
    QLineEdit* textEdit_;
    QLabel* errorLabel_;
};

class CourseDialog : public QDialog {
    Q_OBJECT

public:
    explicit CourseDialog(const std::vector<lms::domain::DepartmentInfo>& departments,
                          QWidget* parent = nullptr);
    QString code() const;
    QString title() const;
    int departmentId() const;
    void accept() override;

private:
    QLineEdit* codeEdit_;
    QLineEdit* titleEdit_;
    QComboBox* departmentCombo_;
    QLabel* errorLabel_;
};

// Choose one person (or item) from a list.
class ChoiceDialog : public QDialog {
    Q_OBJECT

public:
    ChoiceDialog(const QString& title, const QString& label,
                 const std::vector<std::pair<int, QString>>& items, QWidget* parent = nullptr);
    int selectedId() const;
    void accept() override;

private:
    QComboBox* combo_;
    QLabel* errorLabel_;
};

}  // namespace lms::ui
