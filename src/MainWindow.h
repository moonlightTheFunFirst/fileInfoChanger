#pragma once

#include "FileInfo.h"

#include <QList>
#include <QMainWindow>
#include <QStringList>

class QLabel;
class QAction;
class QCheckBox;
class QComboBox;
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
    void applyFilter();
    void populateLeftPane(const QList<FileInfo> &files);
    void updateSelectedPathLabel();
    void loadFilterHistory();
    void saveFilterHistory();
    QStringList currentNameFilters() const;
    QString configPath() const;
    QString kindText(FileKind kind) const;
    int maxScanDepth() const;

    QLabel *pathLabel = nullptr;
    QCheckBox *includeSubfoldersCheckBox = nullptr;
    QComboBox *filterComboBox = nullptr;
    QTableWidget *leftTable = nullptr;
    QTableWidget *rightTable = nullptr;
    QAction *showTextAction = nullptr;
    QAction *showBinaryAction = nullptr;
    QString currentPath;
};
