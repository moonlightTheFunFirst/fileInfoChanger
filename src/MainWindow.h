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
class QPoint;
class QPushButton;
class QTableWidget;

struct RenameQueueItem
{
    QString fileName;
    QString fullPath;
    QString newFileName;
    QString status;
};

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
    void populateRenameQueuePane();
    void addCheckedFilesToRenameQueue();
    void removeSelectedRenameQueueItems();
    void clearRenameQueueItems();
    void moveSelectedRenameQueueItems(int direction);
    void showRenameQueueContextMenu(const QPoint &position);
    void openRenameDialog();
    bool applyRenameTemplate(const QString &renameTemplate, bool preserveExtension, const QString &replacementExtension);
    QString buildRenameName(const QString &renameTemplate, int index, QString *errorMessage) const;
    QString buildRenameBaseName(const QString &sourcePath,
                                const QString &renameTemplate,
                                int index,
                                QString *errorMessage) const;
    QString buildFinalRenameName(const QString &sourcePath,
                                 const QString &baseName,
                                 bool preserveExtension,
                                 const QString &replacementExtension) const;
    QString alphabetSequence(int index, int minimumWidth) const;
    QString numberSequence(int index, int minimumWidth) const;
    bool validateRenameTargets(QStringList *errors) const;
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
    QPushButton *clearRenameQueueButton = nullptr;
    QPushButton *addToRenameQueueButton = nullptr;
    QTableWidget *leftTable = nullptr;
    QTableWidget *rightTable = nullptr;
    QAction *showTextAction = nullptr;
    QAction *showBinaryAction = nullptr;
    QAction *standardViewAction = nullptr;
    QAction *detailViewAction = nullptr;
    QString currentPath;
    QList<FileInfo> currentFiles;
    QVector<EncodingChange> pendingEncodingChanges;
    QVector<RenameQueueItem> renameQueueItems;
};
