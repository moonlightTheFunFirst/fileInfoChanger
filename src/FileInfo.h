#pragma once

#include <QDateTime>
#include <QString>

enum class FileKind
{
    Text,
    Binary,
    Unknown
};

struct FileInfo
{
    QString fileName;
    QString fullPath;
    QDateTime createdAt;
    qint64 size = 0;
    FileKind kind = FileKind::Unknown;
    QString encoding;
    QString newline;
};
