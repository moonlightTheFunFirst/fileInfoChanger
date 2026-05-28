#include "TextInspector.h"

FileKind TextInspector::detectKind(const QByteArray &data)
{
    if (data.isEmpty()) {
        return FileKind::Text;
    }

    if (data.startsWith("\xEF\xBB\xBF") || data.startsWith("\xFF\xFE") || data.startsWith("\xFE\xFF")) {
        return FileKind::Text;
    }

    const int sampleSize = qMin(data.size(), 8192);
    int controlCount = 0;

    for (int i = 0; i < sampleSize; ++i) {
        const unsigned char c = static_cast<unsigned char>(data.at(i));
        if (c == 0) {
            return FileKind::Binary;
        }
        if (c < 0x20 && c != '\t' && c != '\r' && c != '\n' && c != '\f') {
            ++controlCount;
        }
    }

    if (controlCount > sampleSize / 20) {
        return FileKind::Binary;
    }

    return FileKind::Text;
}

QString TextInspector::detectEncoding(const QByteArray &data, FileKind kind)
{
    if (kind == FileKind::Binary) {
        return QStringLiteral("-");
    }

    if (data.startsWith("\xEF\xBB\xBF")) {
        return QStringLiteral("UTF-8 BOMあり");
    }
    if (data.startsWith("\xFF\xFE") || data.startsWith("\xFE\xFF")) {
        return QStringLiteral("UTF-16");
    }

    bool asciiOnly = true;
    for (const char byte : data) {
        if (static_cast<unsigned char>(byte) >= 0x80) {
            asciiOnly = false;
            break;
        }
    }

    if (asciiOnly) {
        return QStringLiteral("UTF-8/ASCII");
    }

    if (isValidUtf8(data)) {
        return QStringLiteral("UTF-8 BOMなし");
    }

    return QStringLiteral("SJIS/CP932");
}

QString TextInspector::detectNewline(const QByteArray &data, FileKind kind)
{
    if (kind == FileKind::Binary) {
        return QStringLiteral("-");
    }

    int crlfCount = 0;
    int lfCount = 0;
    int crCount = 0;

    for (int i = 0; i < data.size(); ++i) {
        const char c = data.at(i);
        if (c == '\r') {
            if (i + 1 < data.size() && data.at(i + 1) == '\n') {
                ++crlfCount;
                ++i;
            } else {
                ++crCount;
            }
        } else if (c == '\n') {
            ++lfCount;
        }
    }

    const int kinds = (crlfCount > 0 ? 1 : 0) + (lfCount > 0 ? 1 : 0) + (crCount > 0 ? 1 : 0);
    if (kinds == 0) {
        return QStringLiteral("なし");
    }
    if (kinds > 1) {
        return QStringLiteral("混在");
    }
    if (crlfCount > 0) {
        return QStringLiteral("CRLF");
    }
    if (lfCount > 0) {
        return QStringLiteral("LF");
    }
    return QStringLiteral("CR");
}

bool TextInspector::isValidUtf8(const QByteArray &data)
{
    int i = 0;
    while (i < data.size()) {
        const unsigned char c = static_cast<unsigned char>(data.at(i));
        if (c <= 0x7F) {
            ++i;
            continue;
        }

        int length = 0;
        unsigned int codePoint = 0;
        if (c >= 0xC2 && c <= 0xDF) {
            length = 2;
            codePoint = c & 0x1F;
        } else if (c >= 0xE0 && c <= 0xEF) {
            length = 3;
            codePoint = c & 0x0F;
        } else if (c >= 0xF0 && c <= 0xF4) {
            length = 4;
            codePoint = c & 0x07;
        } else {
            return false;
        }

        if (i + length > data.size()) {
            return false;
        }

        for (int j = 1; j < length; ++j) {
            const unsigned char continuation = static_cast<unsigned char>(data.at(i + j));
            if ((continuation & 0xC0) != 0x80) {
                return false;
            }
            codePoint = (codePoint << 6) | (continuation & 0x3F);
        }

        if ((length == 3 && codePoint < 0x800) || (length == 4 && codePoint < 0x10000)) {
            return false;
        }
        if (codePoint >= 0xD800 && codePoint <= 0xDFFF) {
            return false;
        }
        if (codePoint > 0x10FFFF) {
            return false;
        }

        i += length;
    }

    return true;
}
