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

private:
    static bool decodeText(const QByteArray &data, QString *text, QString *errorMessage);
    static bool encodeText(const QString &text,
                           const QString &targetEncoding,
                           QByteArray *data,
                           QString *errorMessage);
    static bool decodeCp932(const QByteArray &data, QString *text, QString *errorMessage);
    static bool encodeCp932(const QString &text, QByteArray *data, QString *errorMessage);
};
