#pragma once

#include "FileInfo.h"

#include <QList>
#include <QMainWindow>

class QLabel;
class QAction;
class QCheckBox;
class QTableWidget;

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
    void populateLeftPane(const QList<FileInfo> &files);
    void updateSelectedPathLabel();
    QString kindText(FileKind kind) const;
    int maxScanDepth() const;

    QLabel *pathLabel = nullptr;
    QCheckBox *includeSubfoldersCheckBox = nullptr;
    QTableWidget *leftTable = nullptr;
    QTableWidget *rightTable = nullptr;
    QAction *showTextAction = nullptr;
    QAction *showBinaryAction = nullptr;
    QString currentPath;
};
