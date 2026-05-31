/********************************************************************************
** Form generated from reading UI file 'mainwindow.ui'
**
** Created by: Qt User Interface Compiler version 6.11.1
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_MAINWINDOW_H
#define UI_MAINWINDOW_H

#include <QtCore/QVariant>
#include <QtWidgets/QApplication>
#include <QtWidgets/QLabel>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QMainWindow>
#include <QtWidgets/QMenuBar>
#include <QtWidgets/QProgressBar>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QStatusBar>
#include <QtWidgets/QWidget>

QT_BEGIN_NAMESPACE

class Ui_MainWindow
{
public:
    QWidget *centralwidget;
    QPushButton *selectFileBtn;
    QProgressBar *progressBar;
    QLabel *statusLabel;
    QLabel *timeLabel;
    QLabel *ratioLabel;
    QPushButton *selectOutputBtn;
    QPushButton *compressBtn;
    QPushButton *decompressBtn;
    QLineEdit *filePathEdit;
    QLineEdit *outputPathEdit;
    QPushButton *selectDirBtn;
    QMenuBar *menubar;
    QStatusBar *statusbar;

    void setupUi(QMainWindow *MainWindow)
    {
        if (MainWindow->objectName().isEmpty())
            MainWindow->setObjectName("MainWindow");
        MainWindow->resize(759, 393);
        centralwidget = new QWidget(MainWindow);
        centralwidget->setObjectName("centralwidget");
        selectFileBtn = new QPushButton(centralwidget);
        selectFileBtn->setObjectName("selectFileBtn");
        selectFileBtn->setGeometry(QRect(530, 30, 101, 31));
        progressBar = new QProgressBar(centralwidget);
        progressBar->setObjectName("progressBar");
        progressBar->setGeometry(QRect(20, 110, 501, 31));
        progressBar->setValue(24);
        statusLabel = new QLabel(centralwidget);
        statusLabel->setObjectName("statusLabel");
        statusLabel->setGeometry(QRect(460, 190, 91, 31));
        timeLabel = new QLabel(centralwidget);
        timeLabel->setObjectName("timeLabel");
        timeLabel->setGeometry(QRect(460, 230, 101, 31));
        ratioLabel = new QLabel(centralwidget);
        ratioLabel->setObjectName("ratioLabel");
        ratioLabel->setGeometry(QRect(460, 270, 91, 31));
        selectOutputBtn = new QPushButton(centralwidget);
        selectOutputBtn->setObjectName("selectOutputBtn");
        selectOutputBtn->setGeometry(QRect(530, 70, 211, 31));
        compressBtn = new QPushButton(centralwidget);
        compressBtn->setObjectName("compressBtn");
        compressBtn->setGeometry(QRect(120, 190, 101, 41));
        decompressBtn = new QPushButton(centralwidget);
        decompressBtn->setObjectName("decompressBtn");
        decompressBtn->setGeometry(QRect(300, 190, 101, 41));
        filePathEdit = new QLineEdit(centralwidget);
        filePathEdit->setObjectName("filePathEdit");
        filePathEdit->setGeometry(QRect(20, 30, 501, 31));
        outputPathEdit = new QLineEdit(centralwidget);
        outputPathEdit->setObjectName("outputPathEdit");
        outputPathEdit->setGeometry(QRect(20, 70, 501, 31));
        selectDirBtn = new QPushButton(centralwidget);
        selectDirBtn->setObjectName("selectDirBtn");
        selectDirBtn->setGeometry(QRect(640, 30, 101, 31));
        MainWindow->setCentralWidget(centralwidget);
        menubar = new QMenuBar(MainWindow);
        menubar->setObjectName("menubar");
        menubar->setGeometry(QRect(0, 0, 759, 17));
        MainWindow->setMenuBar(menubar);
        statusbar = new QStatusBar(MainWindow);
        statusbar->setObjectName("statusbar");
        MainWindow->setStatusBar(statusbar);

        retranslateUi(MainWindow);

        QMetaObject::connectSlotsByName(MainWindow);
    } // setupUi

    void retranslateUi(QMainWindow *MainWindow)
    {
        MainWindow->setWindowTitle(QCoreApplication::translate("MainWindow", "MainWindow", nullptr));
        selectFileBtn->setText(QCoreApplication::translate("MainWindow", "\351\200\211\346\213\251\346\226\207\344\273\266\345\234\260\345\235\200", nullptr));
        statusLabel->setText(QCoreApplication::translate("MainWindow", "\347\212\266\346\200\201", nullptr));
        timeLabel->setText(QCoreApplication::translate("MainWindow", "\350\200\227\346\227\266", nullptr));
        ratioLabel->setText(QCoreApplication::translate("MainWindow", "\345\216\213\347\274\251\347\216\207", nullptr));
        selectOutputBtn->setText(QCoreApplication::translate("MainWindow", "\351\200\211\346\213\251\350\276\223\345\207\272\345\234\260\345\235\200", nullptr));
        compressBtn->setText(QCoreApplication::translate("MainWindow", "\345\216\213\347\274\251", nullptr));
        decompressBtn->setText(QCoreApplication::translate("MainWindow", "\350\247\243\345\216\213", nullptr));
        selectDirBtn->setText(QCoreApplication::translate("MainWindow", "\351\200\211\346\213\251\346\226\207\344\273\266\345\244\271", nullptr));
    } // retranslateUi

};

namespace Ui {
    class MainWindow: public Ui_MainWindow {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_MAINWINDOW_H
