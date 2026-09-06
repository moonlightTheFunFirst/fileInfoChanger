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
                                      int maxDepth,
                                      const QStringList &nameFilters,
                                      std::atomic_bool *cancelled) const
{
    QList<FileInfo> files;
    if (cancelled && cancelled->load()) {
        return files;
    }
    const QFileInfo input(path);

    QStringList normalizedNameFilters;
    normalizedNameFilters.reserve(nameFilters.size());
    for (const QString &filter : nameFilters) {
        normalizedNameFilters.append(filter.toLower());
    }

    if (input.isFile()) {
        if (cancelled && cancelled->load()) {
            return files;
        }
        if (!matchesNameFilters(input.fileName(), normalizedNameFilters)) {
            return files;
        }
        const FileInfo file = inspectFile(input.absoluteFilePath(), cancelled);
        if (shouldInclude(file, includeText, includeBinary)) {
            files.append(file);
        }
        return files;
    }

    if (!input.isDir()) {
        return files;
    }

    scanDirectory(input.absoluteFilePath(),
                  includeText,
                  includeBinary,
                  includeSubfolders,
                  0,
                  maxDepth,
                  normalizedNameFilters,
                  files,
                  cancelled);
    return files;
}

void FileScanner::scanDirectory(const QString &dirPath,
                                bool includeText,
                                bool includeBinary,
                                bool includeSubfolders,
                                int currentDepth,
                                int maxDepth,
                                const QStringList &nameFilters,
                                QList<FileInfo> &files,
                                std::atomic_bool *cancelled) const
{
    if (cancelled && cancelled->load()) {
        return;
    }

    const QDir dir(dirPath);
    const QFileInfoList fileEntries = dir.entryInfoList(QDir::Files | QDir::Hidden | QDir::NoSymLinks,
                                                        QDir::Name | QDir::IgnoreCase);
    for (const QFileInfo &entry : fileEntries) {
        if (cancelled && cancelled->load()) {
            return;
        }
        if (!matchesNameFilters(entry.fileName(), nameFilters)) {
            continue;
        }
        const FileInfo file = inspectFile(entry.absoluteFilePath(), cancelled);
        if (shouldInclude(file, includeText, includeBinary)) {
            files.append(file);
        }
    }

    if (!includeSubfolders || currentDepth >= maxDepth) {
        return;
    }

    const QFileInfoList dirEntries = dir.entryInfoList(QDir::Dirs | QDir::Hidden | QDir::NoDotAndDotDot | QDir::NoSymLinks,
                                                       QDir::Name | QDir::IgnoreCase);
    for (const QFileInfo &entry : dirEntries) {
        if (cancelled && cancelled->load()) {
            return;
        }
        scanDirectory(entry.absoluteFilePath(),
                      includeText,
                      includeBinary,
                      includeSubfolders,
                      currentDepth + 1,
                      maxDepth,
                      nameFilters,
                      files,
                      cancelled);
    }
}

FileInfo FileScanner::inspectFile(const QString &filePath, std::atomic_bool *cancelled) const
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

    const QByteArray sample = file.read(64 * 1024);
    info.kind = TextInspector::detectKind(sample);
    info.encoding = TextInspector::detectEncoding(file, info.kind, cancelled);
    info.newline = TextInspector::detectNewline(file, info.kind, cancelled);
    if (cancelled && cancelled->load()) {
        info.kind = FileKind::Unknown;
    }

    return info;
}

bool FileScanner::shouldInclude(const FileInfo &file,
                                bool includeText,
                                bool includeBinary) const
{
    const bool kindMatches = (file.kind == FileKind::Text && includeText) || (file.kind == FileKind::Binary && includeBinary);
    return kindMatches;
}

bool FileScanner::matchesNameFilters(const QString &fileName,
                                     const QStringList &normalizedNameFilters) const
{
    return normalizedNameFilters.isEmpty()
        || QDir::match(normalizedNameFilters, fileName.toLower());
}
