#pragma once

#include "FileInfo.h"

#include <QByteArray>
#include <QString>
#include <atomic>

class QIODevice;

class TextInspector
{
public:
    static FileKind detectKind(const QByteArray &data);
    static QString detectEncoding(const QByteArray &data, FileKind kind);
    static QString detectEncoding(QIODevice &device, FileKind kind, const std::atomic_bool *cancelled = nullptr);
    static QString detectNewline(const QByteArray &data, FileKind kind);
    static QString detectNewline(QIODevice &device, FileKind kind, const std::atomic_bool *cancelled = nullptr);

private:
    static bool isValidUtf8(const QByteArray &data);
};
