#include "FirstRunDialog.h"

#include <exception>

#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>

namespace lms::ui {

FirstRunDialog::FirstRunDialog(lms::app::AppContext& context, QWidget* parent)
    : QDialog(parent), context_(context) {
    setWindowTitle("Moderated LMS - First start");

    auto* intro = new QLabel(
        "The system has no users yet.\nCreate the first administrator account.", this);

    usernameEdit_ = new QLineEdit(this);
    usernameEdit_->setObjectName("usernameEdit");
    fullNameEdit_ = new QLineEdit(this);
    fullNameEdit_->setObjectName("fullNameEdit");
    passwordEdit_ = new QLineEdit(this);
    passwordEdit_->setObjectName("passwordEdit");
    passwordEdit_->setEchoMode(QLineEdit::Password);
    confirmEdit_ = new QLineEdit(this);
    confirmEdit_->setObjectName("confirmEdit");
    confirmEdit_->setEchoMode(QLineEdit::Password);

    auto* rule = new QLabel("The password needs at least 8 characters, with a letter and a digit.",
                            this);
    rule->setWordWrap(true);

    errorLabel_ = new QLabel(this);
    errorLabel_->setObjectName("errorLabel");
    errorLabel_->setWordWrap(true);
    errorLabel_->setStyleSheet("color: #b00020;");

    createButton_ = new QPushButton("Create administrator", this);
    createButton_->setObjectName("createButton");

    auto* form = new QFormLayout();
    form->addRow("&Username:", usernameEdit_);
    form->addRow("&Full name:", fullNameEdit_);
    form->addRow("&Password:", passwordEdit_);
    form->addRow("&Repeat password:", confirmEdit_);

    auto* layout = new QVBoxLayout(this);
    layout->addWidget(intro);
    layout->addLayout(form);
    layout->addWidget(rule);
    layout->addWidget(errorLabel_);
    layout->addWidget(createButton_);

    connect(createButton_, &QPushButton::clicked, this, &FirstRunDialog::createAdministrator);
}

void FirstRunDialog::createAdministrator() {
    errorLabel_->clear();
    if (passwordEdit_->text() != confirmEdit_->text()) {
        errorLabel_->setText("The two passwords are not the same.");
        return;
    }
    try {
        context_.createFirstAdmin(usernameEdit_->text().trimmed().toStdString(),
                                  fullNameEdit_->text().trimmed().toStdString(),
                                  passwordEdit_->text().toStdString());
        accept();
    } catch (const std::exception& error) {
        errorLabel_->setText(QString::fromUtf8(error.what()));
    }
}

}  // namespace lms::ui
