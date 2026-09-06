#pragma once

#include <QByteArray>
#include <QString>

class TextConverter
{
public:
    struct Result
    {
        bool success = false;
        QString errorMessage;
    };

    static Result convertEncoding(const QString &filePath, const QString &targetEncoding);
    static Result convertNewline(const QString &filePath, const QString &targetNewline);
    static Result convert(const QString &filePath, const QString &targetEncoding, const QString &targetNewline);

private:
    static Result readTextFile(const QString &filePath,
                               QByteArray *originalData,
                               QString *text,
                               QString *detectedEncoding);
    static Result writeConvertedData(const QString &filePath, const QByteArray &convertedData);
    static bool decodeText(const QByteArray &data, QString *text, QString *errorMessage);
    static bool encodeText(const QString &text,
                           const QString &targetEncoding,
                           QByteArray *data,
                           QString *errorMessage);
    static bool decodeCp932(const QByteArray &data, QString *text, QString *errorMessage);
    static bool encodeCp932(const QString &text, QByteArray *data, QString *errorMessage);
};
