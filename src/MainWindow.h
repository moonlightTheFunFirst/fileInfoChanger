#pragma once

#include "FileInfo.h"

#include <QList>
#include <QMainWindow>

class QLabel;
class QAction;
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
    QString kindText(FileKind kind) const;

    QLabel *pathLabel = nullptr;
    QTableWidget *leftTable = nullptr;
    QTableWidget *rightTable = nullptr;
    QAction *showTextAction = nullptr;
    QAction *showBinaryAction = nullptr;
    QString currentPath;
};
