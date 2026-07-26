#pragma once

#include <QAtomicInt>
#include <QByteArray>
#include <QObject>
#include <QString>

/**
 * Performs chunked XOR of a single file on a worker thread.
 * Usage: move to QThread, call processFile() via queued invoke; use pause/resume/cancel.
 */
class FileXorWorker : public QObject
{
    Q_OBJECT

public:
    explicit FileXorWorker(QObject *parent = nullptr);

public slots:
    void processFile(const QString &inputPath, const QString &outputPath, const QByteArray &xorKey);
    void pause();
    void resume();
    void cancel();

signals:
    void progress(qint64 bytesDone, qint64 totalBytes);
    void statusChanged(const QString &status);
    void fileFinished(const QString &inputPath, bool success, const QString &message);

private:
    bool waitWhilePaused();
    static void applyXor(QByteArray &chunk, const QByteArray &key, qint64 absoluteOffset);

    QAtomicInt m_paused{0};
    QAtomicInt m_cancelled{0};
};
