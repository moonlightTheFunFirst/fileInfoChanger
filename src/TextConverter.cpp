#include "TextConverter.h"

#include "TextInspector.h"

#include <QFile>
#include <QSaveFile>
#include <QStringDecoder>

#include <windows.h>

TextConverter::Result TextConverter::convertEncoding(const QString &filePath, const QString &targetEncoding)
{
    return convert(filePath, targetEncoding, {});
}

TextConverter::Result TextConverter::convertNewline(const QString &filePath, const QString &targetNewline)
{
    return convert(filePath, {}, targetNewline);
}

TextConverter::Result TextConverter::convert(const QString &filePath, const QString &requestedEncoding,
                                            const QString &targetNewline)
{
    QByteArray originalData;
    QString text;
    QString detectedEncoding;
    const Result readResult = readTextFile(filePath, &originalData, &text, &detectedEncoding);
    if (!readResult.success) {
        return readResult;
    }

    if (!targetNewline.isEmpty()) {
        text.replace(QStringLiteral("\r\n"), QStringLiteral("\n"));
        text.replace(QChar('\r'), QChar('\n'));
        if (targetNewline == QStringLiteral("CRLF")) {
            text.replace(QStringLiteral("\n"), QStringLiteral("\r\n"));
        } else if (targetNewline != QStringLiteral("LF")) {
            return {false, QStringLiteral("未対応の変更先改行コード")};
        }
    }

    QString targetEncoding = requestedEncoding.isEmpty() ? detectedEncoding : requestedEncoding;
    const bool preserveBigEndian = requestedEncoding.isEmpty() && originalData.startsWith("\xFE\xFF");
    originalData.clear();
    if (targetEncoding == QStringLiteral("UTF-8/ASCII")) {
        targetEncoding = QStringLiteral("UTF-8 BOMなし");
    }

    QString errorMessage;
    QByteArray convertedData;
    if (!encodeText(text, targetEncoding, &convertedData, &errorMessage)) {
        return {false, errorMessage};
    }

    if (preserveBigEndian) {
        for (qsizetype i = 0; i + 1 < convertedData.size(); i += 2) {
            const char first = convertedData.at(i);
            convertedData[i] = convertedData.at(i + 1);
            convertedData[i + 1] = first;
        }
    }

    return writeConvertedData(filePath, convertedData);
}

TextConverter::Result TextConverter::readTextFile(const QString &filePath,
                                                  QByteArray *originalData,
                                                  QString *text,
                                                  QString *detectedEncoding)
{
    QFile input(filePath);
    if (!input.open(QIODevice::ReadOnly)) {
        return {false, QStringLiteral("読み込み不可")};
    }

    *originalData = input.readAll();
    if (input.error() != QFileDevice::NoError) {
        return {false, input.errorString()};
    }
    input.close();

    const FileKind kind = TextInspector::detectKind(*originalData);
    if (kind != FileKind::Text) {
        return {false, QStringLiteral("テキストファイルではありません")};
    }

    QString errorMessage;
    if (!decodeText(*originalData, text, &errorMessage)) {
        return {false, errorMessage};
    }

    *detectedEncoding = TextInspector::detectEncoding(*originalData, kind);
    return {true, QString()};
}

TextConverter::Result TextConverter::writeConvertedData(const QString &filePath, const QByteArray &convertedData)
{
    QSaveFile output(filePath);
    if (!output.open(QIODevice::WriteOnly)) {
        return {false, QStringLiteral("書き込み不可: %1").arg(output.errorString())};
    }
    if (output.write(convertedData) != convertedData.size()) {
        return {false, QStringLiteral("書き込み失敗: %1").arg(output.errorString())};
    }
    if (!output.commit()) {
        return {false, QStringLiteral("置換失敗: %1").arg(output.errorString())};
    }

    return {true, QString()};
}

bool TextConverter::decodeText(const QByteArray &data, QString *text, QString *errorMessage)
{
    const QString encoding = TextInspector::detectEncoding(data, TextInspector::detectKind(data));

    if (encoding == QStringLiteral("UTF-8 BOMあり")) {
        QStringDecoder decoder(QStringDecoder::Utf8);
        *text = decoder.decode(data.mid(3));
        if (decoder.hasError()) {
            *errorMessage = QStringLiteral("UTF-8デコード失敗");
            return false;
        }
        return true;
    }

    if (encoding == QStringLiteral("UTF-8 BOMなし") || encoding == QStringLiteral("UTF-8/ASCII")) {
        QStringDecoder decoder(QStringDecoder::Utf8);
        *text = decoder.decode(data);
        if (decoder.hasError()) {
            *errorMessage = QStringLiteral("UTF-8デコード失敗");
            return false;
        }
        return true;
    }

    if (encoding == QStringLiteral("UTF-16")) {
        QStringDecoder decoder(QStringDecoder::Utf16);
        *text = decoder.decode(data);
        if (decoder.hasError()) {
            *errorMessage = QStringLiteral("UTF-16デコード失敗");
            return false;
        }
        return true;
    }

    return decodeCp932(data, text, errorMessage);
}

bool TextConverter::encodeText(const QString &text,
                               const QString &targetEncoding,
                               QByteArray *data,
                               QString *errorMessage)
{
    if (targetEncoding == QStringLiteral("UTF-8 BOMなし")) {
        *data = text.toUtf8();
        return true;
    }

    if (targetEncoding == QStringLiteral("UTF-8 BOMあり")) {
        *data = QByteArray("\xEF\xBB\xBF", 3) + text.toUtf8();
        return true;
    }

    if (targetEncoding == QStringLiteral("UTF-16")) {
        data->clear();
        data->append("\xFF\xFE", 2);
        data->append(reinterpret_cast<const char *>(text.utf16()), text.size() * static_cast<int>(sizeof(char16_t)));
        return true;
    }

    if (targetEncoding == QStringLiteral("SJIS/CP932")) {
        return encodeCp932(text, data, errorMessage);
    }

    *errorMessage = QStringLiteral("未対応の変換先エンコード");
    return false;
}

bool TextConverter::decodeCp932(const QByteArray &data, QString *text, QString *errorMessage)
{
    const int required = MultiByteToWideChar(932, MB_ERR_INVALID_CHARS, data.constData(), data.size(), nullptr, 0);
    if (required <= 0) {
        *errorMessage = QStringLiteral("CP932デコード失敗");
        return false;
    }

    QString decoded;
    decoded.resize(required);
    const int converted = MultiByteToWideChar(932,
                                             MB_ERR_INVALID_CHARS,
                                             data.constData(),
                                             data.size(),
                                             reinterpret_cast<LPWSTR>(decoded.data()),
                                             decoded.size());
    if (converted <= 0) {
        *errorMessage = QStringLiteral("CP932デコード失敗");
        return false;
    }

    *text = decoded;
    return true;
}

bool TextConverter::encodeCp932(const QString &text, QByteArray *data, QString *errorMessage)
{
    if (text.isEmpty()) {
        data->clear();
        return true;
    }
    BOOL usedDefaultChar = FALSE;
    const int required = WideCharToMultiByte(932,
                                            WC_NO_BEST_FIT_CHARS,
                                            reinterpret_cast<LPCWCH>(text.utf16()),
                                            text.size(),
                                            nullptr,
                                            0,
                                            nullptr,
                                            &usedDefaultChar);
    if (required <= 0 || usedDefaultChar) {
        *errorMessage = QStringLiteral("CP932エンコード失敗");
        return false;
    }

    data->resize(required);
    usedDefaultChar = FALSE;
    const int converted = WideCharToMultiByte(932,
                                             WC_NO_BEST_FIT_CHARS,
                                             reinterpret_cast<LPCWCH>(text.utf16()),
                                             text.size(),
                                             data->data(),
                                             data->size(),
                                             nullptr,
                                             &usedDefaultChar);
    if (converted <= 0 || usedDefaultChar) {
        *errorMessage = QStringLiteral("CP932エンコード失敗");
        return false;
    }

    return true;
}
