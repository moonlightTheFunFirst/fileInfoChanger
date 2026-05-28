#pragma once

#include "FileInfo.h"

#include <QList>
#include <QString>

class FileScanner
{
public:
    QList<FileInfo> scanPath(const QString &path, bool includeText, bool includeBinary) const;

private:
    FileInfo inspectFile(const QString &filePath) const;
};
