#include "MainWindow.h"

#include "ProcessingController.h"

#include <QCheckBox>
#include <QCloseEvent>
#include <QFileDialog>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QProgressBar>
#include <QPushButton>
#include <QRadioButton>
#include <QSpinBox>
#include <QVBoxLayout>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , m_controller(std::make_unique<ProcessingController>(this))
{
    setWindowTitle(tr("File Binary Modifier"));
    resize(720, 520);
    buildUi();

    connect(m_controller.get(), &ProcessingController::progress, this, &MainWindow::onProgress);
    connect(m_controller.get(), &ProcessingController::statusChanged, this, &MainWindow::onStatusChanged);
    connect(m_controller.get(), &ProcessingController::runningChanged, this, &MainWindow::onRunningChanged);
    connect(m_controller.get(), &ProcessingController::pausedChanged, this, &MainWindow::onPausedChanged);
    connect(m_controller.get(), &ProcessingController::finishedAll, this, [this]() {
        if (!m_closing)
            setControlsEnabled(true);
    });

    onRunModeChanged();
    onRunningChanged(false);
}

MainWindow::~MainWindow() = default;

void MainWindow::buildUi()
{
    auto *central = new QWidget(this);
    setCentralWidget(central);

    auto *rootLayout = new QVBoxLayout(central);

    auto *settingsGroup = new QGroupBox(tr("Settings"), central);
    auto *form = new QFormLayout(settingsGroup);

    m_maskEdit = new QLineEdit(settingsGroup);
    m_maskEdit->setPlaceholderText(tr("e.g. *.txt; testFile.bin"));
    m_maskEdit->setText(QStringLiteral("*.*"));
    form->addRow(tr("Input file mask (a)"), m_maskEdit);

    m_deleteInputCheck = new QCheckBox(tr("Delete input files after successful processing"), settingsGroup);
    form->addRow(tr("Delete inputs (b)"), m_deleteInputCheck);

    auto *inputRow = new QWidget(settingsGroup);
    auto *inputLayout = new QHBoxLayout(inputRow);
    inputLayout->setContentsMargins(0, 0, 0, 0);
    m_inputPathEdit = new QLineEdit(inputRow);
    auto *browseInput = new QPushButton(tr("Browse…"), inputRow);
    inputLayout->addWidget(m_inputPathEdit);
    inputLayout->addWidget(browseInput);
    form->addRow(tr("Input search path (d)"), inputRow);
    connect(browseInput, &QPushButton::clicked, this, &MainWindow::onBrowseInput);

    auto *outputRow = new QWidget(settingsGroup);
    auto *outputLayout = new QHBoxLayout(outputRow);
    outputLayout->setContentsMargins(0, 0, 0, 0);
    m_outputPathEdit = new QLineEdit(outputRow);
    auto *browseOutput = new QPushButton(tr("Browse…"), outputRow);
    outputLayout->addWidget(m_outputPathEdit);
    outputLayout->addWidget(browseOutput);
    form->addRow(tr("Output path (c)"), outputRow);
    connect(browseOutput, &QPushButton::clicked, this, &MainWindow::onBrowseOutput);

    auto *nameGroup = new QWidget(settingsGroup);
    auto *nameLayout = new QHBoxLayout(nameGroup);
    nameLayout->setContentsMargins(0, 0, 0, 0);
    m_overwriteRadio = new QRadioButton(tr("Overwrite"), nameGroup);
    m_counterRadio = new QRadioButton(tr("Add counter suffix"), nameGroup);
    m_overwriteRadio->setChecked(true);
    nameLayout->addWidget(m_overwriteRadio);
    nameLayout->addWidget(m_counterRadio);
    nameLayout->addStretch();
    form->addRow(tr("Name conflict (e)"), nameGroup);

    auto *runGroup = new QWidget(settingsGroup);
    auto *runLayout = new QHBoxLayout(runGroup);
    runLayout->setContentsMargins(0, 0, 0, 0);
    m_onceRadio = new QRadioButton(tr("Run once"), runGroup);
    m_timerRadio = new QRadioButton(tr("Timer / poll"), runGroup);
    m_onceRadio->setChecked(true);
    runLayout->addWidget(m_onceRadio);
    runLayout->addWidget(m_timerRadio);
    runLayout->addStretch();
    form->addRow(tr("Run mode (f)"), runGroup);
    connect(m_onceRadio, &QRadioButton::toggled, this, &MainWindow::onRunModeChanged);
    connect(m_timerRadio, &QRadioButton::toggled, this, &MainWindow::onRunModeChanged);

    m_pollIntervalSpin = new QSpinBox(settingsGroup);
    m_pollIntervalSpin->setRange(100, 3600000);
    m_pollIntervalSpin->setSingleStep(100);
    m_pollIntervalSpin->setSuffix(tr(" ms"));
    m_pollIntervalSpin->setValue(1000);
    form->addRow(tr("Poll interval (g)"), m_pollIntervalSpin);

    m_xorKeyEdit = new QLineEdit(settingsGroup);
    m_xorKeyEdit->setPlaceholderText(tr("16 hex chars, e.g. 1234567890ABCDEF"));
    m_xorKeyEdit->setText(QStringLiteral("1234567890ABCDEF"));
    m_xorKeyEdit->setMaxLength(16);
    form->addRow(tr("XOR key hex (h)"), m_xorKeyEdit);

    rootLayout->addWidget(settingsGroup);

    auto *progressGroup = new QGroupBox(tr("Progress"), central);
    auto *progressLayout = new QVBoxLayout(progressGroup);
    m_progressBar = new QProgressBar(progressGroup);
    m_progressBar->setRange(0, 100);
    m_progressBar->setValue(0);
    m_statusLabel = new QLabel(tr("Ready"), progressGroup);
    m_statusLabel->setWordWrap(true);
    progressLayout->addWidget(m_progressBar);
    progressLayout->addWidget(m_statusLabel);
    rootLayout->addWidget(progressGroup);

    auto *buttons = new QHBoxLayout();
    m_startButton = new QPushButton(tr("Start"), central);
    m_pauseButton = new QPushButton(tr("Pause"), central);
    m_stopButton = new QPushButton(tr("Stop"), central);
    buttons->addWidget(m_startButton);
    buttons->addWidget(m_pauseButton);
    buttons->addWidget(m_stopButton);
    buttons->addStretch();
    rootLayout->addLayout(buttons);

    connect(m_startButton, &QPushButton::clicked, this, &MainWindow::onStart);
    connect(m_pauseButton, &QPushButton::clicked, this, &MainWindow::onPauseResume);
    connect(m_stopButton, &QPushButton::clicked, this, &MainWindow::onStop);
}

void MainWindow::onBrowseInput()
{
    const QString path = QFileDialog::getExistingDirectory(this, tr("Select input directory"), m_inputPathEdit->text());
    if (!path.isEmpty())
        m_inputPathEdit->setText(path);
}

void MainWindow::onBrowseOutput()
{
    const QString path = QFileDialog::getExistingDirectory(this, tr("Select output directory"), m_outputPathEdit->text());
    if (!path.isEmpty())
        m_outputPathEdit->setText(path);
}

void MainWindow::onStart()
{
    AppSettings settings;
    QString error;
    if (!collectSettings(settings, error)) {
        QMessageBox::warning(this, tr("Invalid settings"), error);
        return;
    }

    m_progressBar->setValue(0);
    setControlsEnabled(false);
    m_controller->start(settings);
}

void MainWindow::onPauseResume()
{
    if (m_controller->isPaused())
        m_controller->resume();
    else
        m_controller->pause();
}

void MainWindow::onStop()
{
    m_controller->stop();
}

void MainWindow::onRunningChanged(bool running)
{
    m_startButton->setEnabled(!running);
    m_pauseButton->setEnabled(running);
    m_stopButton->setEnabled(running);
    if (!running) {
        m_pauseButton->setText(tr("Pause"));
        setControlsEnabled(true);
    } else {
        setControlsEnabled(false);
    }
}

void MainWindow::onPausedChanged(bool paused)
{
    m_pauseButton->setText(paused ? tr("Resume") : tr("Pause"));
}

void MainWindow::onProgress(qint64 bytesDone, qint64 totalBytes)
{
    if (totalBytes <= 0) {
        m_progressBar->setRange(0, 0);
        return;
    }

    m_progressBar->setRange(0, 1000);
    const int value = static_cast<int>((bytesDone * 1000) / totalBytes);
    m_progressBar->setValue(value);
}

void MainWindow::onStatusChanged(const QString &status)
{
    m_statusLabel->setText(status);
}

void MainWindow::onRunModeChanged()
{
    m_pollIntervalSpin->setEnabled(m_timerRadio->isChecked() && m_startButton->isEnabled());
}

bool MainWindow::collectSettings(AppSettings &settings, QString &error) const
{
    settings.fileMask = m_maskEdit->text().trimmed();
    if (settings.fileMask.isEmpty()) {
        error = tr("File mask must not be empty");
        return false;
    }

    settings.deleteInputFiles = m_deleteInputCheck->isChecked();
    settings.outputPath = m_outputPathEdit->text().trimmed();
    settings.inputPath = m_inputPathEdit->text().trimmed();

    if (settings.inputPath.isEmpty()) {
        error = tr("Input path is required");
        return false;
    }
    if (settings.outputPath.isEmpty()) {
        error = tr("Output path is required");
        return false;
    }

    settings.nameConflictPolicy = m_overwriteRadio->isChecked()
        ? AppSettings::NameConflictPolicy::Overwrite
        : AppSettings::NameConflictPolicy::CounterSuffix;

    settings.runMode = m_onceRadio->isChecked()
        ? AppSettings::RunMode::Once
        : AppSettings::RunMode::Timer;

    settings.pollIntervalMs = m_pollIntervalSpin->value();
    settings.xorKey = parseXorKey(m_xorKeyEdit->text(), error);
    if (settings.xorKey.isEmpty())
        return false;

    return true;
}

void MainWindow::setControlsEnabled(bool enabled)
{
    m_maskEdit->setEnabled(enabled);
    m_deleteInputCheck->setEnabled(enabled);
    m_outputPathEdit->setEnabled(enabled);
    m_inputPathEdit->setEnabled(enabled);
    m_overwriteRadio->setEnabled(enabled);
    m_counterRadio->setEnabled(enabled);
    m_onceRadio->setEnabled(enabled);
    m_timerRadio->setEnabled(enabled);
    m_xorKeyEdit->setEnabled(enabled);
    m_pollIntervalSpin->setEnabled(enabled && m_timerRadio->isChecked());
}

QByteArray MainWindow::parseXorKey(const QString &hexText, QString &error)
{
    const QString normalized = hexText.trimmed().toUpper();
    if (normalized.size() != 16) {
        error = tr("XOR key must be exactly 16 hexadecimal characters");
        return {};
    }

    for (const QChar ch : normalized) {
        if (!ch.isDigit() && (ch < QLatin1Char('A') || ch > QLatin1Char('F'))) {
            error = tr("XOR key contains invalid hexadecimal character");
            return {};
        }
    }

    QByteArray key = QByteArray::fromHex(normalized.toLatin1());
    if (key.size() != 8) {
        error = tr("Failed to parse XOR key");
        return {};
    }
    return key;
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    if (!m_controller->isRunning() && !m_controller->isBusy()) {
        event->accept();
        return;
    }

    m_closing = true;
    m_statusLabel->setText(tr("Stopping before exit…"));
    m_startButton->setEnabled(false);
    m_pauseButton->setEnabled(false);
    m_stopButton->setEnabled(false);

    m_controller->requestShutdown(10000);
    event->accept();
}
