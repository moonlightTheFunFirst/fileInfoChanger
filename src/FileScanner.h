#pragma once

#include "FileInfo.h"

#include <QList>
#include <QString>
#include <QStringList>

#include <atomic>

class FileScanner
{
public:
    QList<FileInfo> scanPath(const QString &path,
                             bool includeText,
                             bool includeBinary,
                             bool includeSubfolders,
                             int maxDepth,
                             const QStringList &nameFilters,
                             std::atomic_bool *cancelled = nullptr) const;

private:
    void scanDirectory(const QString &dirPath,
                       bool includeText,
                       bool includeBinary,
                       bool includeSubfolders,
                       int currentDepth,
                       int maxDepth,
                       const QStringList &nameFilters,
                       QList<FileInfo> &files,
                       std::atomic_bool *cancelled) const;
    FileInfo inspectFile(const QString &filePath, std::atomic_bool *cancelled) const;
    bool shouldInclude(const FileInfo &file,
                       bool includeText,
                       bool includeBinary) const;
    bool matchesNameFilters(const QString &fileName,
                            const QStringList &normalizedNameFilters) const;
};
