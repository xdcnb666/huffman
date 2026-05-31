#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "huffman.h"
#include <QFileDialog>
#include <QMessageBox>
#include <QFileInfo>
#include <QDir>
#include <filesystem>
#include <chrono>
#include <fstream>

// ------------------ CompressWorker ------------------
CompressWorker::CompressWorker(bool isCompress, QString inPath, QString outPath, QObject *parent)
    : QObject(parent), m_isCompress(isCompress), m_inputPath(inPath), m_outputPath(outPath)
{}

void CompressWorker::doWork()
{
    bool success = false;
    double elapsed = 0.0;
    double ratio = 0.0;
    QString message;

    auto start = std::chrono::high_resolution_clock::now();
    auto callback = [this](double progress) {
        emit progressUpdated(progress);
    };

    std::filesystem::path input = m_inputPath.toStdString();
    std::filesystem::path output = m_outputPath.toStdString();

    if (m_isCompress) {
        // 判断是文件还是目录
        std::error_code ec;
        if (std::filesystem::is_directory(input, ec)) {
            // 压缩文件夹
            std::vector<std::filesystem::path> paths = { input };
            std::filesystem::path rootDir = input.parent_path();
            success = HuffmanLib::CompressFiles(paths, output, rootDir, callback, false);
            // 归档压缩暂时不计算压缩率
            ratio = 0.0;
        } else {
            // 压缩单个文件
            uint64_t origSize = 0;
            if (std::filesystem::exists(input, ec))
                origSize = std::filesystem::file_size(input, ec);
            success = HuffmanLib::CompressFile(input, output, callback);
            if (success && origSize > 0) {
                uint64_t compSize = std::filesystem::file_size(output, ec);
                if (compSize > 0)
                    ratio = 100.0 * compSize / origSize;
            }
        }
    } else {
        // 解压：判断是归档包还是单文件包
        std::ifstream fin(input.string(), std::ios::binary);
        uint32_t magic = 0;
        if (fin.read(reinterpret_cast<char*>(&magic), 4) && magic == HuffmanLib::ARCHIVE_MAGIC) {
            success = HuffmanLib::DecompressArchive(input, output, callback);
        } else {
            success = HuffmanLib::DecompressFile(input, output, callback);
        }
    }

    auto end = std::chrono::high_resolution_clock::now();
    elapsed = std::chrono::duration<double>(end - start).count();

    message = success ? (m_isCompress ? "压缩成功" : "解压成功") : (m_isCompress ? "压缩失败" : "解压失败");
    emit resultReady(success, message, elapsed, ratio);
}

// ------------------ MainWindow ------------------
MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , m_workerThread(nullptr)
{
    ui->setupUi(this);

    // 连接信号槽
    connect(ui->selectFileBtn, &QPushButton::clicked, this, &MainWindow::onSelectFileClicked);
    connect(ui->selectDirBtn, &QPushButton::clicked, this, &MainWindow::onSelectDirClicked);
    connect(ui->selectOutputBtn, &QPushButton::clicked, this, &MainWindow::onSelectOutputClicked);
    connect(ui->compressBtn, &QPushButton::clicked, this, &MainWindow::onCompressClicked);
    connect(ui->decompressBtn, &QPushButton::clicked, this, &MainWindow::onDecompressClicked);

    ui->progressBar->setRange(0, 100);
    ui->progressBar->setValue(0);
}

MainWindow::~MainWindow()
{
    if (m_workerThread && m_workerThread->isRunning()) {
        m_workerThread->quit();
        m_workerThread->wait();
    }
    delete m_workerThread;
    delete ui;
}

void MainWindow::onSelectFileClicked()
{
    QString file = QFileDialog::getOpenFileName(this, "选择文件", "", "所有文件 (*.*)");
    if (!file.isEmpty()) {
        ui->filePathEdit->setText(file);
        if (ui->outputPathEdit->text().isEmpty())
            ui->outputPathEdit->setText(file + ".huf");
    }
}

void MainWindow::onSelectDirClicked()
{
    QString dir = QFileDialog::getExistingDirectory(this, "选择文件夹", "");
    if (!dir.isEmpty()) {
        ui->filePathEdit->setText(dir);
        QString archiveName = QFileInfo(dir).fileName() + ".huf";
        QString parentDir = QFileInfo(dir).path();
        if (ui->outputPathEdit->text().isEmpty())
            ui->outputPathEdit->setText(parentDir + "/" + archiveName);
    }
}

void MainWindow::onSelectOutputClicked()
{
    QString input = ui->filePathEdit->text();
    if (input.isEmpty()) {
        QString file = QFileDialog::getSaveFileName(this, "选择输出文件", "", "*.huf;;所有文件 (*.*)");
        if (!file.isEmpty()) ui->outputPathEdit->setText(file);
        return;
    }

    bool isArchive = false;
    if (input.endsWith(".huf", Qt::CaseInsensitive))
        isArchive = isArchiveFile(input);

    if (isArchive) {
        QString dir = QFileDialog::getExistingDirectory(this, "选择解压目录", "");
        if (!dir.isEmpty()) ui->outputPathEdit->setText(dir);
    } else {
        QString defaultName = input + ".dec";
        QString file = QFileDialog::getSaveFileName(this, "选择输出文件", defaultName, "所有文件 (*.*)");
        if (!file.isEmpty()) ui->outputPathEdit->setText(file);
    }
}

void MainWindow::onCompressClicked()
{
    QString inPath = ui->filePathEdit->text();
    if (inPath.isEmpty()) {
        QMessageBox::warning(this, "提示", "请先选择文件或文件夹！");
        return;
    }
    QString outPath = ui->outputPathEdit->text();
    if (outPath.isEmpty()) {
        if (QFileInfo(inPath).isDir())
            outPath = QFileInfo(inPath).path() + "/" + QFileInfo(inPath).fileName() + ".huf";
        else
            outPath = inPath + ".huf";
        ui->outputPathEdit->setText(outPath);
    }
    startWork(true, inPath, outPath);
}

void MainWindow::onDecompressClicked()
{
    QString inPath = ui->filePathEdit->text();
    if (inPath.isEmpty()) {
        QMessageBox::warning(this, "提示", "请先选择压缩文件！");
        return;
    }
    if (!inPath.endsWith(".huf", Qt::CaseInsensitive) && !isArchiveFile(inPath)) {
        QMessageBox::warning(this, "提示", "请选择一个有效的 .huf 压缩文件！");
        return;
    }

    QString outPath = ui->outputPathEdit->text();
    if (outPath.isEmpty()) {
        bool isArchive = isArchiveFile(inPath);
        if (isArchive) {
            QFileInfo info(inPath);
            outPath = info.path() + "/" + info.completeBaseName();
        } else {
            outPath = inPath.left(inPath.lastIndexOf('.'));
        }
        ui->outputPathEdit->setText(outPath);
    }
    startWork(false, inPath, outPath);
}

void MainWindow::startWork(bool isCompress, QString inPath, QString outPath)
{
    ui->compressBtn->setEnabled(false);
    ui->decompressBtn->setEnabled(false);
    ui->selectFileBtn->setEnabled(false);
    ui->selectDirBtn->setEnabled(false);
    ui->selectOutputBtn->setEnabled(false);
    ui->progressBar->setValue(0);
    ui->timeLabel->setText("耗时: -- s");
    ui->ratioLabel->setText("压缩率: -- %");
    ui->statusLabel->setText("状态: 工作中...");

    if (m_workerThread) {
        if (m_workerThread->isRunning()) {
            m_workerThread->quit();
            m_workerThread->wait();
        }
        delete m_workerThread;
    }

    m_workerThread = new QThread(this);
    CompressWorker *worker = new CompressWorker(isCompress, inPath, outPath);
    worker->moveToThread(m_workerThread);

    connect(m_workerThread, &QThread::started, worker, &CompressWorker::doWork);
    connect(worker, &CompressWorker::progressUpdated, this, &MainWindow::updateProgress);
    connect(worker, &CompressWorker::resultReady, this, &MainWindow::handleWorkFinished);
    connect(worker, &CompressWorker::resultReady, worker, &CompressWorker::deleteLater);
    connect(m_workerThread, &QThread::finished, m_workerThread, &QThread::deleteLater);

    m_workerThread->start();
}

void MainWindow::updateProgress(double value)
{
    ui->progressBar->setValue(static_cast<int>(value * 100));
}

void MainWindow::handleWorkFinished(bool success, QString message, double elapsedSeconds, double ratio)
{
    ui->compressBtn->setEnabled(true);
    ui->decompressBtn->setEnabled(true);
    ui->selectFileBtn->setEnabled(true);
    ui->selectDirBtn->setEnabled(true);
    ui->selectOutputBtn->setEnabled(true);
    ui->statusLabel->setText(QString("状态: %1").arg(message));

    if (success) {
        ui->timeLabel->setText(QString("耗时: %1 s").arg(elapsedSeconds, 0, 'f', 3));
        if (ratio > 0)
            ui->ratioLabel->setText(QString("压缩率: %1%").arg(ratio, 0, 'f', 1));
        else
            ui->ratioLabel->setText("压缩率: N/A");
        QMessageBox::information(this, "完成", message);
    } else {
        QMessageBox::critical(this, "错误", message);
        ui->progressBar->setValue(0);
    }
}

bool MainWindow::isArchiveFile(const QString &path)
{
    std::ifstream f(path.toStdString(), std::ios::binary);
    uint32_t magic = 0;
    if (f.read(reinterpret_cast<char*>(&magic), 4))
        return magic == HuffmanLib::ARCHIVE_MAGIC;
    return false;
}