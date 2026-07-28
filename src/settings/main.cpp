#include <QApplication>
#include <QIcon>
#include "settings_window.h"

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    app.setApplicationName("fcitx5-skey-settings");
    app.setApplicationDisplayName(QString::fromUtf8("Skey - Tùy chỉnh"));

    // Critical on Wayland: the compositor uses the .desktop file's Icon=
    // for the taskbar, not setWindowIcon().  desktopFileName must match
    // the basename of the installed .desktop file without the extension.
    app.setDesktopFileName("fcitx5-skey-settings");

    // Window icon as PNG — works everywhere (Qt renders PNG natively).
    app.setWindowIcon(QIcon("/usr/share/icons/hicolor/128x128/apps/fcitx-skey.png"));

    SkeySettingsWindow window;
    window.show();

    return app.exec();
}

