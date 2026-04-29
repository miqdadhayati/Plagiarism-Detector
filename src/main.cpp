#include <QApplication>
#include "mainwindow.hpp"

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);

    app.setApplicationName("VP-Tree Plagiarism Detector");
    app.setApplicationVersion("1.0.0");
    app.setOrganizationName("VPTreePlagiarism");

    // Set default font
    QFont font("Segoe UI", 10);
    font.setStyleHint(QFont::SansSerif);
    app.setFont(font);

    MainWindow window;
    window.show();

    return app.exec();
}
