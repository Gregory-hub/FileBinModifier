#include "ProcessingController.h"

#include "FileXorWorker.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMetaObject>
#include <QRegularExpression>
#include <QTimer>

ProcessingController::ProcessingController(QObject *parent)
    : QObject(parent)
    , m_pollTimer(std::make_unique<QTimer>(this))
{
    m_pollTimer->setSingleShot(false);
    connect(m_pollTimer.get(), &QTimer::timeout, this, &ProcessingController::onPollTimer);
}

ProcessingController::~ProcessingController()
{
    requestShutdown(5000);
}

bool ProcessingController::isRunning() const
{
    return m_running;
}

bool ProcessingController::isPaused() const
{
    return m_paused;
}

bool ProcessingController::isBusy() const
{
    return m_workerBusy || !m_queue.isEmpty();
}

void ProcessingController::start(const AppSettings &settings)
{
    if (m_running)
        return;

    m_settings = settings;
    m_stopRequested = false;
    m_queue.clear();
    m_seenInputs.clear();
    m_inProgress.clear();
    setPaused(false);

    cleanupWorkerThread();

    m_thread = std::make_unique<QThread>();
    m_worker = new FileXorWorker();
    m_worker->moveToThread(m_thread.get());

    connect(m_worker, &FileXorWorker::progress, this, &ProcessingController::progress);
    connect(m_worker, &FileXorWorker::statusChanged, this, &ProcessingController::statusChanged);
    connect(m_worker, &FileXorWorker::fileFinished, this, &ProcessingController::onFileFinished);

    m_thread->start();
    setRunning(true);

    scanAndEnqueue();
    processNext();

    if (m_settings.runMode == AppSettings::RunMode::Timer) {
        m_pollTimer->start(qMax(100, m_settings.pollIntervalMs));
        emitActiveStatus();
    } else if (!m_workerBusy && m_queue.isEmpty()) {
        cleanupWorkerThread();
        setRunning(false);
        emit statusChanged(tr("No matching files found"));
        emit finishedAll();
    }
}

void ProcessingController::pause()
{
    if (!m_running || m_paused)
        return;

    setPaused(true);
    if (m_worker)
        m_worker->pause();
    emit statusChanged(tr("Paused"));
}

void ProcessingController::resume()
{
    if (!m_running || !m_paused)
        return;

    setPaused(false);
    if (m_worker)
        m_worker->resume();

    scanAndEnqueue();
    if (!m_workerBusy)
        processNext();

    emitActiveStatus();
}

void ProcessingController::stop()
{
    if (!m_running)
        return;

    m_stopRequested = true;
    m_pollTimer->stop();
    m_queue.clear();
    setPaused(false);

    if (m_worker)
        m_worker->cancel();

    if (!m_workerBusy) {
        cleanupWorkerThread();
        setRunning(false);
        emit statusChanged(tr("Stopped"));
        emit finishedAll();
    } else {
        emit statusChanged(tr("Stopping…"));
    }
}

bool ProcessingController::requestShutdown(int timeoutMs)
{
    if (!m_running && !m_thread)
        return true;

    m_stopRequested = true;
    m_pollTimer->stop();
    m_queue.clear();
    setPaused(false);

    cleanupWorkerThread(timeoutMs);
    setRunning(false);
    setPaused(false);
    return true;
}

void ProcessingController::onPollTimer()
{
    if (!m_running || m_stopRequested || m_paused)
        return;

    scanAndEnqueue();
    if (!m_workerBusy)
        processNext();

    if (!m_workerBusy && m_queue.isEmpty())
        emitActiveStatus();
}

void ProcessingController::onFileFinished(const QString &inputPath, bool success, const QString &message)
{
    m_workerBusy = false;
    m_inProgress.remove(inputPath);

    if (success) {
        if (m_settings.deleteInputFiles)
            QFile::remove(inputPath);
    } else {
        m_seenInputs.remove(inputPath);
        emit statusChanged(tr("Failed: %1 — %2").arg(inputPath, message));
    }

    if (m_stopRequested) {
        cleanupWorkerThread();
        setRunning(false);
        setPaused(false);
        emit statusChanged(tr("Stopped"));
        emit finishedAll();
        return;
    }

    if (m_paused) {
        emit statusChanged(tr("Paused"));
        return;
    }

    processNext();

    if (!m_workerBusy && m_queue.isEmpty()) {
        if (m_settings.runMode == AppSettings::RunMode::Once) {
            cleanupWorkerThread();
            setRunning(false);
            emit statusChanged(tr("All files processed"));
            emit finishedAll();
        } else {
            emitActiveStatus();
        }
    }
}

void ProcessingController::scanAndEnqueue()
{
    QDir dir(m_settings.inputPath);
    if (!dir.exists()) {
        emit statusChanged(tr("Input path does not exist: %1").arg(m_settings.inputPath));
        return;
    }

    const QStringList masks = expandMasks(m_settings.fileMask);
    const QFileInfoList entries = dir.entryInfoList(masks, QDir::Files | QDir::Readable | QDir::NoSymLinks);

    for (const QFileInfo &info : entries) {
        const QString absolutePath = info.absoluteFilePath();
        if (m_seenInputs.contains(absolutePath))
            continue;
        if (m_inProgress.contains(absolutePath))
            continue;

        m_seenInputs.insert(absolutePath);
        m_queue.enqueue(absolutePath);
    }
}

void ProcessingController::processNext()
{
    if (m_workerBusy || m_paused || m_stopRequested || !m_worker)
        return;

    if (m_queue.isEmpty())
        return;

    const QString inputPath = m_queue.dequeue();
    const QString outputPath = resolveOutputPath(inputPath);

    if (outputPath.isEmpty()) {
        emit statusChanged(tr("Cannot resolve output path for %1").arg(inputPath));
        processNext();
        return;
    }

    QDir().mkpath(QFileInfo(outputPath).absolutePath());

    m_workerBusy = true;
    m_inProgress.insert(inputPath);
    emit statusChanged(tr("Processing: %1").arg(inputPath));

    const bool invoked = QMetaObject::invokeMethod(
        m_worker,
        "processFile",
        Qt::QueuedConnection,
        Q_ARG(QString, inputPath),
        Q_ARG(QString, outputPath),
        Q_ARG(QByteArray, m_settings.xorKey));

    if (!invoked) {
        m_workerBusy = false;
        m_inProgress.remove(inputPath);
        emit statusChanged(tr("Failed to start worker for %1").arg(inputPath));
        processNext();
    }
}

void ProcessingController::emitActiveStatus()
{
    if (!m_running || m_stopRequested)
        return;

    if (m_paused) {
        emit statusChanged(tr("Paused"));
        return;
    }

    if (m_workerBusy && !m_inProgress.isEmpty()) {
        emit statusChanged(tr("Processing: %1").arg(*m_inProgress.constBegin()));
        return;
    }

    if (m_settings.runMode == AppSettings::RunMode::Timer)
        emit statusChanged(tr("Waiting for new files…"));
}

QStringList ProcessingController::expandMasks(const QString &maskText) const
{
    QStringList masks;
    const QStringList parts = maskText.split(QRegularExpression(QStringLiteral("[;,]")), Qt::SkipEmptyParts);
    for (QString part : parts) {
        part = part.trimmed();
        if (part.isEmpty())
            continue;
        if (!part.contains('*') && !part.contains('?') && !part.startsWith('.')) {
            // plain extension like ".txt" or "txt"
            if (part.startsWith('.'))
                masks.append(QStringLiteral("*") + part);
            else if (!part.contains('.'))
                masks.append(QStringLiteral("*.") + part);
            else
                masks.append(part);
        } else if (part.startsWith('.')) {
            masks.append(QStringLiteral("*") + part);
        } else {
            masks.append(part);
        }
    }

    if (masks.isEmpty())
        masks.append(QStringLiteral("*"));

    return masks;
}

QString ProcessingController::resolveOutputPath(const QString &inputFilePath) const
{
    const QFileInfo inputInfo(inputFilePath);
    const QDir outDir(m_settings.outputPath);
    QString candidate = outDir.filePath(inputInfo.fileName());

    if (m_settings.nameConflictPolicy == AppSettings::NameConflictPolicy::Overwrite)
        return candidate;

    if (!QFileInfo::exists(candidate))
        return candidate;

    const QString baseName = inputInfo.completeBaseName();
    const QString suffix = inputInfo.suffix();
    int counter = 1;
    while (true) {
        const QString numbered = suffix.isEmpty()
            ? QStringLiteral("%1_%2").arg(baseName).arg(counter)
            : QStringLiteral("%1_%2.%3").arg(baseName).arg(counter).arg(suffix);
        candidate = outDir.filePath(numbered);
        if (!QFileInfo::exists(candidate))
            return candidate;
        ++counter;
    }
}

void ProcessingController::setRunning(bool running)
{
    if (m_running == running)
        return;
    m_running = running;
    emit runningChanged(m_running);
}

void ProcessingController::setPaused(bool paused)
{
    if (m_paused == paused)
        return;
    m_paused = paused;
    emit pausedChanged(m_paused);
}

void ProcessingController::cleanupWorkerThread(int waitTimeoutMs)
{
    m_pollTimer->stop();

    if (m_worker) {
        disconnect(m_worker, nullptr, this, nullptr);
        m_worker->cancel();
    }

    if (m_thread) {
        if (m_thread->isRunning()) {
            m_thread->quit();
            if (!m_thread->wait(waitTimeoutMs)) {
                m_thread->terminate();
                m_thread->wait(1000);
            }
        }
        m_thread.reset();
    }

    delete m_worker;
    m_worker = nullptr;
    m_workerBusy = false;
}
