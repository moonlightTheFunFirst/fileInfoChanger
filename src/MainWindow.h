#pragma once

#include "FileInfo.h"

#include <QList>
#include <QMainWindow>
#include <QStringList>
#include <QVector>

class QLabel;
class QAction;
class QCheckBox;
class QComboBox;
class QPushButton;
class QTableWidget;

struct EncodingChange
{
    QString fileName;
    QString fullPath;
    QString fromEncoding;
    QString toEncoding;
    QString status;
    bool succeeded = false;
    bool failed = false;
};

enum class PendingOperation
{
    None,
    Encoding,
    Newline
};

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);

protected:
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dropEvent(QDropEvent *event) override;

private:
    void setupUi();
    void setupMenus();
    void openFile();
    void openFolder();
    void loadPath(const QString &path);
    void refreshCurrentPath();
    void applyFilter();
    void applyStructuredFilters();
    void previewEncodingChanges();
    void previewNewlineChanges();
    void commitEncodingChanges();
    void populateLeftPane(const QList<FileInfo> &files);
    void populateRightPane(const QVector<EncodingChange> &changes);
    void updateSelectedPathLabel();
    void loadFilterHistory();
    void saveFilterHistory();
    QStringList currentNameFilters() const;
    QString currentEncodingFilter() const;
    QString currentNewlineFilter() const;
    QVector<EncodingChange> checkedEncodingChanges() const;
    QVector<EncodingChange> checkedNewlineChanges() const;
    bool isSameEncoding(const QString &fromEncoding, const QString &toEncoding) const;
    bool isSameNewline(const QString &fromNewline, const QString &toNewline) const;
    QString configPath() const;
    QString kindText(FileKind kind) const;
    int maxScanDepth() const;

    QLabel *pathLabel = nullptr;
    QCheckBox *includeSubfoldersCheckBox = nullptr;
    QComboBox *filterComboBox = nullptr;
    QComboBox *encodingFilterComboBox = nullptr;
    QComboBox *newlineFilterComboBox = nullptr;
    QComboBox *targetEncodingComboBox = nullptr;
    QComboBox *targetNewlineComboBox = nullptr;
    QPushButton *commitEncodingButton = nullptr;
    QTableWidget *leftTable = nullptr;
    QTableWidget *rightTable = nullptr;
    QAction *showTextAction = nullptr;
    QAction *showBinaryAction = nullptr;
    QString currentPath;
    QVector<EncodingChange> pendingEncodingChanges;
    PendingOperation pendingOperation = PendingOperation::None;
};
