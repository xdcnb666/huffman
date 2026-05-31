#include <QApplication>
#include "mainwindow.h"

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    // 全局样式表：白色背景 + 黑色文字 + 浅色按钮
    a.setStyleSheet(
        "QWidget { background-color: white; color: black; }"
        "QPushButton { background-color: #e1e1e1; border: 1px solid #ccc; padding: 4px; border-radius: 3px; }"
        "QPushButton:hover { background-color: #c0c0c0; }"
        "QPushButton:pressed { background-color: #a0a0a0; }"
        "QLineEdit { background-color: white; border: 1px solid #ccc; padding: 2px; }"
        "QLabel { color: black; }"
        "QProgressBar { border: 1px solid #ccc; background-color: #f0f0f0; text-align: center; }"
        "QProgressBar::chunk { background-color: #4caf50; }"
        );

    MainWindow w;
    w.show();
    return a.exec();
}