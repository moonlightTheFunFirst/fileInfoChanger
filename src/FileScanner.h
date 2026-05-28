#pragma once

#include "FileInfo.h"

#include <QList>
#include <QString>

class FileScanner
{
public:
    QList<FileInfo> scanPath(const QString &path,
                             bool includeText,
                             bool includeBinary,
                             bool includeSubfolders,
                             int maxDepth) const;

private:
    void scanDirectory(const QString &dirPath,
                       bool includeText,
                       bool includeBinary,
                       bool includeSubfolders,
                       int currentDepth,
                       int maxDepth,
                       QList<FileInfo> &files) const;
    FileInfo inspectFile(const QString &filePath) const;
    bool shouldInclude(const FileInfo &file, bool includeText, bool includeBinary) const;
};
