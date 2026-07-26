#pragma once

#include <QByteArray>
#include <QString>

/**
 * Holds user-configured processing options from the main form.
 * Usage: fill from MainWindow controls and pass to ProcessingController::start().
 */
struct AppSettings
{
    enum class NameConflictPolicy
    {
        Overwrite,
        CounterSuffix
    };

    enum class RunMode
    {
        Once,
        Timer
    };

    QString fileMask;
    bool deleteInputFiles = false;
    QString outputPath;
    QString inputPath;
    NameConflictPolicy nameConflictPolicy = NameConflictPolicy::Overwrite;
    RunMode runMode = RunMode::Once;
    int pollIntervalMs = 1000;
    QByteArray xorKey; // exactly 8 bytes
};
