#pragma once

#include "FileInfo.h"

#include <QList>
#include <QString>
#include <QStringList>

class FileScanner
{
public:
    QList<FileInfo> scanPath(const QString &path,
                             bool includeText,
                             bool includeBinary,
                             bool includeSubfolders,
                             int maxDepth,
                             const QStringList &nameFilters,
                             const QString &encodingFilter,
                             const QString &newlineFilter) const;

private:
    void scanDirectory(const QString &dirPath,
                       bool includeText,
                       bool includeBinary,
                       bool includeSubfolders,
                       int currentDepth,
                       int maxDepth,
                       const QStringList &nameFilters,
                       const QString &encodingFilter,
                       const QString &newlineFilter,
                       QList<FileInfo> &files) const;
    FileInfo inspectFile(const QString &filePath) const;
    bool shouldInclude(const FileInfo &file,
                       bool includeText,
                       bool includeBinary,
                       const QStringList &nameFilters,
                       const QString &encodingFilter,
                       const QString &newlineFilter) const;
};
