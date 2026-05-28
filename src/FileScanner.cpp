#include "FileScanner.h"

#include "TextInspector.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFileInfoList>

QList<FileInfo> FileScanner::scanPath(const QString &path,
                                      bool includeText,
                                      bool includeBinary,
                                      bool includeSubfolders,
                                      int maxDepth) const
{
    QList<FileInfo> files;
    const QFileInfo input(path);

    if (input.isFile()) {
        const FileInfo file = inspectFile(input.absoluteFilePath());
        if (shouldInclude(file, includeText, includeBinary)) {
            files.append(file);
        }
        return files;
    }

    if (!input.isDir()) {
        return files;
    }

    scanDirectory(input.absoluteFilePath(), includeText, includeBinary, includeSubfolders, 0, maxDepth, files);
    return files;
}

void FileScanner::scanDirectory(const QString &dirPath,
                                bool includeText,
                                bool includeBinary,
                                bool includeSubfolders,
                                int currentDepth,
                                int maxDepth,
                                QList<FileInfo> &files) const
{
    const QDir dir(dirPath);
    const QFileInfoList fileEntries = dir.entryInfoList(QDir::Files | QDir::NoSymLinks, QDir::Name | QDir::IgnoreCase);
    for (const QFileInfo &entry : fileEntries) {
        const FileInfo file = inspectFile(entry.absoluteFilePath());
        if (shouldInclude(file, includeText, includeBinary)) {
            files.append(file);
        }
    }

    if (!includeSubfolders || currentDepth >= maxDepth) {
        return;
    }

    const QFileInfoList dirEntries = dir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot | QDir::NoSymLinks,
                                                       QDir::Name | QDir::IgnoreCase);
    for (const QFileInfo &entry : dirEntries) {
        scanDirectory(entry.absoluteFilePath(),
                      includeText,
                      includeBinary,
                      includeSubfolders,
                      currentDepth + 1,
                      maxDepth,
                      files);
    }
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

bool FileScanner::shouldInclude(const FileInfo &file, bool includeText, bool includeBinary) const
{
    return (file.kind == FileKind::Text && includeText) || (file.kind == FileKind::Binary && includeBinary);
}
