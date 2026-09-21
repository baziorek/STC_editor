#pragma once

#include <QString>
#include <QDateTime>
#include <QFileInfo>

class BackupManager
{
public:
    BackupManager();

    // Get backup file path for a given file
    static QString getBackupFilePath(const QString& originalFilePath);

    // Check if backup exists for a file
    static bool backupExists(const QString& originalFilePath);

    // Create backup of current content
    static bool createBackup(const QString& originalFilePath, const QString& content);

    // Load backup content
    static QString loadBackup(const QString& originalFilePath);

    // Delete backup file
    static bool deleteBackup(const QString& originalFilePath);

    // Get backup file modification time
    static QDateTime getBackupModificationTime(const QString& originalFilePath);

    // Get original file modification time
    static QDateTime getOriginalFileModificationTime(const QString& originalFilePath);

    // Check if backup is newer than original file
    static bool isBackupNewer(const QString& originalFilePath);

    // Clean up old backups (can be called periodically)
    static void cleanupOldBackups();
};