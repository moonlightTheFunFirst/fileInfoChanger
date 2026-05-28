#include "FileScanner.h"

#include "TextInspector.h"

#include <QDirIterator>
#include <QFile>
#include <QFileInfo>

QList<FileInfo> FileScanner::scanPath(const QString &path, bool includeText, bool includeBinary) const
{
    QList<FileInfo> files;
    const QFileInfo input(path);

    if (input.isFile()) {
        const FileInfo file = inspectFile(input.absoluteFilePath());
        if ((file.kind == FileKind::Text && includeText) || (file.kind == FileKind::Binary && includeBinary)) {
            files.append(file);
        }
        return files;
    }

    if (!input.isDir()) {
        return files;
    }

    QDirIterator iterator(input.absoluteFilePath(), QDir::Files | QDir::NoSymLinks, QDirIterator::NoIteratorFlags);
    while (iterator.hasNext()) {
        const FileInfo file = inspectFile(iterator.next());
        if ((file.kind == FileKind::Text && includeText) || (file.kind == FileKind::Binary && includeBinary)) {
            files.append(file);
        }
    }

    return files;
}

FileInfo FileScanner::inspectFile(const QString &filePath) const
{
    const QFileInfo source(filePath);

    FileInfo info;
    info.fileName = source.fileName();
    info.fullPath = source.absoluteFilePath();
    info.createdAt = source.birthTime();
    if (!info.createdAt.isValid()) {
        info.createdAt = source.metadataChangeTime();
    }
    info.size = source.size();

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        info.kind = FileKind::Unknown;
        info.encoding = QStringLiteral("読込不可");
        info.newline = QStringLiteral("-");
        return info;
    }

    const QByteArray data = file.readAll();
    info.kind = TextInspector::detectKind(data);
    info.encoding = TextInspector::detectEncoding(data, info.kind);
    info.newline = TextInspector::detectNewline(data, info.kind);

    return info;
}
