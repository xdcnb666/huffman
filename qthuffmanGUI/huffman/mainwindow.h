#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QThread>
#include <QElapsedTimer>

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

// 工作线程类
class CompressWorker : public QObject
{
    Q_OBJECT
public:
    explicit CompressWorker(bool isCompress, QString inPath, QString outPath, QObject *parent = nullptr);
public slots:
    void doWork();
signals:
    void progressUpdated(double value);
    void resultReady(bool success, QString message, double elapsedSeconds, double ratio);
private:
    bool m_isCompress;
    QString m_inputPath;
    QString m_outputPath;
};

// 主窗口类
class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void onSelectFileClicked();      // 选择文件
    void onSelectDirClicked();       // 选择文件夹
    void onSelectOutputClicked();    // 选择输出（文件或目录，自动判断）
    void onCompressClicked();
    void onDecompressClicked();
    void updateProgress(double value);
    void handleWorkFinished(bool success, QString message, double elapsedSeconds, double ratio);

private:
    void startWork(bool isCompress, QString inPath, QString outPath);
    bool isArchiveFile(const QString &path);   // 判断是否为多文件归档包
    Ui::MainWindow *ui;
    QThread *m_workerThread;
};

#endif // MAINWINDOW_H