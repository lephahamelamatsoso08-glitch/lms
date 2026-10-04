#include <exception>
#include <memory>

#include <QApplication>
#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QDialog>
#include <QDir>
#include <QMessageBox>
#include <QStandardPaths>
#include <QTimer>

#include "AppContext.h"
#include "FirstRunDialog.h"
#include "LoginWindow.h"
#include "RoleWindow.h"

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    QCoreApplication::setApplicationName("LMS");

    QCommandLineParser parser;
    parser.addHelpOption();
    QCommandLineOption dbOption("db", "Path of the database file.", "file");
    parser.addOption(dbOption);
    parser.process(app);

    // The database file: --db <file>, or a folder for the user's application data.
    QString databasePath = parser.value(dbOption);
    if (databasePath.isEmpty()) {
        const QString folder = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
        QDir().mkpath(folder);
        databasePath = folder + "/lms.db";
    }

    try {
        lms::app::AppContext context(databasePath.toStdString());

        if (context.needsFirstAdmin()) {
            lms::ui::FirstRunDialog firstRun(context);
            if (firstRun.exec() != QDialog::Accepted) {
                return 0;
            }
        }

        lms::ui::LoginWindow login(context);
        std::unique_ptr<lms::ui::RoleWindow> roleWindow;

        QObject::connect(&login, &lms::ui::LoginWindow::loggedIn,
                         [&](std::shared_ptr<lms::domain::User> user) {
                             roleWindow =
                                 std::make_unique<lms::ui::RoleWindow>(context, std::move(user));
                             QObject::connect(roleWindow.get(), &lms::ui::RoleWindow::loggedOut,
                                              [&]() {
                                                  roleWindow->hide();
                                                  login.show();
                                                  QTimer::singleShot(0, [&]() { roleWindow.reset(); });
                                              });
                             login.hide();
                             roleWindow->show();
                         });

        login.show();
        return app.exec();
    } catch (const std::exception& error) {
        QMessageBox::critical(nullptr, "Moderated LMS",
                              QString("The program cannot start:\n") +
                                  QString::fromUtf8(error.what()));
        return 1;
    }
}
