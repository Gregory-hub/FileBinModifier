#pragma once

#include "AppSettings.h"

#include <QMainWindow>
#include <memory>

class ProcessingController;
class QCheckBox;
class QLineEdit;
class QProgressBar;
class QPushButton;
class QRadioButton;
class QSpinBox;
class QLabel;

/**
 * Main application window with processing settings and progress UI.
 * Usage: create in main() and show(); owns ProcessingController.
 */
class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

protected:
    void closeEvent(QCloseEvent *event) override;

private slots:
    void onBrowseInput();
    void onBrowseOutput();
    void onStart();
    void onPauseResume();
    void onStop();
    void onRunningChanged(bool running);
    void onPausedChanged(bool paused);
    void onProgress(qint64 bytesDone, qint64 totalBytes);
    void onStatusChanged(const QString &status);
    void onRunModeChanged();

private:
    void buildUi();
    bool collectSettings(AppSettings &settings, QString &error) const;
    void setControlsEnabled(bool enabled);
    static QByteArray parseXorKey(const QString &hexText, QString &error);

    QLineEdit *m_maskEdit = nullptr;
    QCheckBox *m_deleteInputCheck = nullptr;
    QLineEdit *m_outputPathEdit = nullptr;
    QLineEdit *m_inputPathEdit = nullptr;
    QRadioButton *m_overwriteRadio = nullptr;
    QRadioButton *m_counterRadio = nullptr;
    QRadioButton *m_onceRadio = nullptr;
    QRadioButton *m_timerRadio = nullptr;
    QSpinBox *m_pollIntervalSpin = nullptr;
    QLineEdit *m_xorKeyEdit = nullptr;
    QProgressBar *m_progressBar = nullptr;
    QLabel *m_statusLabel = nullptr;
    QPushButton *m_startButton = nullptr;
    QPushButton *m_pauseButton = nullptr;
    QPushButton *m_stopButton = nullptr;

    std::unique_ptr<ProcessingController> m_controller;
    bool m_closing = false;
};
