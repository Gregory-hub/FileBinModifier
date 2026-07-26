#include "FileXorWorker.h"

#include <QFile>
#include <QThread>

namespace
{
constexpr qint64 kChunkSize = 4 * 1024 * 1024; // 4 MiB
}

FileXorWorker::FileXorWorker(QObject *parent)
    : QObject(parent)
{
}

void FileXorWorker::pause()
{
    m_paused.storeRelaxed(1);
}

void FileXorWorker::resume()
{
    m_paused.storeRelaxed(0);
}

void FileXorWorker::cancel()
{
    m_cancelled.storeRelaxed(1);
    m_paused.storeRelaxed(0);
}

bool FileXorWorker::waitWhilePaused()
{
    while (m_paused.loadRelaxed() != 0) {
        if (m_cancelled.loadRelaxed() != 0)
            return false;
        QThread::msleep(50);
    }
    return m_cancelled.loadRelaxed() == 0;
}

void FileXorWorker::applyXor(QByteArray &chunk, const QByteArray &key, qint64 absoluteOffset)
{
    const int keySize = key.size();
    if (keySize <= 0)
        return;

    char *data = chunk.data();
    const char *keyData = key.constData();
    const int size = chunk.size();
    for (int i = 0; i < size; ++i) {
        const int keyIndex = static_cast<int>((absoluteOffset + i) % keySize);
        data[i] = static_cast<char>(static_cast<unsigned char>(data[i])
                                    ^ static_cast<unsigned char>(keyData[keyIndex]));
    }
}

void FileXorWorker::processFile(const QString &inputPath, const QString &outputPath, const QByteArray &xorKey)
{
    m_cancelled.storeRelaxed(0);
    m_paused.storeRelaxed(0);

    if (xorKey.size() != 8) {
        emit fileFinished(inputPath, false, tr("XOR key must be exactly 8 bytes"));
        return;
    }

    QFile inputFile(inputPath);
    if (!inputFile.open(QIODevice::ReadOnly)) {
        emit fileFinished(inputPath, false, tr("Failed to open input: %1").arg(inputFile.errorString()));
        return;
    }

    QFile outputFile(outputPath);
    if (!outputFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        emit fileFinished(inputPath, false, tr("Failed to open output: %1").arg(outputFile.errorString()));
        return;
    }

    const qint64 totalBytes = inputFile.size();
    qint64 bytesDone = 0;

    emit statusChanged(tr("Processing: %1").arg(inputPath));
    emit progress(0, totalBytes);

    bool cancelled = false;
    bool success = true;
    QString message;

    while (bytesDone < totalBytes) {
        if (!waitWhilePaused()) {
            cancelled = true;
            break;
        }
        if (m_cancelled.loadRelaxed() != 0) {
            cancelled = true;
            break;
        }

        const qint64 toRead = qMin(kChunkSize, totalBytes - bytesDone);
        QByteArray chunk = inputFile.read(toRead);
        if (chunk.size() != toRead) {
            success = false;
            message = tr("Read error: %1").arg(inputFile.errorString());
            break;
        }

        applyXor(chunk, xorKey, bytesDone);

        if (outputFile.write(chunk) != chunk.size()) {
            success = false;
            message = tr("Write error: %1").arg(outputFile.errorString());
            break;
        }

        bytesDone += chunk.size();
        emit progress(bytesDone, totalBytes);
    }

    inputFile.close();
    outputFile.close();

    if (cancelled) {
        QFile::remove(outputPath);
        emit fileFinished(inputPath, false, tr("Cancelled"));
        return;
    }

    if (!success) {
        QFile::remove(outputPath);
        emit fileFinished(inputPath, false, message);
        return;
    }

    emit progress(totalBytes, totalBytes);
    emit fileFinished(inputPath, true, tr("Done"));
}
