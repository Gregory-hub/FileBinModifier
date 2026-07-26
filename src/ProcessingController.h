#pragma once

#include "AppSettings.h"

#include <QObject>
#include <QQueue>
#include <QSet>
#include <QString>
#include <QThread>
#include <memory>

class FileXorWorker;
class QTimer;

/**
 * Orchestrates file discovery, queueing, naming and worker lifetime.
 * Usage: create once in MainWindow, call start/pause/resume/stop and connect signals.
 */
class ProcessingController : public QObject
{
    Q_OBJECT

public:
    explicit ProcessingController(QObject *parent = nullptr);
    ~ProcessingController() override;

    bool isRunning() const;
    bool isPaused() const;
    bool isBusy() const;

public slots:
    void start(const AppSettings &settings);
    void pause();
    void resume();
    void stop();
    bool requestShutdown(int timeoutMs = 10000);

signals:
    void progress(qint64 bytesDone, qint64 totalBytes);
    void statusChanged(const QString &status);
    void runningChanged(bool running);
    void pausedChanged(bool paused);
    void finishedAll();

private slots:
    void onPollTimer();
    void onFileFinished(const QString &inputPath, bool success, const QString &message);

private:
    void scanAndEnqueue();
    void processNext();
    void emitActiveStatus();
    QStringList expandMasks(const QString &maskText) const;
    QString resolveOutputPath(const QString &inputFilePath) const;
    void setRunning(bool running);
    void setPaused(bool paused);
    void cleanupWorkerThread(int waitTimeoutMs = 3000);

    AppSettings m_settings;
    QQueue<QString> m_queue;
    QSet<QString> m_seenInputs;
    QSet<QString> m_inProgress;
    bool m_running = false;
    bool m_paused = false;
    bool m_workerBusy = false;
    bool m_stopRequested = false;

    std::unique_ptr<QTimer> m_pollTimer;
    std::unique_ptr<QThread> m_thread;
    FileXorWorker *m_worker = nullptr;
};
