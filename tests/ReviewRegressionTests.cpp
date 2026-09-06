#include "MainWindow.h"
#include "TextInspector.h"
#include "TextConverter.h"
#include "FileScanner.h"

#include <QtTest>
#include <QBuffer>
#include <QCheckBox>
#include <QComboBox>
#include <QDir>
#include <QFile>
#include <QMessageBox>
#include <QPushButton>
#include <QTableWidget>
#include <QTemporaryDir>
#include <QTimer>

class CancellingBuffer : public QBuffer
{
public:
    CancellingBuffer(QByteArray *data, std::atomic_bool &flag) : QBuffer(data), cancelled(flag) {}
    qint64 bytesRead = 0;
protected:
    qint64 readData(char *data, qint64 maxSize) override
    {
        const qint64 count = QBuffer::readData(data, maxSize);
        bytesRead += qMax(qint64(0), count);
        if (bytesRead >= 65536) cancelled.store(true);
        return count;
    }
private:
    std::atomic_bool &cancelled;
};

class ReviewRegressionTests : public QObject
{
    Q_OBJECT
    QTemporaryDir workspace;
    QString originalDirectory;

    static bool writeFile(const QString &path, const QByteArray &data)
    {
        QFile file(path);
        return file.open(QIODevice::WriteOnly) && file.write(data) == data.size();
    }

    static QByteArray readFile(const QString &path)
    {
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly)) return {};
        return file.readAll();
    }

private slots:
    void initTestCase()
    {
        QVERIFY(workspace.isValid());
        originalDirectory = QDir::currentPath();
        QVERIFY(QDir::setCurrent(workspace.path()));
    }

    void cleanupTestCase() { QDir::setCurrent(originalDirectory); }

    void utf8ChunkBoundaries()
    {
        for (const auto &character : {QByteArray::fromHex("c2a2"), QByteArray::fromHex("e38182"),
                                     QByteArray::fromHex("f09f9880")}) {
            for (int offset = 1; offset < character.size(); ++offset) {
                QByteArray data(65536 - offset, 'x');
                data += character + QByteArray(65536, 'y');
                QBuffer buffer(&data);
                QVERIFY(buffer.open(QIODevice::ReadOnly));
                buffer.seek(7);
                QCOMPARE(TextInspector::detectEncoding(buffer, FileKind::Text),
                         QStringLiteral("UTF-8 BOMなし"));
                QCOMPARE(buffer.pos(), 7);
            }
        }
        QByteArray incomplete = QByteArray(65535, 'x') + QByteArray::fromHex("e3");
        QBuffer buffer(&incomplete);
        QVERIFY(buffer.open(QIODevice::ReadOnly));
        QCOMPARE(TextInspector::detectEncoding(buffer, FileKind::Text), QStringLiteral("SJIS/CP932"));
    }

    void newlineAndAtomicConversion()
    {
        QTemporaryDir dir;
        const QString path = dir.filePath("sample.txt");
        for (const bool be : {false, true}) {
            const QByteArray before = QByteArray::fromHex(be ? "feff0041000d000a0042" : "fffe41000d000a004200");
            const QByteArray after = QByteArray::fromHex(be ? "feff0041000a0042" : "fffe41000a004200");
            QVERIFY(writeFile(path, before));
            QVERIFY(TextConverter::convertNewline(path, "LF").success);
            QCOMPARE(readFile(path), after);
        }
        const QByteArray original = QByteArray::fromHex("f09f9880") + "\r\n";
        QVERIFY(writeFile(path, original));
        QVERIFY(!TextConverter::convert(path, QStringLiteral("SJIS/CP932"), "LF").success);
        QCOMPARE(readFile(path), original);
        QVERIFY(TextConverter::convert(path, QStringLiteral("UTF-8 BOMあり"), "LF").success);
        QCOMPARE(readFile(path), QByteArray::fromHex("efbbbff09f9880") + "\n");
    }

    void alphabetNamesAreUnique()
    {
        MainWindow window;
        QCOMPARE(window.alphabetSequence(26, 1), QString("AA"));
        QCOMPARE(window.alphabetSequence(26, 2), QString("BA"));
        QCOMPARE(window.alphabetSequence(26, 3), QString("ABA"));
        for (int width : {1, 2, 3}) {
            QSet<QString> names;
            for (int i = 0; i < 20000; ++i) {
                const QString name = window.alphabetSequence(i, width);
                QVERIFY(!names.contains(name));
                QVERIFY(name.size() >= width);
                names.insert(name);
            }
        }
    }

    void cancelsWithinFileAndOnDestruction()
    {
        for (int mode : {0, 1, 2}) {
            QByteArray data(1024 * 1024, 'x');
            if (mode == 2) data.prepend(QByteArray::fromHex("fffe"));
            std::atomic_bool cancelled{false};
            CancellingBuffer buffer(&data, cancelled);
            QVERIFY(buffer.open(QIODevice::ReadOnly));
            const QString result = mode == 0
                ? TextInspector::detectEncoding(buffer, FileKind::Text, &cancelled)
                : TextInspector::detectNewline(buffer, FileKind::Text, &cancelled);
            QVERIFY(cancelled.load());
            QVERIFY(result.isEmpty());
            QVERIFY(buffer.bytesRead < 3 * 65536);
        }
        QTemporaryDir dir;
        QVERIFY(writeFile(dir.filePath("one.txt"), "one\n"));
        std::shared_ptr<std::atomic_bool> cancellation;
        {
            MainWindow window;
            window.loadPath(dir.path());
            cancellation = window.activeScanCancellation;
            QVERIFY(cancellation);
        }
        QVERIFY(cancellation->load());
    }

    void latestDirectoryWins()
    {
        QTemporaryDir first;
        QTemporaryDir second;
        QVERIFY(writeFile(first.filePath("old.txt"), "old\n"));
        QVERIFY(writeFile(second.filePath("new.txt"), "new\n"));
        MainWindow window;
        window.loadPath(first.path());
        const auto cancelled = window.activeScanCancellation;
        window.loadPath(second.path());
        QVERIFY(cancelled->load());
        QTRY_VERIFY_WITH_TIMEOUT(!window.activeScanCancellation, 5000);
        QCOMPARE(window.currentFiles.size(), 1);
        QCOMPARE(window.currentFiles.first().fileName, QString("new.txt"));
        QCOMPARE(window.leftTable->item(0, 1)->text(), QString("new.txt"));
    }

    void selectionAndToggleRefresh()
    {
        QTemporaryDir dir;
        QVERIFY(writeFile(dir.filePath("one.txt"), "one\r\n"));
        QVERIFY(QDir(dir.path()).mkdir("sub"));
        QVERIFY(writeFile(dir.filePath("sub/two.txt"), "two\r\n"));
        MainWindow window;
        window.loadPath(dir.path());
        // Even a fast queued scan must block execution until its result is applied.
        QVERIFY(!window.commitEncodingButton->isEnabled());
        window.commitEncodingChanges();
        QTRY_VERIFY_WITH_TIMEOUT(!window.activeScanCancellation, 5000);
        QCOMPARE(window.leftTable->rowCount(), 1);
        auto *check = window.leftTable->cellWidget(0, 0)->findChild<QCheckBox *>();
        check->setChecked(false);
        window.targetNewlineComboBox->setCurrentText("LF");
        window.changeNewlineCheckBox->setChecked(true);
        window.includeSubfoldersCheckBox->setChecked(true);
        QTRY_VERIFY_WITH_TIMEOUT(!window.activeScanCancellation, 5000);
        QCOMPARE(window.leftTable->rowCount(), 2);
        QCOMPARE(window.pendingEncodingChanges.size(), 1);
        QCOMPARE(window.pendingEncodingChanges.first().fileName, QString("two.txt"));
        QVERIFY(!window.checkedPaths.value(dir.filePath("one.txt")));
        window.refreshCurrentPath();
        QTRY_VERIFY_WITH_TIMEOUT(!window.activeScanCancellation, 5000);
        QCOMPARE(window.pendingEncodingChanges.size(), 1);
    }

    void caseOnlyRenameAndConfirmationGuard()
    {
        QTemporaryDir dir;
        const QString path = dir.filePath("sample.txt");
        const QString target = dir.filePath("Sample.txt");
        QVERIFY(writeFile(path, "one\r\n"));
        MainWindow window;
        window.loadPath(path);
        QTRY_VERIFY_WITH_TIMEOUT(!window.activeScanCancellation, 5000);
        window.renameQueueItems.append({"sample.txt", path, "Sample.txt", QStringLiteral("変更予定")});
        window.targetNewlineComboBox->setCurrentText("LF");
        window.changeNewlineCheckBox->setChecked(true);
        QTimer responder;
        bool sawQuestion = false;
        connect(&responder, &QTimer::timeout, &window, [&]() {
            for (QWidget *widget : QApplication::topLevelWidgets()) {
                auto *box = qobject_cast<QMessageBox *>(widget);
                if (!box || !box->isVisible()) continue;
                if (box->standardButtons().testFlag(QMessageBox::Yes)) {
                    sawQuestion = true;
                    window.loadPath(dir.path());
                    QCOMPARE(window.currentPath, path);
                    QVERIFY(window.operationInProgress);
                    box->button(QMessageBox::Yes)->click();
                } else {
                    box->button(QMessageBox::Ok)->click();
                }
            }
        });
        responder.start(10);
        window.commitEncodingChanges();
        responder.stop();
        QVERIFY(sawQuestion);
        QTRY_VERIFY_WITH_TIMEOUT(!window.activeScanCancellation, 5000);
        QCOMPARE(window.renameQueueItems.first().status, QStringLiteral("成功"));
        QCOMPARE(window.currentPath, target);
        QCOMPARE(QDir(dir.path()).entryList(QDir::Files), QStringList{"Sample.txt"});
        QCOMPARE(readFile(target), QByteArray("one\n"));
        QCOMPARE(window.leftTable->rowCount(), 1);
    }
};

QTEST_MAIN(ReviewRegressionTests)
#include "ReviewRegressionTests.moc"
