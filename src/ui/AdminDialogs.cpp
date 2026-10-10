#include "AdminDialogs.h"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QVBoxLayout>

namespace lms::ui {

using lms::domain::Role;

namespace {

QLabel* makeErrorLabel(QWidget* parent) {
    auto* label = new QLabel(parent);
    label->setObjectName("errorLabel");
    label->setWordWrap(true);
    label->setStyleSheet("color: #b00020;");
    return label;
}

QDialogButtonBox* makeButtons(QDialog* dialog) {
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, dialog);
    buttons->setObjectName("buttons");
    QObject::connect(buttons, &QDialogButtonBox::accepted, dialog, &QDialog::accept);
    QObject::connect(buttons, &QDialogButtonBox::rejected, dialog, &QDialog::reject);
    return buttons;
}

QLineEdit* makeEdit(QWidget* parent, const char* name, bool isPassword = false) {
    auto* edit = new QLineEdit(parent);
    edit->setObjectName(name);
    if (isPassword) {
        edit->setEchoMode(QLineEdit::Password);
    }
    return edit;
}

QComboBox* makeDepartmentCombo(QWidget* parent,
                               const std::vector<lms::domain::DepartmentInfo>& departments) {
    auto* combo = new QComboBox(parent);
    combo->setObjectName("departmentCombo");
    for (const lms::domain::DepartmentInfo& department : departments) {
        combo->addItem(QString::fromStdString(department.name), department.id);
    }
    return combo;
}

}  // namespace

// ---- New user (FR3) ----
NewUserDialog::NewUserDialog(const std::vector<lms::domain::DepartmentInfo>& departments,
                             QWidget* parent)
    : QDialog(parent) {
    setWindowTitle("New user");

    roleCombo_ = new QComboBox(this);
    roleCombo_->setObjectName("roleCombo");
    roleCombo_->addItem("Student", static_cast<int>(Role::Student));
    roleCombo_->addItem("Lecturer", static_cast<int>(Role::Lecturer));
    roleCombo_->addItem("Head of Department", static_cast<int>(Role::HoD));
    roleCombo_->addItem("Administrator", static_cast<int>(Role::Admin));

    usernameEdit_ = makeEdit(this, "usernameEdit");
    fullNameEdit_ = makeEdit(this, "fullNameEdit");
    studentNumberEdit_ = makeEdit(this, "studentNumberEdit");
    departmentCombo_ = makeDepartmentCombo(this, departments);
    passwordEdit_ = makeEdit(this, "passwordEdit", true);
    confirmEdit_ = makeEdit(this, "confirmEdit", true);
    errorLabel_ = makeErrorLabel(this);

    auto* rule = new QLabel("The password needs at least 8 characters, with a letter and a digit.", this);
    rule->setWordWrap(true);

    auto* form = new QFormLayout();
    form->addRow("&Role:", roleCombo_);
    form->addRow("&Username:", usernameEdit_);
    form->addRow("&Full name:", fullNameEdit_);
    form->addRow("Student &number:", studentNumberEdit_);
    form->addRow("&Department:", departmentCombo_);
    form->addRow("&Password:", passwordEdit_);
    form->addRow("&Repeat password:", confirmEdit_);

    auto* layout = new QVBoxLayout(this);
    layout->addLayout(form);
    layout->addWidget(rule);
    layout->addWidget(errorLabel_);
    layout->addWidget(makeButtons(this));

    connect(roleCombo_, &QComboBox::currentIndexChanged, this, [this](int) { updateFields(); });
    updateFields();
}

void NewUserDialog::updateFields() {
    studentNumberEdit_->setEnabled(role() == Role::Student);
    departmentCombo_->setEnabled(role() == Role::HoD && departmentCombo_->count() > 0);
}

Role NewUserDialog::role() const { return static_cast<Role>(roleCombo_->currentData().toInt()); }

lms::domain::UserData NewUserDialog::userData() const {
    lms::domain::UserData data;
    data.username = usernameEdit_->text().trimmed().toStdString();
    data.fullName = fullNameEdit_->text().trimmed().toStdString();
    if (role() == Role::Student) {
        data.studentNumber = studentNumberEdit_->text().trimmed().toStdString();
    }
    if (role() == Role::HoD && departmentCombo_->count() > 0) {
        data.departmentId = departmentCombo_->currentData().toInt();
    }
    return data;
}

QString NewUserDialog::password() const { return passwordEdit_->text(); }

void NewUserDialog::accept() {
    if (usernameEdit_->text().trimmed().isEmpty() || fullNameEdit_->text().trimmed().isEmpty()) {
        errorLabel_->setText("Enter a username and a full name.");
        return;
    }
    if (role() == Role::Student && studentNumberEdit_->text().trimmed().isEmpty()) {
        errorLabel_->setText("Enter the student number.");
        return;
    }
    if (role() == Role::HoD && departmentCombo_->count() == 0) {
        errorLabel_->setText("Create a department first.");
        return;
    }
    if (passwordEdit_->text() != confirmEdit_->text()) {
        errorLabel_->setText("The two passwords are not the same.");
        return;
    }
    QDialog::accept();
}

// ---- Reset a password (FR20) ----
PasswordDialog::PasswordDialog(const QString& userName, QWidget* parent) : QDialog(parent) {
    setWindowTitle("Reset password");

    passwordEdit_ = makeEdit(this, "passwordEdit", true);
    confirmEdit_ = makeEdit(this, "confirmEdit", true);
    errorLabel_ = makeErrorLabel(this);

    auto* form = new QFormLayout();
    form->addRow(new QLabel("User: " + userName, this));
    form->addRow("&New password:", passwordEdit_);
    form->addRow("&Repeat password:", confirmEdit_);

    auto* layout = new QVBoxLayout(this);
    layout->addLayout(form);
    layout->addWidget(new QLabel("At least 8 characters, with a letter and a digit.", this));
    layout->addWidget(errorLabel_);
    layout->addWidget(makeButtons(this));
}

QString PasswordDialog::password() const { return passwordEdit_->text(); }

void PasswordDialog::accept() {
    if (passwordEdit_->text().isEmpty()) {
        errorLabel_->setText("Enter a password.");
        return;
    }
    if (passwordEdit_->text() != confirmEdit_->text()) {
        errorLabel_->setText("The two passwords are not the same.");
        return;
    }
    QDialog::accept();
}

// ---- One text value (a department name) ----
TextDialog::TextDialog(const QString& title, const QString& label, QWidget* parent)
    : QDialog(parent) {
    setWindowTitle(title);
    textEdit_ = makeEdit(this, "textEdit");
    errorLabel_ = makeErrorLabel(this);

    auto* form = new QFormLayout();
    form->addRow(label, textEdit_);
    auto* layout = new QVBoxLayout(this);
    layout->addLayout(form);
    layout->addWidget(errorLabel_);
    layout->addWidget(makeButtons(this));
}

QString TextDialog::text() const { return textEdit_->text().trimmed(); }

void TextDialog::accept() {
    if (text().isEmpty()) {
        errorLabel_->setText("Enter a value.");
        return;
    }
    QDialog::accept();
}

// ---- New course (FR4) ----
CourseDialog::CourseDialog(const std::vector<lms::domain::DepartmentInfo>& departments,
                           QWidget* parent)
    : QDialog(parent) {
    setWindowTitle("New course");
    codeEdit_ = makeEdit(this, "codeEdit");
    titleEdit_ = makeEdit(this, "titleEdit");
    departmentCombo_ = makeDepartmentCombo(this, departments);
    errorLabel_ = makeErrorLabel(this);

    auto* form = new QFormLayout();
    form->addRow("&Code:", codeEdit_);
    form->addRow("&Title:", titleEdit_);
    form->addRow("&Department:", departmentCombo_);
    auto* layout = new QVBoxLayout(this);
    layout->addLayout(form);
    layout->addWidget(errorLabel_);
    layout->addWidget(makeButtons(this));
}

QString CourseDialog::code() const { return codeEdit_->text().trimmed(); }
QString CourseDialog::title() const { return titleEdit_->text().trimmed(); }
int CourseDialog::departmentId() const { return departmentCombo_->currentData().toInt(); }

void CourseDialog::accept() {
    if (code().isEmpty() || title().isEmpty()) {
        errorLabel_->setText("Enter a code and a title.");
        return;
    }
    if (departmentCombo_->count() == 0) {
        errorLabel_->setText("Create a department first.");
        return;
    }
    QDialog::accept();
}

// ---- Choose a lecturer or a student (FR5) ----
ChoiceDialog::ChoiceDialog(const QString& title, const QString& label,
                           const std::vector<std::pair<int, QString>>& items, QWidget* parent)
    : QDialog(parent) {
    setWindowTitle(title);
    combo_ = new QComboBox(this);
    combo_->setObjectName("choiceCombo");
    for (const auto& item : items) {
        combo_->addItem(item.second, item.first);
    }
    errorLabel_ = makeErrorLabel(this);

    auto* form = new QFormLayout();
    form->addRow(label, combo_);
    auto* layout = new QVBoxLayout(this);
    layout->addLayout(form);
    layout->addWidget(errorLabel_);
    layout->addWidget(makeButtons(this));
}

int ChoiceDialog::selectedId() const { return combo_->currentData().toInt(); }

void ChoiceDialog::accept() {
    if (combo_->count() == 0) {
        errorLabel_->setText("There is nothing to choose from.");
        return;
    }
    QDialog::accept();
}

}  // namespace lms::ui
