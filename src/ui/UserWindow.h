#pragma once

#include <QMainWindow>

namespace lms::ui {

// Base class of the page shown after login. Each role has its own subclass.
class UserWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit UserWindow(QWidget* parent = nullptr) : QMainWindow(parent) {}

signals:
    void loggedOut();
};

}  // namespace lms::ui
