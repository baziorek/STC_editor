#include "BackupManager.h"
#include <QFile>
#include <QDir>
#include <QFileInfo>
#include <QDateTime>
#include <QDebug>

BackupManager::BackupManager()
{
}

QString BackupManager::getBackupFilePath(const QString& originalFilePath)
{
    if (originalFilePath.isEmpty())
        return QString();

    QFileInfo fileInfo(originalFilePath);
    QString backupDir = fileInfo.absolutePath() + "/.stc_backups";
    
    // Create backup directory if it doesn't exist
    QDir dir;
    if (!dir.exists(backupDir))
    {
        dir.mkpath(backupDir);
    }

    // Use original filename with .backup extension
    return backupDir + "/" + fileInfo.fileName() + ".backup";
}

bool BackupManager::backupExists(const QString& originalFilePath)
{
    QString backupPath = getBackupFilePath(originalFilePath);
    return QFileInfo::exists(backupPath);
}

bool BackupManager::createBackup(const QString& originalFilePath, const QString& content)
{
    if (originalFilePath.isEmpty())
        return false;

    QString backupPath = getBackupFilePath(originalFilePath);
    
    QFile backupFile(backupPath);
    if (!backupFile.open(QIODevice::WriteOnly | QIODevice::Text))
    {
        qWarning() << "Failed to create backup file:" << backupPath;
        return false;
    }

    backupFile.write(content.toUtf8());
    backupFile.close();
    
    qDebug() << "Backup created:" << backupPath;
    return true;
}

QString BackupManager::loadBackup(const QString& originalFilePath)
{
    QString backupPath = getBackupFilePath(originalFilePath);
    
    QFile backupFile(backupPath);
    if (!backupFile.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        qWarning() << "Failed to load backup file:" << backupPath;
        return QString();
    }

    QString content = QString::fromUtf8(backupFile.readAll());
    backupFile.close();
    
    return content;
}

bool BackupManager::deleteBackup(const QString& originalFilePath)
{
    QString backupPath = getBackupFilePath(originalFilePath);
    
    if (QFileInfo::exists(backupPath))
    {
        QFile backupFile(backupPath);
        if (backupFile.remove())
        {
            qDebug() << "Backup deleted:" << backupPath;
            return true;
        }
        else
        {
            qWarning() << "Failed to delete backup file:" << backupPath;
            return false;
        }
    }
    
    return true; // No backup to delete is considered success
}

QDateTime BackupManager::getBackupModificationTime(const QString& originalFilePath)
{
    QString backupPath = getBackupFilePath(originalFilePath);
    QFileInfo backupInfo(backupPath);
    
    if (backupInfo.exists())
        return backupInfo.lastModified();
    
    return QDateTime();
}

QDateTime BackupManager::getOriginalFileModificationTime(const QString& originalFilePath)
{
    QFileInfo originalInfo(originalFilePath);
    
    if (originalInfo.exists())
        return originalInfo.lastModified();
    
    return QDateTime();
}

bool BackupManager::isBackupNewer(const QString& originalFilePath)
{
    QDateTime backupTime = getBackupModificationTime(originalFilePath);
    QDateTime originalTime = getOriginalFileModificationTime(originalFilePath);
    
    if (!backupTime.isValid() || !originalTime.isValid())
        return false;
    
    return backupTime > originalTime;
}

void BackupManager::cleanupOldBackups()
{
    QString backupDir = ".stc_backups";
    QDir dir(QDir::currentPath() + "/" + backupDir);
    
    if (!dir.exists())
        return;
    
    // Remove backups older than 7 days
    QDateTime cutoff = QDateTime::currentDateTime().addDays(-7);
    
    QStringList filters;
    filters << "*.backup";
    dir.setNameFilters(filters);
    
    QFileInfoList backupFiles = dir.entryInfoList(QDir::Files);
    for (const QFileInfo& fileInfo : backupFiles)
    {
        if (fileInfo.lastModified() < cutoff)
        {
            QFile::remove(fileInfo.absoluteFilePath());
            qDebug() << "Cleaned up old backup:" << fileInfo.absoluteFilePath();
        }
    }
}