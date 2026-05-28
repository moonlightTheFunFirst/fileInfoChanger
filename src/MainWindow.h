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

enum class PendingOperation
{
    None,
    Encoding,
    Newline
};

struct EncodingChange
{
    QString fileName;
    QString fullPath;
    QString fromEncoding;
    QString toEncoding;
    QString status;
    PendingOperation operation = PendingOperation::None;
    bool succeeded = false;
    bool failed = false;
};

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);

protected:
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dropEvent(QDropEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void setupUi();
    void setupMenus();
    void openFile();
    void openFolder();
    void loadPath(const QString &path);
    void refreshCurrentPath();
    void applyFilter();
    void applyStructuredFilters();
    void updateChangePreview();
    void commitEncodingChanges();
    void populateLeftPane(const QList<FileInfo> &files);
    void populateRightPane(const QVector<EncodingChange> &changes);
    void updateStructuredFilterOptions(const QList<FileInfo> &files);
    void applyCurrentDisplayFilters();
    void applyViewMode();
    void saveViewMode();
    void updateSelectedPathLabel();
    void loadFilterHistory();
    void saveFilterHistory();
    QStringList currentNameFilters() const;
    QString currentEncodingFilter() const;
    QString currentNewlineFilter() const;
    QList<FileInfo> filteredCurrentFiles() const;
    QVector<EncodingChange> checkedEncodingChanges() const;
    QVector<EncodingChange> checkedNewlineChanges() const;
    QVector<EncodingChange> checkedRequestedChanges() const;
    bool isSameEncoding(const QString &fromEncoding, const QString &toEncoding) const;
    bool isSameNewline(const QString &fromNewline, const QString &toNewline) const;
    QString configPath() const;
    QString kindText(FileKind kind) const;
    int maxScanDepth() const;

    QCheckBox *includeSubfoldersCheckBox = nullptr;
    QComboBox *filterComboBox = nullptr;
    QComboBox *encodingFilterComboBox = nullptr;
    QComboBox *newlineFilterComboBox = nullptr;
    QComboBox *targetEncodingComboBox = nullptr;
    QComboBox *targetNewlineComboBox = nullptr;
    QCheckBox *changeEncodingCheckBox = nullptr;
    QCheckBox *changeNewlineCheckBox = nullptr;
    QPushButton *commitEncodingButton = nullptr;
    QTableWidget *leftTable = nullptr;
    QTableWidget *rightTable = nullptr;
    QAction *showTextAction = nullptr;
    QAction *showBinaryAction = nullptr;
    QAction *standardViewAction = nullptr;
    QAction *detailViewAction = nullptr;
    QString currentPath;
    QList<FileInfo> currentFiles;
    QVector<EncodingChange> pendingEncodingChanges;
};
