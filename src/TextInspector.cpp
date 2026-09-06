#include "TextInspector.h"

#include <QIODevice>

namespace
{
class Utf8Validator
{
public:
    void consume(const QByteArray &data)
    {
        for (const char byte : data) {
            consume(static_cast<unsigned char>(byte));
            if (!valid) {
                return;
            }
        }
    }

    bool isAsciiOnly() const
    {
        return asciiOnly;
    }

    bool isValid() const
    {
        return valid && remaining == 0;
    }

    bool hasError() const { return !valid; }

private:
    void consume(unsigned char byte)
    {
        if (remaining == 0) {
            if (byte <= 0x7F) {
                return;
            }

            asciiOnly = false;
            if (byte >= 0xC2 && byte <= 0xDF) {
                remaining = 1;
                codePoint = byte & 0x1F;
                minimumCodePoint = 0x80;
            } else if (byte >= 0xE0 && byte <= 0xEF) {
                remaining = 2;
                codePoint = byte & 0x0F;
                minimumCodePoint = 0x800;
            } else if (byte >= 0xF0 && byte <= 0xF4) {
                remaining = 3;
                codePoint = byte & 0x07;
                minimumCodePoint = 0x10000;
            } else {
                valid = false;
            }
            return;
        }

        if ((byte & 0xC0) != 0x80) {
            valid = false;
            return;
        }

        codePoint = (codePoint << 6) | (byte & 0x3F);
        --remaining;
        if (remaining == 0
            && (codePoint < minimumCodePoint
                || (codePoint >= 0xD800 && codePoint <= 0xDFFF)
                || codePoint > 0x10FFFF)) {
            valid = false;
        }
    }

    bool valid = true;
    bool asciiOnly = true;
    int remaining = 0;
    unsigned int codePoint = 0;
    unsigned int minimumCodePoint = 0;
};

QString newlineFromCounts(int crlfCount, int lfCount, int crCount)
{
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

void countNewlineCodeUnit(int codeUnit,
                          int &crlfCount,
                          int &lfCount,
                          int &crCount,
                          bool &pendingCarriageReturn)
{
    if (pendingCarriageReturn) {
        if (codeUnit == '\n') {
            ++crlfCount;
            pendingCarriageReturn = false;
            return;
        }
        ++crCount;
        pendingCarriageReturn = false;
    }

    if (codeUnit == '\r') {
        pendingCarriageReturn = true;
    } else if (codeUnit == '\n') {
        ++lfCount;
    }
}
}

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

    Utf8Validator validator;
    validator.consume(data);
    if (validator.isAsciiOnly()) {
        return QStringLiteral("UTF-8/ASCII");
    }

    if (validator.isValid()) {
        return QStringLiteral("UTF-8 BOMなし");
    }

    return QStringLiteral("SJIS/CP932");
}

QString TextInspector::detectEncoding(QIODevice &device, FileKind kind, const std::atomic_bool *cancelled)
{
    if (kind == FileKind::Binary) {
        return QStringLiteral("-");
    }

    const qint64 originalPosition = device.pos();
    if (!device.seek(0)) {
        return QStringLiteral("読込不可");
    }

    const QByteArray prefix = device.read(3);
    if (prefix.startsWith("\xEF\xBB\xBF")) {
        device.seek(originalPosition);
        return QStringLiteral("UTF-8 BOMあり");
    }
    if (prefix.startsWith("\xFF\xFE") || prefix.startsWith("\xFE\xFF")) {
        device.seek(originalPosition);
        return QStringLiteral("UTF-16");
    }

    if (!device.seek(0)) {
        return QStringLiteral("読込不可");
    }

    Utf8Validator validator;
    while (!device.atEnd()) {
        if (cancelled && cancelled->load()) {
            device.seek(originalPosition);
            return {};
        }
        const QByteArray chunk = device.read(64 * 1024);
        if (chunk.isEmpty()) {
            break;
        }
        validator.consume(chunk);
        if (validator.hasError()) {
            break;
        }
    }

    device.seek(originalPosition);
    if (validator.isAsciiOnly()) {
        return QStringLiteral("UTF-8/ASCII");
    }
    return validator.isValid() ? QStringLiteral("UTF-8 BOMなし") : QStringLiteral("SJIS/CP932");
}

QString TextInspector::detectNewline(const QByteArray &data, FileKind kind)
{
    if (kind == FileKind::Binary) {
        return QStringLiteral("-");
    }

    if (data.startsWith("\xFF\xFE") || data.startsWith("\xFE\xFF")) {
        const bool littleEndian = data.startsWith("\xFF\xFE");
        int crlfCount = 0;
        int lfCount = 0;
        int crCount = 0;
        bool pendingCarriageReturn = false;
        for (int i = 2; i + 1 < data.size(); i += 2) {
            const unsigned char first = static_cast<unsigned char>(data.at(i));
            const unsigned char second = static_cast<unsigned char>(data.at(i + 1));
            const int codeUnit = littleEndian
                ? first | (second << 8)
                : (first << 8) | second;
            countNewlineCodeUnit(codeUnit,
                                 crlfCount,
                                 lfCount,
                                 crCount,
                                 pendingCarriageReturn);
        }
        if (pendingCarriageReturn) {
            ++crCount;
        }
        return newlineFromCounts(crlfCount, lfCount, crCount);
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

    return newlineFromCounts(crlfCount, lfCount, crCount);
}

QString TextInspector::detectNewline(QIODevice &device, FileKind kind, const std::atomic_bool *cancelled)
{
    if (kind == FileKind::Binary) {
        return QStringLiteral("-");
    }

    const qint64 originalPosition = device.pos();
    if (!device.seek(0)) {
        return QStringLiteral("読込不可");
    }

    const QByteArray prefix = device.read(2);
    if (prefix.startsWith("\xFF\xFE") || prefix.startsWith("\xFE\xFF")) {
        const bool littleEndian = prefix.startsWith("\xFF\xFE");
        int crlfCount = 0;
        int lfCount = 0;
        int crCount = 0;
        bool pendingCarriageReturn = false;
        QByteArray carry;

        while (!device.atEnd()) {
            if (cancelled && cancelled->load()) {
                device.seek(originalPosition);
                return {};
            }
            QByteArray chunk = carry;
            const QByteArray next = device.read(64 * 1024);
            if (next.isEmpty()) {
                break;
            }
            chunk += next;
            const int usableSize = chunk.size() - (chunk.size() % 2);
            for (int i = 0; i < usableSize; i += 2) {
                const unsigned char first = static_cast<unsigned char>(chunk.at(i));
                const unsigned char second = static_cast<unsigned char>(chunk.at(i + 1));
                const int codeUnit = littleEndian
                    ? first | (second << 8)
                    : (first << 8) | second;
                countNewlineCodeUnit(codeUnit,
                                         crlfCount,
                                         lfCount,
                                         crCount,
                                         pendingCarriageReturn);
            }
            carry = chunk.mid(usableSize);
        }

        if (pendingCarriageReturn) {
            ++crCount;
        }
        device.seek(originalPosition);
        return newlineFromCounts(crlfCount, lfCount, crCount);
    }

    if (!device.seek(0)) {
        return QStringLiteral("読込不可");
    }

    int crlfCount = 0;
    int lfCount = 0;
    int crCount = 0;
    bool pendingCarriageReturn = false;

    while (!device.atEnd()) {
        if (cancelled && cancelled->load()) {
            device.seek(originalPosition);
            return {};
        }
        const QByteArray chunk = device.read(64 * 1024);
        if (chunk.isEmpty()) {
            break;
        }

        for (const char byte : chunk) {
            const char current = byte;
            countNewlineCodeUnit(static_cast<unsigned char>(current),
                                 crlfCount,
                                 lfCount,
                                 crCount,
                                 pendingCarriageReturn);
        }
    }

    if (pendingCarriageReturn) {
        ++crCount;
    }
    device.seek(originalPosition);
    return newlineFromCounts(crlfCount, lfCount, crCount);
}

bool TextInspector::isValidUtf8(const QByteArray &data)
{
    Utf8Validator validator;
    validator.consume(data);
    return validator.isValid();
}
