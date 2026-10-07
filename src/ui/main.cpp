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
#include "DemoData.h"
#include "FirstRunDialog.h"
#include "LoginWindow.h"
#include "UserWindow.h"
#include "UserWindowFactory.h"

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    QCoreApplication::setApplicationName("LMS");

    QCommandLineParser parser;
    parser.addHelpOption();
    QCommandLineOption dbOption("db", "Path of the database file.", "file");
    QCommandLineOption demoOption("demo", "Fill a new database with demo accounts and data.");
    parser.addOption(dbOption);
    parser.addOption(demoOption);
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
            if (parser.isSet(demoOption)) {
                lms::app::seedDemoData(context);
                QMessageBox::information(
                    nullptr, "Moderated LMS - demo data",
                    "Demo accounts were created.\n\n"
                    "Usernames: admin, lecturer1, hod1, student1, student2\n"
                    "Password for all of them: Passw0rd1");
            } else {
                lms::ui::FirstRunDialog firstRun(context);
                if (firstRun.exec() != QDialog::Accepted) {
                    return 0;
                }
            }
        }

        lms::ui::LoginWindow login(context);
        std::unique_ptr<lms::ui::UserWindow> userWindow;

        QObject::connect(
            &login, &lms::ui::LoginWindow::loggedIn,
            [&](std::shared_ptr<lms::domain::User> user) {
                userWindow = lms::ui::makeUserWindow(context, std::move(user));
                QObject::connect(userWindow.get(), &lms::ui::UserWindow::loggedOut, [&]() {
                    userWindow->hide();
                    login.show();
                    QTimer::singleShot(0, [&]() { userWindow.reset(); });
                });
                login.hide();
                userWindow->show();
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
