#pragma once

#include "FileInfo.h"

#include <QByteArray>
#include <QString>

class TextInspector
{
public:
    static FileKind detectKind(const QByteArray &data);
    static QString detectEncoding(const QByteArray &data, FileKind kind);
    static QString detectNewline(const QByteArray &data, FileKind kind);

private:
    static bool isValidUtf8(const QByteArray &data);
};
