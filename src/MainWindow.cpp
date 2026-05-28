#include "MainWindow.h"

#include "FileScanner.h"

#include <QAction>
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QCoreApplication>
#include <QDir>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMenuBar>
#include <QMimeData>
#include <QPushButton>
#include <QSettings>
#include <QSplitter>
#include <QStatusBar>
#include <QTableWidget>
#include <QUrl>
#include <QVBoxLayout>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setupUi();
    setupMenus();
    setAcceptDrops(true);
    resize(1180, 720);
}

void MainWindow::dragEnterEvent(QDragEnterEvent *event)
{
    if (event->mimeData()->hasUrls()) {
        event->acceptProposedAction();
    }
}

void MainWindow::dropEvent(QDropEvent *event)
{
    const QList<QUrl> urls = event->mimeData()->urls();
    if (urls.isEmpty()) {
        return;
    }

    const QString path = urls.first().toLocalFile();
    if (!path.isEmpty()) {
        loadPath(path);
        event->acceptProposedAction();
    }
}

void MainWindow::setupUi()
{
    auto *central = new QWidget(this);
    auto *rootLayout = new QVBoxLayout(central);

    auto *pathLayout = new QHBoxLayout();
    pathLabel = new QLabel(tr("ファイルまたはフォルダを開くか、ここへドラッグ＆ドロップしてください。"), central);
    pathLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    pathLayout->addWidget(pathLabel, 1);

    includeSubfoldersCheckBox = new QCheckBox(tr("サブフォルダも対象にする"), central);
    connect(includeSubfoldersCheckBox, &QCheckBox::toggled, this, &MainWindow::refreshCurrentPath);
    pathLayout->addWidget(includeSubfoldersCheckBox);

    rootLayout->addLayout(pathLayout);

    auto *filterLayout = new QHBoxLayout();
    filterLayout->addWidget(new QLabel(tr("フィルタ:"), central));

    filterComboBox = new QComboBox(central);
    filterComboBox->setEditable(true);
    filterComboBox->setInsertPolicy(QComboBox::NoInsert);
    filterComboBox->setMinimumWidth(320);
    filterComboBox->lineEdit()->setPlaceholderText(tr("例: *.h, *.c, *.cpp"));
    filterLayout->addWidget(filterComboBox, 1);

    auto *applyFilterButton = new QPushButton(tr("適用"), central);
    connect(applyFilterButton, &QPushButton::clicked, this, &MainWindow::applyFilter);
    connect(filterComboBox->lineEdit(), &QLineEdit::returnPressed, this, &MainWindow::applyFilter);
    connect(filterComboBox, &QComboBox::textActivated, this, &MainWindow::applyFilter);
    filterLayout->addWidget(applyFilterButton);

    rootLayout->addLayout(filterLayout);

    auto *splitter = new QSplitter(Qt::Horizontal, central);

    leftTable = new QTableWidget(splitter);
    leftTable->setColumnCount(7);
    leftTable->setHorizontalHeaderLabels({
        tr("選択"),
        tr("ファイル名"),
        tr("作成日時"),
        tr("サイズ"),
        tr("種別"),
        tr("エンコード"),
        tr("改行コード"),
    });
    leftTable->horizontalHeader()->setStretchLastSection(false);
    leftTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    leftTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    leftTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    leftTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    connect(leftTable, &QTableWidget::currentCellChanged, this, &MainWindow::updateSelectedPathLabel);
    connect(leftTable, &QTableWidget::cellClicked, this, &MainWindow::updateSelectedPathLabel);

    auto *arrowLabel = new QLabel(tr("→"), splitter);
    arrowLabel->setAlignment(Qt::AlignCenter);
    arrowLabel->setMinimumWidth(24);
    arrowLabel->setMaximumWidth(36);

    rightTable = new QTableWidget(splitter);
    rightTable->setColumnCount(4);
    rightTable->setHorizontalHeaderLabels({tr("ファイル名"), tr("変換後エンコード"), tr("変換後改行コード"), tr("状態")});
    rightTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    rightTable->setEditTriggers(QAbstractItemView::NoEditTriggers);

    splitter->addWidget(leftTable);
    splitter->addWidget(arrowLabel);
    splitter->addWidget(rightTable);
    splitter->setStretchFactor(0, 3);
    splitter->setStretchFactor(1, 0);
    splitter->setStretchFactor(2, 2);
    rootLayout->addWidget(splitter, 1);

    setCentralWidget(central);
    loadFilterHistory();
    statusBar()->showMessage(tr("準備完了"));
}

void MainWindow::setupMenus()
{
    auto *fileMenu = menuBar()->addMenu(tr("ファイル"));
    auto *openFileAction = fileMenu->addAction(tr("ファイルを開く..."));
    connect(openFileAction, &QAction::triggered, this, &MainWindow::openFile);

    auto *openFolderAction = fileMenu->addAction(tr("フォルダを開く..."));
    connect(openFolderAction, &QAction::triggered, this, &MainWindow::openFolder);

    auto *refreshAction = fileMenu->addAction(tr("再読み込み"));
    connect(refreshAction, &QAction::triggered, this, &MainWindow::refreshCurrentPath);

    fileMenu->addSeparator();
    auto *exitAction = fileMenu->addAction(tr("終了"));
    connect(exitAction, &QAction::triggered, qApp, &QApplication::quit);

    auto *viewMenu = menuBar()->addMenu(tr("表示"));
    showTextAction = viewMenu->addAction(tr("テキストファイル"));
    showTextAction->setCheckable(true);
    showTextAction->setChecked(true);
    connect(showTextAction, &QAction::toggled, this, &MainWindow::refreshCurrentPath);

    showBinaryAction = viewMenu->addAction(tr("バイナリファイル"));
    showBinaryAction->setCheckable(true);
    showBinaryAction->setChecked(false);
    connect(showBinaryAction, &QAction::toggled, this, &MainWindow::refreshCurrentPath);
}

void MainWindow::openFile()
{
    const QString path = QFileDialog::getOpenFileName(this, tr("ファイルを開く"));
    if (!path.isEmpty()) {
        loadPath(path);
    }
}

void MainWindow::openFolder()
{
    const QString path = QFileDialog::getExistingDirectory(this, tr("フォルダを開く"));
    if (!path.isEmpty()) {
        loadPath(path);
    }
}

void MainWindow::loadPath(const QString &path)
{
    const QFileInfo target(path);
    if (!target.exists()) {
        statusBar()->showMessage(tr("対象が見つかりません: %1").arg(path));
        return;
    }

    currentPath = target.absoluteFilePath();
    pathLabel->setText(tr("対象: %1").arg(currentPath));

    FileScanner scanner;
    const int depth = maxScanDepth();
    const bool includeSubfolders = includeSubfoldersCheckBox->isChecked();
    const QList<FileInfo> files = scanner.scanPath(currentPath,
                                                   showTextAction->isChecked(),
                                                   showBinaryAction->isChecked(),
                                                   includeSubfolders,
                                                   depth,
                                                   currentNameFilters());
    populateLeftPane(files);
    updateSelectedPathLabel();
    const QString filterText = filterComboBox->currentText().trimmed();
    if (includeSubfolders && target.isDir()) {
        if (filterText.isEmpty()) {
            statusBar()->showMessage(tr("%1 件を表示（サブフォルダ深さ %2 まで）").arg(files.size()).arg(depth));
        } else {
            statusBar()->showMessage(tr("%1 件を表示（フィルタ: %2 / サブフォルダ深さ %3 まで）")
                                         .arg(files.size())
                                         .arg(filterText)
                                         .arg(depth));
        }
    } else {
        if (filterText.isEmpty()) {
            statusBar()->showMessage(tr("%1 件を表示").arg(files.size()));
        } else {
            statusBar()->showMessage(tr("%1 件を表示（フィルタ: %2）").arg(files.size()).arg(filterText));
        }
    }
}

void MainWindow::refreshCurrentPath()
{
    if (!currentPath.isEmpty()) {
        loadPath(currentPath);
    }
}

void MainWindow::applyFilter()
{
    const QString filterText = filterComboBox->currentText().trimmed();
    if (!filterText.isEmpty() && filterComboBox->findText(filterText, Qt::MatchFixedString) < 0) {
        filterComboBox->insertItem(0, filterText);
        filterComboBox->setCurrentIndex(0);
    }

    while (filterComboBox->count() > 20) {
        filterComboBox->removeItem(filterComboBox->count() - 1);
    }

    saveFilterHistory();
    refreshCurrentPath();
}

void MainWindow::populateLeftPane(const QList<FileInfo> &files)
{
    leftTable->setRowCount(files.size());

    for (int row = 0; row < files.size(); ++row) {
        const FileInfo &file = files.at(row);

        auto *check = new QCheckBox(leftTable);
        check->setChecked(file.kind == FileKind::Text);
        auto *checkContainer = new QWidget(leftTable);
        auto *checkLayout = new QHBoxLayout(checkContainer);
        checkLayout->setContentsMargins(0, 0, 0, 0);
        checkLayout->setAlignment(Qt::AlignCenter);
        checkLayout->addWidget(check);
        leftTable->setCellWidget(row, 0, checkContainer);

        auto *nameItem = new QTableWidgetItem(file.fileName);
        nameItem->setToolTip(file.fullPath);
        leftTable->setItem(row, 1, nameItem);
        leftTable->setItem(row, 2, new QTableWidgetItem(file.createdAt.toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"))));
        leftTable->setItem(row, 3, new QTableWidgetItem(QString::number(file.size)));
        leftTable->setItem(row, 4, new QTableWidgetItem(kindText(file.kind)));
        leftTable->setItem(row, 5, new QTableWidgetItem(file.encoding));
        leftTable->setItem(row, 6, new QTableWidgetItem(file.newline));
    }

    leftTable->resizeColumnsToContents();
    leftTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
}

void MainWindow::updateSelectedPathLabel()
{
    const int row = leftTable->currentRow();
    const QTableWidgetItem *nameItem = row >= 0 ? leftTable->item(row, 1) : nullptr;
    const QString selectedPath = nameItem ? nameItem->toolTip() : QString();

    if (!selectedPath.isEmpty()) {
        pathLabel->setText(tr("対象: %1").arg(selectedPath));
        return;
    }

    if (!currentPath.isEmpty()) {
        pathLabel->setText(tr("対象: %1").arg(currentPath));
        return;
    }

    pathLabel->setText(tr("ファイルまたはフォルダを開くか、ここへドラッグ＆ドロップしてください。"));
}

void MainWindow::loadFilterHistory()
{
    QSettings settings(configPath(), QSettings::IniFormat);
    const QStringList filters = settings.value(QStringLiteral("Filter/history")).toStringList();
    for (const QString &filter : filters) {
        if (!filter.trimmed().isEmpty()) {
            filterComboBox->addItem(filter.trimmed());
        }
    }
    filterComboBox->setCurrentText(QString());
}

void MainWindow::saveFilterHistory()
{
    QStringList filters;
    for (int i = 0; i < filterComboBox->count(); ++i) {
        const QString filter = filterComboBox->itemText(i).trimmed();
        if (!filter.isEmpty() && !filters.contains(filter, Qt::CaseInsensitive)) {
            filters.append(filter);
        }
    }

    QSettings settings(configPath(), QSettings::IniFormat);
    settings.setValue(QStringLiteral("Filter/history"), filters);
}

QStringList MainWindow::currentNameFilters() const
{
    const QString filterText = filterComboBox->currentText();
    QStringList filters;
    for (const QString &part : filterText.split(QLatin1Char(','), Qt::SkipEmptyParts)) {
        const QString filter = part.trimmed();
        if (!filter.isEmpty()) {
            filters.append(filter);
        }
    }
    return filters;
}

QString MainWindow::configPath() const
{
    const QString appConfigPath = QCoreApplication::applicationDirPath() + QStringLiteral("/fileInfoChanger.ini");
    const QString cwdConfigPath = QDir::currentPath() + QStringLiteral("/fileInfoChanger.ini");
    return QFileInfo::exists(appConfigPath) ? appConfigPath : cwdConfigPath;
}

QString MainWindow::kindText(FileKind kind) const
{
    switch (kind) {
    case FileKind::Text:
        return tr("テキスト");
    case FileKind::Binary:
        return tr("バイナリ");
    case FileKind::Unknown:
        return tr("不明");
    }
    return tr("不明");
}

int MainWindow::maxScanDepth() const
{
    QSettings settings(configPath(), QSettings::IniFormat);
    const int depth = settings.value(QStringLiteral("Scan/maxDepth"), 5).toInt();
    return qBound(0, depth, 100);
}
