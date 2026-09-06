#include "MainWindow.h"

#include "FileScanner.h"
#include "TextConverter.h"

#include <QAction>
#include <QActionGroup>
#include <QApplication>
#include <QBrush>
#include <QCheckBox>
#include <QCloseEvent>
#include <QProgressDialog>
#include <QEventLoop>
#include <QScopedValueRollback>
#include <windows.h>
#include <QColor>
#include <QComboBox>
#include <QCoreApplication>
#include <QDir>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QGroupBox>
#include <QHash>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QItemSelectionModel>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QMimeData>
#include <QPushButton>
#include <QRegularExpression>
#include <QFutureWatcher>
#include <QSignalBlocker>
#include <QtGlobal>
#include <QSettings>
#include <QSize>
#include <QSizePolicy>
#include <QSplitter>
#include <QStatusBar>
#include <QTableWidget>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>
#include <QtConcurrent/QtConcurrentRun>

#include <algorithm>
#include <functional>

namespace
{
int findUnescapedMarker(const QString &text, QChar marker)
{
    for (int i = 0; i < text.size(); ++i) {
        if (text.at(i) != marker) {
            continue;
        }
        if (i + 1 < text.size() && text.at(i + 1) == marker) {
            ++i;
            continue;
        }
        return i;
    }
    return -1;
}
}

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setupUi();
    setupMenus();
    loadViewMode();
    setAcceptDrops(true);
    resize(1180, 720);
}

MainWindow::~MainWindow()
{
    if (activeScanCancellation) {
        activeScanCancellation->store(true);
    }
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    if (operationInProgress) {
        event->ignore();
        return;
    }
    if (activeScanCancellation) {
        activeScanCancellation->store(true);
    }
    QMainWindow::closeEvent(event);
}

void MainWindow::dragEnterEvent(QDragEnterEvent *event)
{
    if (event->mimeData()->hasUrls()) {
        event->setDropAction(Qt::CopyAction);
        event->accept();
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
        event->setDropAction(Qt::CopyAction);
        event->accept();
        QTimer::singleShot(0, this, [this, path]() {
            loadPath(path, true);
        });
    }
}

bool MainWindow::eventFilter(QObject *watched, QEvent *event)
{
    const bool isDropTarget = (leftTable && (watched == leftTable || watched == leftTable->viewport()))
        || (rightTable && (watched == rightTable || watched == rightTable->viewport()))
        || (addToRenameQueueButton && watched == addToRenameQueueButton);

    if (isDropTarget && (event->type() == QEvent::DragEnter || event->type() == QEvent::DragMove)) {
        auto *dragEvent = static_cast<QDragMoveEvent *>(event);
        if (dragEvent->mimeData()->hasUrls()) {
            dragEvent->setDropAction(Qt::CopyAction);
            dragEvent->accept();
            return true;
        }
    }

    if (isDropTarget && event->type() == QEvent::Drop) {
        auto *dropEvent = static_cast<QDropEvent *>(event);
        const QList<QUrl> urls = dropEvent->mimeData()->urls();
        if (!urls.isEmpty()) {
            const QString path = urls.first().toLocalFile();
            if (!path.isEmpty()) {
                dropEvent->setDropAction(Qt::CopyAction);
                dropEvent->accept();
                QTimer::singleShot(0, this, [this, path]() {
                    loadPath(path, true);
                });
                return true;
            }
        }
    }

    if (watched == filterComboBox->lineEdit() && event->type() == QEvent::KeyPress) {
        auto *keyEvent = static_cast<QKeyEvent *>(event);
        if (keyEvent->key() == Qt::Key_Escape) {
            filterComboBox->setCurrentText(QString());
            saveFilterHistory();
            refreshCurrentPath();
            return true;
        }
    }

    if (watched == leftTable && event->type() == QEvent::KeyPress) {
        auto *keyEvent = static_cast<QKeyEvent *>(event);
        if (keyEvent->key() == Qt::Key_Space) {
            QList<int> rows;
            for (const QModelIndex &index : leftTable->selectionModel()->selectedRows()) {
                rows.append(index.row());
            }
            if (rows.isEmpty() && leftTable->currentRow() >= 0) {
                rows.append(leftTable->currentRow());
            }

            bool shouldCheck = false;
            for (int row : rows) {
                const QWidget *checkContainer = leftTable->cellWidget(row, 0);
                const QCheckBox *checkBox = checkContainer ? checkContainer->findChild<QCheckBox *>() : nullptr;
                if (checkBox && !checkBox->isChecked()) {
                    shouldCheck = true;
                    break;
                }
            }

            for (int row : rows) {
                QWidget *checkContainer = leftTable->cellWidget(row, 0);
                QCheckBox *checkBox = checkContainer ? checkContainer->findChild<QCheckBox *>() : nullptr;
                if (checkBox) {
                    const QSignalBlocker blocker(checkBox);
                    checkBox->setChecked(shouldCheck);
                    const auto *name = leftTable->item(row, 1);
                    if (name) {
                        checkedPaths.insert(name->data(Qt::UserRole).toString(), shouldCheck);
                    }
                }
            }
            updateChangePreview();
            return true;
        }
    }

    if (watched == rightTable && event->type() == QEvent::KeyPress) {
        auto *keyEvent = static_cast<QKeyEvent *>(event);
        if (keyEvent->key() == Qt::Key_Delete) {
            removeSelectedRenameQueueItems();
            return true;
        }
        if (keyEvent->key() == Qt::Key_Space && rightTable->currentRow() >= 0) {
            const int row = rightTable->currentRow();
            const bool selected = rightTable->selectionModel()->isRowSelected(row, QModelIndex());
            const QItemSelectionModel::SelectionFlags flags = selected
                ? QItemSelectionModel::Deselect | QItemSelectionModel::Rows
                : QItemSelectionModel::Select | QItemSelectionModel::Rows;
            rightTable->selectionModel()->select(rightTable->model()->index(row, 0), flags);
            return true;
        }
    }

    return QMainWindow::eventFilter(watched, event);
}

void MainWindow::setupUi()
{
    setStyleSheet(QStringLiteral(
        "QCheckBox::indicator {"
        "  width: 11px;"
        "  height: 11px;"
        "  border: 1px solid #777777;"
        "  background-color: #2b2b2b;"
        "}"
        "QCheckBox::indicator:checked {"
        "  border: 1px solid #9a9a9a;"
        "  background-color: #8a8a8a;"
        "  image: url(:/icons/checkmark.xpm);"
        "}"
        "QCheckBox::indicator:checked:hover {"
        "  background-color: #9a9a9a;"
        "}"
        "QCheckBox::indicator:unchecked:hover {"
        "  border-color: #9a9a9a;"
        "}"
        "QCheckBox::indicator:disabled {"
        "  border-color: #555555;"
        "  background-color: #3a3a3a;"
        "}"
    ));

    auto *central = new QWidget(this);
    auto *rootLayout = new QVBoxLayout(central);
    rootLayout->setContentsMargins(6, 4, 6, 4);
    rootLayout->setSpacing(2);

    auto *pathLayout = new QHBoxLayout();
    pathLayout->setContentsMargins(0, 0, 0, 0);
    pathLayout->setSpacing(0);

    includeSubfoldersCheckBox = new QCheckBox(tr("サブフォルダも対象にする"), central);
    connect(includeSubfoldersCheckBox, &QCheckBox::toggled, this, &MainWindow::refreshCurrentPath);
    pathLayout->addWidget(includeSubfoldersCheckBox);
    pathLayout->addStretch(1);

	const auto spacingSize = 20;
    rootLayout->addSpacing(spacingSize);
    rootLayout->addLayout(pathLayout);
    // 非推奨の操作ではあるが、サイズ調整のため、ひとまず以下のように設定しておく
    rootLayout->addSpacing(-spacingSize);

    auto *filterLayout = new QHBoxLayout();
    filterLayout->setContentsMargins(0, 0, 0, 0);
    filterLayout->setSpacing(6);
    filterLayout->setAlignment(Qt::AlignLeft);
    filterLayout->addWidget(new QLabel(tr("フィルタ:"), central));

    filterComboBox = new QComboBox(central);
    filterComboBox->setEditable(true);
    filterComboBox->setInsertPolicy(QComboBox::NoInsert);
    filterComboBox->setMinimumWidth(180);
    filterComboBox->setMaximumWidth(180);
    filterComboBox->lineEdit()->setPlaceholderText(tr("例: *.h, *.c, *.cpp"));
    filterComboBox->lineEdit()->installEventFilter(this);
    filterLayout->addWidget(filterComboBox);

    connect(filterComboBox->lineEdit(), &QLineEdit::returnPressed, this, &MainWindow::applyFilter);
    connect(filterComboBox, &QComboBox::textActivated, this, &MainWindow::applyFilter);

    filterLayout->addSpacing(12);
    filterLayout->addWidget(new QLabel(tr("エンコード:"), central));
    encodingFilterComboBox = new QComboBox(central);
    encodingFilterComboBox->setMinimumWidth(120);
    encodingFilterComboBox->setMaximumWidth(120);
    encodingFilterComboBox->addItems({
        tr("すべて"),
    });
    connect(encodingFilterComboBox, &QComboBox::currentTextChanged, this, &MainWindow::applyStructuredFilters);
    filterLayout->addWidget(encodingFilterComboBox);

    filterLayout->addWidget(new QLabel(tr("改行:"), central));
    newlineFilterComboBox = new QComboBox(central);
    newlineFilterComboBox->setMinimumWidth(80);
    newlineFilterComboBox->setMaximumWidth(80);
    newlineFilterComboBox->addItems({
        tr("すべて"),
    });
    connect(newlineFilterComboBox, &QComboBox::currentTextChanged, this, &MainWindow::applyStructuredFilters);
    filterLayout->addWidget(newlineFilterComboBox);

    auto *changeLayout = new QHBoxLayout();
    changeLayout->setContentsMargins(0, 0, 0, 0);
    changeLayout->setSpacing(6);
    changeLayout->setAlignment(Qt::AlignLeft);

    auto *encodingGroup = new QGroupBox(tr("エンコード"), central);
    encodingGroup->setMaximumWidth(210);
    encodingGroup->setMaximumHeight(54);
    encodingGroup->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    auto *encodingGroupLayout = new QHBoxLayout(encodingGroup);
    encodingGroupLayout->setContentsMargins(8, 4, 8, 4);
    encodingGroupLayout->setSpacing(6);

    targetEncodingComboBox = new QComboBox(encodingGroup);
    targetEncodingComboBox->setMinimumWidth(130);
    targetEncodingComboBox->addItems({
        tr("UTF-8 BOMなし"),
        tr("UTF-8 BOMあり"),
        tr("UTF-16"),
        tr("SJIS/CP932"),
    });
    connect(targetEncodingComboBox, &QComboBox::currentTextChanged, this, &MainWindow::updateChangePreview);
    encodingGroupLayout->addWidget(targetEncodingComboBox);

    changeEncodingCheckBox = new QCheckBox(tr("変更"), encodingGroup);
    connect(changeEncodingCheckBox, &QCheckBox::toggled, this, &MainWindow::updateChangePreview);
    encodingGroupLayout->addWidget(changeEncodingCheckBox);
    changeLayout->addWidget(encodingGroup);

    auto *newlineGroup = new QGroupBox(tr("改行"), central);
    newlineGroup->setMaximumWidth(170);
    newlineGroup->setMaximumHeight(54);
    newlineGroup->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    auto *newlineGroupLayout = new QHBoxLayout(newlineGroup);
    newlineGroupLayout->setContentsMargins(8, 4, 8, 4);
    newlineGroupLayout->setSpacing(6);

    targetNewlineComboBox = new QComboBox(newlineGroup);
    targetNewlineComboBox->setMinimumWidth(80);
    targetNewlineComboBox->addItems({
        tr("CRLF"),
        tr("LF"),
    });
    connect(targetNewlineComboBox, &QComboBox::currentTextChanged, this, &MainWindow::updateChangePreview);
    newlineGroupLayout->addWidget(targetNewlineComboBox);

    changeNewlineCheckBox = new QCheckBox(tr("変更"), newlineGroup);
    connect(changeNewlineCheckBox, &QCheckBox::toggled, this, &MainWindow::updateChangePreview);
    newlineGroupLayout->addWidget(changeNewlineCheckBox);
    changeLayout->addWidget(newlineGroup);

    commitEncodingButton = new QPushButton(tr("決定"), central);
    commitEncodingButton->setEnabled(false);
    commitEncodingButton->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    connect(commitEncodingButton, &QPushButton::clicked, this, &MainWindow::commitEncodingChanges);

    clearRenameQueueButton = new QPushButton(tr("クリア"), central);
    clearRenameQueueButton->setEnabled(false);
    clearRenameQueueButton->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    connect(clearRenameQueueButton, &QPushButton::clicked, this, &MainWindow::clearRenameQueueItems);

    changeLayout->addWidget(commitEncodingButton);
    changeLayout->setAlignment(commitEncodingButton, Qt::AlignBottom);
    changeLayout->addWidget(clearRenameQueueButton);
    changeLayout->setAlignment(clearRenameQueueButton, Qt::AlignBottom);
    changeLayout->addStretch(1);

    auto *toolLayout = new QHBoxLayout();
    toolLayout->setContentsMargins(0, 0, 0, 0);
    toolLayout->setSpacing(4);
    toolLayout->addLayout(filterLayout);
    toolLayout->setAlignment(filterLayout, Qt::AlignBottom);
    toolLayout->addSpacing(86);
    toolLayout->addLayout(changeLayout);
    toolLayout->addStretch(1);

    rootLayout->addLayout(toolLayout);

    auto *splitter = new QSplitter(Qt::Horizontal, central);
    splitter->setHandleWidth(2);

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
    leftTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Fixed);
    leftTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    leftTable->horizontalHeader()->setMinimumSectionSize(36);
    leftTable->setColumnWidth(0, 36);
    leftTable->setColumnWidth(5, 120);
    leftTable->setColumnWidth(6, 90);
    leftTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    leftTable->setSelectionMode(QAbstractItemView::ExtendedSelection);
    leftTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    leftTable->setAcceptDrops(true);
    leftTable->installEventFilter(this);
    leftTable->viewport()->setAcceptDrops(true);
    leftTable->viewport()->installEventFilter(this);
    connect(leftTable, &QTableWidget::currentCellChanged, this, &MainWindow::updateSelectedPathLabel);
    connect(leftTable, &QTableWidget::cellClicked, this, &MainWindow::updateSelectedPathLabel);

    auto *arrowContainer = new QWidget(splitter);
    arrowContainer->setFixedWidth(62);
    arrowContainer->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
    auto *arrowLayout = new QHBoxLayout(arrowContainer);
    arrowLayout->setContentsMargins(0, 0, 0, 0);
    arrowLayout->setSpacing(0);

    addToRenameQueueButton = new QPushButton(arrowContainer);
    addToRenameQueueButton->setFixedWidth(48);
    addToRenameQueueButton->setMinimumHeight(54);
    addToRenameQueueButton->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
    addToRenameQueueButton->setIcon(QIcon(QStringLiteral(":/icons/arrow-add.png")));
    addToRenameQueueButton->setIconSize(QSize(36, 36));
    addToRenameQueueButton->setToolTip(tr("選択またはチェックされたファイルをリネーム対象へ追加"));
    addToRenameQueueButton->setAcceptDrops(true);
    addToRenameQueueButton->installEventFilter(this);
    connect(addToRenameQueueButton, &QPushButton::clicked, this, &MainWindow::addCheckedFilesToRenameQueue);
    arrowLayout->addSpacing(7);
    arrowLayout->addWidget(addToRenameQueueButton);
    arrowLayout->addSpacing(7);

    rightTable = new QTableWidget(splitter);
    rightTable->setColumnCount(3);
    rightTable->setHorizontalHeaderLabels({tr("ファイル名"), tr("変更後"), tr("状態")});
    rightTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    rightTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    rightTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    rightTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    rightTable->setSelectionMode(QAbstractItemView::ExtendedSelection);
    rightTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    rightTable->setContextMenuPolicy(Qt::CustomContextMenu);
    rightTable->setAcceptDrops(true);
    rightTable->installEventFilter(this);
    rightTable->viewport()->setAcceptDrops(true);
    rightTable->viewport()->installEventFilter(this);
    connect(rightTable, &QTableWidget::customContextMenuRequested, this, &MainWindow::showRenameQueueContextMenu);

    splitter->addWidget(leftTable);
    splitter->addWidget(arrowContainer);
    splitter->addWidget(rightTable);
    splitter->setCollapsible(1, false);
    splitter->setStretchFactor(0, 1);
    splitter->setStretchFactor(1, 0);
    splitter->setStretchFactor(2, 1);
    rootLayout->addWidget(splitter, 1);

    setCentralWidget(central);
    loadFilterHistory();
    applyViewMode();
    statusBar()->showMessage(tr("ファイルまたはフォルダを開くか、ここへドラッグ＆ドロップしてください。"));
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

    auto *editMenu = menuBar()->addMenu(tr("編集"));
    auto *renameAction = editMenu->addAction(tr("名前の変更..."));
    connect(renameAction, &QAction::triggered, this, &MainWindow::openRenameDialog);

    auto *viewMenu = menuBar()->addMenu(tr("表示"));
    auto *viewModeGroup = new QActionGroup(this);

    standardViewAction = viewMenu->addAction(tr("標準"));
    standardViewAction->setCheckable(true);
    standardViewAction->setActionGroup(viewModeGroup);
    standardViewAction->setChecked(true);

    detailViewAction = viewMenu->addAction(tr("詳細"));
    detailViewAction->setCheckable(true);
    detailViewAction->setActionGroup(viewModeGroup);

    connect(standardViewAction, &QAction::triggered, this, [this]() {
        applyViewMode();
        saveViewMode();
    });
    connect(detailViewAction, &QAction::triggered, this, [this]() {
        applyViewMode();
        saveViewMode();
    });

    viewMenu->addSeparator();

    showTextAction = viewMenu->addAction(tr("テキストファイル"));
    showTextAction->setCheckable(true);
    showTextAction->setChecked(true);
    connect(showTextAction, &QAction::toggled, this, &MainWindow::refreshCurrentPath);

    showBinaryAction = viewMenu->addAction(tr("バイナリファイル"));
    showBinaryAction->setCheckable(true);
    showBinaryAction->setChecked(false);
    connect(showBinaryAction, &QAction::toggled, this, &MainWindow::refreshCurrentPath);

    menuBar()->setStyleSheet(QStringLiteral(
        "QMenuBar {"
        "  background-color: #2b2b2b;"
        "  color: #f0f0f0;"
        "  border-bottom: 1px solid #3a3a3a;"
        "}"
        "QMenuBar::item {"
        "  background: transparent;"
        "  padding: 4px 10px;"
        "}"
        "QMenuBar::item:selected {"
        "  background-color: #3a3a3a;"
        "}"
        "QMenu {"
        "  background-color: #2b2b2b;"
        "  color: #f0f0f0;"
        "  border: 1px solid #444;"
        "}"
        "QMenu::item:selected {"
        "  background-color: #3f5f7f;"
        "}"
    ));
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

void MainWindow::loadPath(const QString &path, bool resetState, bool preservePendingChanges)
{
    if (operationInProgress) {
        return;
    }
    const QFileInfo target(path);
    if (!target.exists()) {
        statusBar()->showMessage(tr("対象が見つかりません: %1").arg(path));
        return;
    }

    currentPath = target.absoluteFilePath();
    if (resetState) {
        checkedPaths.clear();
        pendingEncodingChanges.clear();
        renameQueueItems.clear();
        const QSignalBlocker encodingBlocker(changeEncodingCheckBox);
        const QSignalBlocker newlineBlocker(changeNewlineCheckBox);
        changeEncodingCheckBox->setChecked(false);
        changeNewlineCheckBox->setChecked(false);
        currentFiles.clear();
        leftTable->setRowCount(0);
        populateRenameQueuePane();
        commitEncodingButton->setEnabled(false);
    }

    const int depth = maxScanDepth();
    const bool includeSubfolders = includeSubfoldersCheckBox->isChecked();
    const bool includeText = showTextAction->isChecked();
    const bool includeBinary = showBinaryAction->isChecked();
    const QStringList nameFilters = currentNameFilters();
    const QString scanPath = currentPath;
    const quint64 generation = ++scanGeneration;
    if (activeScanCancellation) {
        activeScanCancellation->store(true);
    }
    const auto cancellation = std::make_shared<std::atomic_bool>(false);
    activeScanCancellation = cancellation;
    commitEncodingButton->setEnabled(false);

    statusBar()->showMessage(tr("読み込み中: %1").arg(scanPath));

    auto *watcher = new QFutureWatcher<QList<FileInfo>>(this);
    connect(watcher, &QFutureWatcher<QList<FileInfo>>::finished, this, [this, watcher, generation, preservePendingChanges]() {
        const QList<FileInfo> files = watcher->result();
        watcher->deleteLater();
        if (generation != scanGeneration) {
            return;
        }

        activeScanCancellation.reset();
        currentFiles = files;
        updateStructuredFilterOptions(currentFiles);
        applyCurrentDisplayFilters(!preservePendingChanges);
        if (preservePendingChanges) {
            populateRenameQueuePane();
            commitEncodingButton->setEnabled(!pendingEncodingChanges.isEmpty() || hasPendingRenameChanges());
        }
    });
    watcher->setFuture(QtConcurrent::run([scanPath,
                                          includeText,
                                          includeBinary,
                                          includeSubfolders,
                                          depth,
                                          nameFilters,
                                          cancellation]() {
        FileScanner scanner;
        return scanner.scanPath(scanPath,
                                includeText,
                                includeBinary,
                                includeSubfolders,
                                depth,
                                nameFilters,
                                cancellation.get());
    }));
}

void MainWindow::applyCurrentDisplayFilters(bool updatePreview)
{
    const QList<FileInfo> files = filteredCurrentFiles();
    populateLeftPane(files);
    updateSelectedPathLabel();
    if (updatePreview) {
        updateChangePreview();
    }

    const QFileInfo target(currentPath);
    const int depth = maxScanDepth();
    const bool includeSubfolders = includeSubfoldersCheckBox->isChecked();
    const QStringList activeFilters = {
        filterComboBox->currentText().trimmed().isEmpty() ? QString() : tr("ファイル名: %1").arg(filterComboBox->currentText().trimmed()),
        currentEncodingFilter().isEmpty() ? QString() : tr("エンコード: %1").arg(currentEncodingFilter()),
        currentNewlineFilter().isEmpty() ? QString() : tr("改行: %1").arg(currentNewlineFilter()),
    };
    QStringList activeFilterTexts;
    for (const QString &filter : activeFilters) {
        if (!filter.isEmpty()) {
            activeFilterTexts.append(filter);
        }
    }
    const QString filterSummary = activeFilterTexts.join(QStringLiteral(" / "));
    const QString targetText = tr("対象: %1").arg(currentPath);
    if (includeSubfolders && target.isDir()) {
        if (filterSummary.isEmpty()) {
            statusBar()->showMessage(tr("%1 | %2 件を表示（サブフォルダ深さ %3 まで）")
                                         .arg(targetText)
                                         .arg(files.size())
                                         .arg(depth));
        } else {
            statusBar()->showMessage(tr("%1 | %2 件を表示（%3 / サブフォルダ深さ %4 まで）")
                                         .arg(targetText)
                                         .arg(files.size())
                                         .arg(filterSummary)
                                         .arg(depth));
        }
    } else {
        if (filterSummary.isEmpty()) {
            statusBar()->showMessage(tr("%1 | %2 件を表示").arg(targetText).arg(files.size()));
        } else {
            statusBar()->showMessage(tr("%1 | %2 件を表示（%3）")
                                         .arg(targetText)
                                         .arg(files.size())
                                         .arg(filterSummary));
        }
    }
}

void MainWindow::refreshCurrentPath()
{
    if (!currentPath.isEmpty()) {
        loadPath(currentPath, false);
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

void MainWindow::applyStructuredFilters()
{
    QSettings settings(configPath(), QSettings::IniFormat);
    settings.setValue(QStringLiteral("Filter/encoding"), encodingFilterComboBox->currentText());
    settings.setValue(QStringLiteral("Filter/newline"), newlineFilterComboBox->currentText());
    applyCurrentDisplayFilters();
}

void MainWindow::updateChangePreview()
{
    if (operationInProgress) {
        return;
    }
    pendingEncodingChanges = checkedRequestedChanges();
    commitEncodingButton->setEnabled(!activeScanCancellation
        && (!pendingEncodingChanges.isEmpty() || hasPendingRenameChanges()));
    populateRenameQueuePane(false);
}

void MainWindow::commitEncodingChanges()
{
    if (operationInProgress || activeScanCancellation) {
        return;
    }
    QScopedValueRollback<bool> operationGuard(operationInProgress, true);
    pendingEncodingChanges = checkedRequestedChanges();
    const bool hasRenameChanges = hasPendingRenameChanges();
    commitEncodingButton->setEnabled(!pendingEncodingChanges.isEmpty() || hasRenameChanges);

    if (pendingEncodingChanges.isEmpty() && !hasRenameChanges) {
        QMessageBox::information(this, tr("変更"), tr("変更対象がありません。"));
        return;
    }

    QStringList errors;
    if (hasRenameChanges && !validateRenameTargets(&errors)) {
        QMessageBox::warning(this, tr("名前の変更"), errors.join(QLatin1Char('\n')));
        populateRenameQueuePane();
        return;
    }

    int pendingRenameCount = 0;
    for (const RenameQueueItem &item : renameQueueItems) {
        if (item.status == tr("変更予定") && !item.newFileName.trimmed().isEmpty()) {
            ++pendingRenameCount;
        }
    }

    const QMessageBox::StandardButton answer = QMessageBox::question(
        this,
        tr("変更"),
        tr("リネーム %1 件、変換 %2 件を実行します。続行しますか？")
            .arg(pendingRenameCount)
            .arg(pendingEncodingChanges.size()));
    if (answer != QMessageBox::Yes) {
        return;
    }

    int renameSucceededCount = 0;
    int renameFailedCount = 0;
    QHash<QString, QString> renamedPaths;
    const QFileInfo openedTarget(currentPath);
    const QString openedFilePath = openedTarget.isFile()
        ? openedTarget.absoluteFilePath().toLower()
        : QString();
    if (hasRenameChanges) {
        for (RenameQueueItem &item : renameQueueItems) {
            if (item.status != tr("変更予定") || item.newFileName.trimmed().isEmpty()) {
                continue;
            }
            const QFileInfo sourceInfo(item.fullPath);
            const QString targetPath = sourceInfo.dir().absoluteFilePath(item.newFileName);
            const QString sourcePath = sourceInfo.absoluteFilePath();
            const QString normalizedSourcePath = sourcePath.toLower();
            if (QFileInfo(targetPath).absoluteFilePath() == sourcePath) {
                item.status = tr("変更なし");
                continue;
            }

            const bool caseOnly = targetPath.compare(sourcePath, Qt::CaseInsensitive) == 0;
            const bool renamed = caseOnly
                ? MoveFileExW(reinterpret_cast<LPCWSTR>(sourcePath.utf16()),
                              reinterpret_cast<LPCWSTR>(targetPath.utf16()), 0) != 0
                : QFile::rename(item.fullPath, targetPath);
            if (renamed) {
                const QString newPath = QFileInfo(targetPath).absoluteFilePath();
                if (checkedPaths.contains(item.fullPath)) {
                    const bool checked = checkedPaths.take(item.fullPath);
                    checkedPaths.insert(newPath, checked);
                }
                item.fileName = item.newFileName;
                item.fullPath = newPath;
                item.status = tr("成功");
                renamedPaths.insert(normalizedSourcePath, newPath);
                if (!openedFilePath.isEmpty() && normalizedSourcePath == openedFilePath) {
                    currentPath = newPath;
                }
                ++renameSucceededCount;
            } else {
                item.status = tr("失敗");
                ++renameFailedCount;
            }
        }
    }

    for (EncodingChange &change : pendingEncodingChanges) {
        const QString renamedPath = renamedPaths.value(QFileInfo(change.fullPath).absoluteFilePath().toLower());
        if (!renamedPath.isEmpty()) {
            change.fullPath = renamedPath;
            change.fileName = QFileInfo(renamedPath).fileName();
        }
    }

    QVector<EncodingChange> failedChanges;
    QVector<EncodingChange> succeededChanges;

    // The worker owns a snapshot; widgets remain on the GUI thread.
    QFutureWatcher<QVector<EncodingChange>> conversionWatcher;
    QEventLoop conversionLoop;
    QProgressDialog progress(tr("変換中..."), QString(), 0, 0, this);
    progress.setCancelButton(nullptr);
    progress.setWindowModality(Qt::ApplicationModal);
    progress.setMinimumDuration(0);
    connect(&conversionWatcher, &QFutureWatcher<QVector<EncodingChange>>::finished,
            &conversionLoop, &QEventLoop::quit);
    conversionWatcher.setFuture(QtConcurrent::run([changes = pendingEncodingChanges]() mutable {
        QHash<QString, QList<int>> groups;
        QStringList paths;
        for (int i = 0; i < changes.size(); ++i) {
            if (!groups.contains(changes[i].fullPath)) {
                paths.append(changes[i].fullPath);
            }
            groups[changes[i].fullPath].append(i);
        }
        for (const QString &path : paths) {
            QString encoding;
            QString newline;
            for (int i : groups.value(path)) {
                if (changes[i].operation == PendingOperation::Encoding) {
                    encoding = changes[i].toEncoding;
                } else {
                    newline = changes[i].toEncoding;
                }
            }
            const auto result = TextConverter::convert(path, encoding, newline);
            for (int i : groups.value(path)) {
                changes[i].succeeded = result.success;
                changes[i].failed = !result.success;
                changes[i].status = result.errorMessage;
            }
        }
        return changes;
    }));
    centralWidget()->setEnabled(false);
    menuBar()->setEnabled(false);
    progress.show();
    if (!conversionWatcher.isFinished()) {
        conversionLoop.exec();
    }
    progress.hide();
    centralWidget()->setEnabled(true);
    menuBar()->setEnabled(true);

    for (EncodingChange change : conversionWatcher.result()) {
        const TextConverter::Result result{change.succeeded, change.status};
        if (result.success) {
            change.succeeded = true;
            change.failed = false;
            change.status = tr("成功");
            succeededChanges.append(change);
        } else {
            change.succeeded = false;
            change.failed = true;
            change.status = tr("失敗: %1").arg(result.errorMessage);
            failedChanges.append(change);
        }
    }

    pendingEncodingChanges = failedChanges;
    commitEncodingButton->setEnabled(!pendingEncodingChanges.isEmpty() || hasPendingRenameChanges());

    QMessageBox::information(
        this,
        tr("変更"),
        tr("リネーム: %1 件成功 / %2 件失敗\n変換: %3 件成功 / %4 件失敗")
            .arg(renameSucceededCount)
            .arg(renameFailedCount)
            .arg(succeededChanges.size())
            .arg(failedChanges.size()));

    operationInProgress = false;
    loadPath(currentPath, false, true);
    populateRenameQueuePane();
}

void MainWindow::populateLeftPane(const QList<FileInfo> &files)
{
    leftTable->setUpdatesEnabled(false);
    leftTable->setRowCount(files.size());

    for (int row = 0; row < files.size(); ++row) {
        const FileInfo &file = files.at(row);

        auto *check = new QCheckBox(leftTable);
        const bool checked = checkedPaths.value(file.fullPath, file.kind == FileKind::Text);
        checkedPaths.insert(file.fullPath, checked);
        check->setChecked(checked);
        connect(check, &QCheckBox::toggled, this, [this, path = file.fullPath](bool value) {
            checkedPaths.insert(path, value);
            updateChangePreview();
        });
        auto *checkContainer = new QWidget(leftTable);
        auto *checkLayout = new QHBoxLayout(checkContainer);
        checkLayout->setContentsMargins(0, 0, 0, 0);
        checkLayout->setAlignment(Qt::AlignCenter);
        checkLayout->addWidget(check);
        leftTable->setCellWidget(row, 0, checkContainer);

        auto *nameItem = new QTableWidgetItem(file.fileName);
        nameItem->setToolTip(file.fullPath);
        nameItem->setData(Qt::UserRole, file.fullPath);
        leftTable->setItem(row, 1, nameItem);
        leftTable->setItem(row, 2, new QTableWidgetItem(file.createdAt.toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"))));
        leftTable->setItem(row, 3, new QTableWidgetItem(QString::number(file.size)));
        leftTable->setItem(row, 4, new QTableWidgetItem(kindText(file.kind)));
        leftTable->setItem(row, 5, new QTableWidgetItem(file.encoding));
        leftTable->setItem(row, 6, new QTableWidgetItem(file.newline));
    }

    leftTable->resizeColumnsToContents();
    leftTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Fixed);
    leftTable->setColumnWidth(0, 36);
    leftTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    leftTable->setColumnWidth(5, qMax(leftTable->columnWidth(5), 120));
    leftTable->setColumnWidth(6, qMax(leftTable->columnWidth(6), 90));
    applyViewMode();
    leftTable->setUpdatesEnabled(true);
}

void MainWindow::populateRenameQueuePane(bool resizeColumns)
{
    rightTable->setUpdatesEnabled(false);
    clearRenameQueueButton->setEnabled(!renameQueueItems.isEmpty() || !pendingEncodingChanges.isEmpty());
    rightTable->setRowCount(renameQueueItems.size() + pendingEncodingChanges.size());

    for (int row = 0; row < renameQueueItems.size(); ++row) {
        const RenameQueueItem &item = renameQueueItems.at(row);

        auto *nameItem = new QTableWidgetItem(item.fileName);
        nameItem->setToolTip(item.fullPath);
        nameItem->setData(Qt::UserRole, item.fullPath);
        nameItem->setData(Qt::UserRole + 1, QStringLiteral("rename"));
        rightTable->setItem(row, 0, nameItem);

        auto *newNameItem = new QTableWidgetItem(item.newFileName);
        newNameItem->setToolTip(item.newFileName);
        rightTable->setItem(row, 1, newNameItem);

        auto *statusItem = new QTableWidgetItem(item.status);
        statusItem->setData(Qt::ForegroundRole, QBrush(QColor(73, 80, 87)));
        rightTable->setItem(row, 2, statusItem);
    }

    for (int index = 0; index < pendingEncodingChanges.size(); ++index) {
        const int row = renameQueueItems.size() + index;
        const EncodingChange &change = pendingEncodingChanges.at(index);
        const QColor textColor = change.failed
            ? QColor(220, 53, 69)
            : (change.succeeded ? QColor(25, 135, 84) : QColor(184, 134, 11));

        auto *nameItem = new QTableWidgetItem(change.fileName);
        nameItem->setToolTip(change.fullPath);
        nameItem->setData(Qt::UserRole, change.fullPath);
        nameItem->setData(Qt::UserRole + 1, QStringLiteral("conversion"));
        rightTable->setItem(row, 0, nameItem);

        const QString operationText = change.operation == PendingOperation::Newline ? tr("改行") : tr("エンコード");
        auto *changeItem = new QTableWidgetItem(tr("%1: %2 -> %3").arg(operationText, change.fromEncoding, change.toEncoding));
        changeItem->setData(Qt::ForegroundRole, QBrush(textColor));
        rightTable->setItem(row, 1, changeItem);

        auto *statusItem = new QTableWidgetItem(change.status);
        statusItem->setData(Qt::ForegroundRole, QBrush(textColor));
        rightTable->setItem(row, 2, statusItem);
    }

    if (resizeColumns) {
        rightTable->resizeColumnsToContents();
    }
    rightTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    rightTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    rightTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    rightTable->setUpdatesEnabled(true);
}

void MainWindow::addCheckedFilesToRenameQueue()
{
    QList<int> rows;
    for (const QModelIndex &index : leftTable->selectionModel()->selectedRows()) {
        rows.append(index.row());
    }

    if (rows.isEmpty()) {
        for (int row = 0; row < leftTable->rowCount(); ++row) {
            const QWidget *checkContainer = leftTable->cellWidget(row, 0);
            const QCheckBox *checkBox = checkContainer ? checkContainer->findChild<QCheckBox *>() : nullptr;
            if (checkBox && checkBox->isChecked()) {
                rows.append(row);
            }
        }
    }

    if (rows.isEmpty()) {
        QMessageBox::information(this, tr("一括リネーム"), tr("追加するファイルを選択またはチェックしてください。"));
        return;
    }

    QStringList queuedPaths;
    for (const RenameQueueItem &item : renameQueueItems) {
        queuedPaths.append(item.fullPath);
    }

    int addedCount = 0;
    int skippedCount = 0;
    std::sort(rows.begin(), rows.end());
    rows.erase(std::unique(rows.begin(), rows.end()), rows.end());

    for (int row : rows) {
        const QTableWidgetItem *nameItem = leftTable->item(row, 1);
        if (!nameItem) {
            continue;
        }

        QString fullPath = nameItem->data(Qt::UserRole).toString();
        if (fullPath.isEmpty()) {
            fullPath = nameItem->toolTip();
        }
        if (fullPath.isEmpty() || queuedPaths.contains(fullPath, Qt::CaseInsensitive)) {
            ++skippedCount;
            continue;
        }

        RenameQueueItem item;
        item.fileName = nameItem->text();
        item.fullPath = fullPath;
        item.status = tr("待機");
        renameQueueItems.append(item);
        queuedPaths.append(fullPath);
        ++addedCount;
    }

    populateRenameQueuePane();
    statusBar()->showMessage(tr("リネーム対象に %1 件追加しました。%2 件は追加済みのためスキップしました。")
                                 .arg(addedCount)
                                 .arg(skippedCount));
}

void MainWindow::removeSelectedRenameQueueItems()
{
    QList<int> rows;
    for (const QModelIndex &index : rightTable->selectionModel()->selectedRows()) {
        if (index.row() < renameQueueItems.size()) {
            rows.append(index.row());
        }
    }
    if (rows.isEmpty()) {
        return;
    }

    std::sort(rows.begin(), rows.end(), std::greater<int>());
    for (int row : rows) {
        if (row >= 0 && row < renameQueueItems.size()) {
            renameQueueItems.removeAt(row);
        }
    }

    populateRenameQueuePane();
    statusBar()->showMessage(tr("リネーム対象から %1 件削除しました。").arg(rows.size()));
}

void MainWindow::clearRenameQueueItems()
{
    if (renameQueueItems.isEmpty() && pendingEncodingChanges.isEmpty()) {
        return;
    }

    const QMessageBox::StandardButton answer = QMessageBox::question(
        this,
        tr("クリア"),
        tr("リネーム対象と変換結果を全て削除しますがよろしいですか？"));
    if (answer != QMessageBox::Yes) {
        return;
    }

    const int clearedCount = renameQueueItems.size() + pendingEncodingChanges.size();
    renameQueueItems.clear();
    pendingEncodingChanges.clear();
    commitEncodingButton->setEnabled(false);
    populateRenameQueuePane();
    statusBar()->showMessage(tr("右ペインの項目を %1 件削除しました。").arg(clearedCount));
}

void MainWindow::moveSelectedRenameQueueItems(int direction)
{
    if (direction == 0) {
        return;
    }

    QList<int> rows;
    for (const QModelIndex &index : rightTable->selectionModel()->selectedRows()) {
        if (index.row() < renameQueueItems.size()) {
            rows.append(index.row());
        }
    }
    if (rows.isEmpty()) {
        return;
    }

    std::sort(rows.begin(), rows.end());
    rows.erase(std::unique(rows.begin(), rows.end()), rows.end());

    QList<int> movedRows;
    if (direction < 0) {
        if (rows.first() == 0) {
            return;
        }
        for (int row : rows) {
            std::swap(renameQueueItems[row], renameQueueItems[row - 1]);
            movedRows.append(row - 1);
        }
    } else {
        if (rows.last() == renameQueueItems.size() - 1) {
            return;
        }
        for (auto it = rows.crbegin(); it != rows.crend(); ++it) {
            const int row = *it;
            std::swap(renameQueueItems[row], renameQueueItems[row + 1]);
            movedRows.prepend(row + 1);
        }
    }

    populateRenameQueuePane();
    rightTable->clearSelection();
    for (int row : movedRows) {
        rightTable->selectionModel()->select(rightTable->model()->index(row, 0),
                                             QItemSelectionModel::Select | QItemSelectionModel::Rows);
    }
}

void MainWindow::showRenameQueueContextMenu(const QPoint &position)
{
    QMenu menu(this);
    QAction *moveUpAction = menu.addAction(tr("上へ移動"));
    QAction *moveDownAction = menu.addAction(tr("下へ移動"));
    menu.addSeparator();
    QAction *removeAction = menu.addAction(tr("削除"));

    bool hasRenameSelection = false;
    for (const QModelIndex &index : rightTable->selectionModel()->selectedRows()) {
        if (index.row() < renameQueueItems.size()) {
            hasRenameSelection = true;
            break;
        }
    }
    moveUpAction->setEnabled(hasRenameSelection);
    moveDownAction->setEnabled(hasRenameSelection);
    removeAction->setEnabled(hasRenameSelection);

    QAction *selectedAction = menu.exec(rightTable->viewport()->mapToGlobal(position));
    if (selectedAction == moveUpAction) {
        moveSelectedRenameQueueItems(-1);
    } else if (selectedAction == moveDownAction) {
        moveSelectedRenameQueueItems(1);
    } else if (selectedAction == removeAction) {
        removeSelectedRenameQueueItems();
    }
}

void MainWindow::openRenameDialog()
{
    if (renameQueueItems.isEmpty()) {
        QMessageBox::information(this, tr("名前の変更"), tr("右ペインにリネーム対象を追加してください。"));
        return;
    }

    QDialog dialog(this);
    dialog.setWindowTitle(tr("名前の変更"));
    dialog.resize(760, 560);
    auto *layout = new QVBoxLayout(&dialog);

    auto *formLayout = new QFormLayout();
    auto *templateEdit = new QLineEdit(&dialog);
    templateEdit->setPlaceholderText(tr("例: Fixed_\\AAA\\_\\000\\"));
    formLayout->addRow(tr("テンプレート:"), templateEdit);

    auto *extensionModeComboBox = new QComboBox(&dialog);
    extensionModeComboBox->addItems({
        tr("拡張子を維持する"),
        tr("拡張子も変更する"),
    });
    formLayout->addRow(tr("拡張子:"), extensionModeComboBox);

    auto *extensionEdit = new QLineEdit(&dialog);
    extensionEdit->setPlaceholderText(tr("例: txt"));
    extensionEdit->setEnabled(false);
    formLayout->addRow(tr("変更拡張子:"), extensionEdit);
    layout->addLayout(formLayout);

    auto *previewTable = new QTableWidget(&dialog);
    previewTable->setColumnCount(3);
    previewTable->setHorizontalHeaderLabels({tr("現在"), tr("変更後"), tr("状態")});
    previewTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    previewTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    previewTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    previewTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    previewTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    previewTable->setMinimumHeight(360);
    layout->addWidget(previewTable);

    auto updateExtensionInput = [extensionModeComboBox, extensionEdit]() {
        extensionEdit->setEnabled(extensionModeComboBox->currentIndex() == 1);
    };

    auto updatePreview = [this, previewTable, templateEdit, extensionModeComboBox, extensionEdit]() {
        const QString renameTemplate = templateEdit->text();
        const bool preserveExtension = extensionModeComboBox->currentIndex() == 0;
        const QString replacementExtension = extensionEdit->text();
        previewTable->setRowCount(renameQueueItems.size());
        for (int row = 0; row < renameQueueItems.size(); ++row) {
            const RenameQueueItem &item = renameQueueItems.at(row);
            QString errorMessage;
            const QString baseName = buildRenameBaseName(item.fullPath, renameTemplate, row, &errorMessage);
            const QString newName = errorMessage.isEmpty()
                ? buildFinalRenameName(item.fullPath, baseName, preserveExtension, replacementExtension)
                : QString();

            previewTable->setItem(row, 0, new QTableWidgetItem(item.fileName));
            previewTable->setItem(row, 1, new QTableWidgetItem(newName));
            previewTable->setItem(row, 2, new QTableWidgetItem(errorMessage.isEmpty() ? tr("OK") : errorMessage));
        }
    };
    connect(templateEdit, &QLineEdit::textChanged, &dialog, updatePreview);
    connect(extensionModeComboBox, &QComboBox::currentTextChanged, &dialog, [updateExtensionInput, updatePreview]() {
        updateExtensionInput();
        updatePreview();
    });
    connect(extensionEdit, &QLineEdit::textChanged, &dialog, updatePreview);
    updatePreview();

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    layout->addWidget(buttons);

    if (dialog.exec() != QDialog::Accepted) {
        return;
    }

    if (!applyRenameTemplate(templateEdit->text(), extensionModeComboBox->currentIndex() == 0, extensionEdit->text())) {
        return;
    }

    QStringList errors;
    if (!validateRenameTargets(&errors)) {
        QMessageBox::warning(this, tr("名前の変更"), errors.join(QLatin1Char('\n')));
        populateRenameQueuePane();
        return;
    }

    populateRenameQueuePane();
    commitEncodingButton->setEnabled(!pendingEncodingChanges.isEmpty() || hasPendingRenameChanges());
    statusBar()->showMessage(tr("名前変更のプレビューを右ペインに反映しました。決定ボタンで実行します。"));
}

bool MainWindow::applyRenameTemplate(const QString &renameTemplate, bool preserveExtension, const QString &replacementExtension)
{
    if (renameTemplate.isEmpty()) {
        QMessageBox::warning(this, tr("名前の変更"), tr("テンプレートを入力してください。"));
        return false;
    }

    for (int row = 0; row < renameQueueItems.size(); ++row) {
        QString errorMessage;
        const QString baseName = buildRenameBaseName(renameQueueItems.at(row).fullPath, renameTemplate, row, &errorMessage);
        const QString newName = errorMessage.isEmpty()
            ? buildFinalRenameName(renameQueueItems.at(row).fullPath, baseName, preserveExtension, replacementExtension)
            : QString();
        renameQueueItems[row].newFileName = newName;
        renameQueueItems[row].status = errorMessage.isEmpty() ? tr("変更予定") : errorMessage;
        if (!errorMessage.isEmpty()) {
            populateRenameQueuePane();
            QMessageBox::warning(this, tr("名前の変更"), errorMessage);
            return false;
        }
    }

    populateRenameQueuePane();
    commitEncodingButton->setEnabled(!pendingEncodingChanges.isEmpty() || hasPendingRenameChanges());
    return true;
}

bool MainWindow::hasPendingRenameChanges() const
{
    for (const RenameQueueItem &item : renameQueueItems) {
        if (item.status == tr("変更予定") && !item.newFileName.trimmed().isEmpty()) {
            return true;
        }
    }
    return false;
}

QString MainWindow::buildRenameName(const QString &renameTemplate, int index, QString *errorMessage) const
{
    QString output;
    int position = 0;

    while (position < renameTemplate.size()) {
        const int start = renameTemplate.indexOf(QLatin1Char('\\'), position);
        if (start < 0) {
            output += renameTemplate.mid(position);
            break;
        }

        output += renameTemplate.mid(position, start - position);
        if (start + 1 < renameTemplate.size() && renameTemplate.at(start + 1) == QLatin1Char('\\')) {
            output += QLatin1Char('\\');
            position = start + 2;
            continue;
        }

        const int end = renameTemplate.indexOf(QLatin1Char('\\'), start + 1);
        if (end < 0) {
            if (errorMessage) {
                *errorMessage = tr("テンプレートの連番指定が閉じられていません。");
            }
            return QString();
        }

        const QString token = renameTemplate.mid(start + 1, end - start - 1);
        if (token.isEmpty()) {
            if (errorMessage) {
                *errorMessage = tr("空の連番指定があります。");
            }
            return QString();
        }

        const bool alphaToken = std::all_of(token.cbegin(), token.cend(), [](QChar c) {
            return c == QLatin1Char('A');
        });
        const bool numberToken = std::all_of(token.cbegin(), token.cend(), [](QChar c) {
            return c == QLatin1Char('0');
        });

        if (alphaToken) {
            output += alphabetSequence(index, token.size());
        } else if (numberToken) {
            output += numberSequence(index, token.size());
        } else {
            if (errorMessage) {
                *errorMessage = tr("連番指定は A または 0 のみ使用できます: \\%1\\").arg(token);
            }
            return QString();
        }

        position = end + 1;
    }

    output.replace(QStringLiteral("$$"), QStringLiteral("$"));
    output.replace(QStringLiteral("^^"), QStringLiteral("^"));

    if (output.trimmed().isEmpty()) {
        if (errorMessage) {
            *errorMessage = tr("変更後のファイル名が空です。");
        }
        return QString();
    }

    return output;
}

QString MainWindow::buildRenameBaseName(const QString &sourcePath,
                                        const QString &renameTemplate,
                                        int index,
                                        QString *errorMessage) const
{
    if (renameTemplate.startsWith(QStringLiteral("$$"))) {
        return buildRenameName(renameTemplate.mid(1), index, errorMessage);
    }
    if (renameTemplate.startsWith(QStringLiteral("^^"))) {
        return buildRenameName(renameTemplate.mid(1), index, errorMessage);
    }

    const bool appendToCurrentName = renameTemplate.startsWith(QLatin1Char('$'));
    const bool prependToCurrentName = renameTemplate.startsWith(QLatin1Char('^'));
    if (!appendToCurrentName && !prependToCurrentName) {
        return buildRenameName(renameTemplate, index, errorMessage);
    }

    const QFileInfo sourceInfo(sourcePath);
    QString currentBaseName = sourceInfo.completeBaseName();
    if (currentBaseName.isEmpty()) {
        currentBaseName = sourceInfo.fileName();
    }

    const QString bodyTemplate = renameTemplate.mid(1);
    if (prependToCurrentName) {
        const int appendMarker = findUnescapedMarker(bodyTemplate, QLatin1Char('$'));
        if (appendMarker >= 0) {
            const QString prefixTemplate = bodyTemplate.left(appendMarker);
            const QString suffixTemplate = bodyTemplate.mid(appendMarker + 1);
            const QString prefix = buildRenameName(prefixTemplate, index, errorMessage);
            if (errorMessage && !errorMessage->isEmpty()) {
                return QString();
            }
            const QString suffix = buildRenameName(suffixTemplate, index, errorMessage);
            if (errorMessage && !errorMessage->isEmpty()) {
                return QString();
            }
            return prefix + currentBaseName + suffix;
        }
    }

    const QString addition = buildRenameName(bodyTemplate, index, errorMessage);
    if (errorMessage && !errorMessage->isEmpty()) {
        return QString();
    }

    return appendToCurrentName ? currentBaseName + addition : addition + currentBaseName;
}

QString MainWindow::buildFinalRenameName(const QString &sourcePath,
                                         const QString &baseName,
                                         bool preserveExtension,
                                         const QString &replacementExtension) const
{
    QString extension;
    if (preserveExtension) {
        extension = QFileInfo(sourcePath).suffix();
    } else {
        extension = replacementExtension.trimmed();
        while (extension.startsWith(QLatin1Char('.'))) {
            extension.remove(0, 1);
        }
    }

    if (extension.isEmpty()) {
        return baseName;
    }
    return QStringLiteral("%1.%2").arg(baseName, extension);
}

QString MainWindow::alphabetSequence(int index, int minimumWidth) const
{
    QString result;
    qint64 value = qMax(0, index);
    do {
        const int digit = value % 26;
        result.prepend(QChar(QLatin1Char('A' + digit)));
        value /= 26;
        if (result.size() >= minimumWidth) {
            --value;
        }
    } while (value >= 0);
    return result;
}

QString MainWindow::numberSequence(int index, int minimumWidth) const
{
    return QString::number(index + 1).rightJustified(minimumWidth, QLatin1Char('0'));
}

bool MainWindow::validateRenameTargets(QStringList *errors) const
{
    QStringList validationErrors;
    QHash<QString, int> targetPathCounts;

    const QRegularExpression invalidChars(QStringLiteral(R"([<>:"/\\|?*])"));
    for (const RenameQueueItem &item : renameQueueItems) {
        if (item.status != tr("変更予定") || item.newFileName.trimmed().isEmpty()) {
            continue;
        }

        const QFileInfo sourceInfo(item.fullPath);
        const QString newName = item.newFileName.trimmed();
        const QString targetPath = sourceInfo.dir().absoluteFilePath(newName);

        if (!sourceInfo.exists()) {
            validationErrors.append(tr("%1: 元ファイルが見つかりません。").arg(item.fileName));
            continue;
        }
        if (newName.isEmpty()) {
            validationErrors.append(tr("%1: 変更後のファイル名が空です。").arg(item.fileName));
            continue;
        }
        if (newName.contains(invalidChars)) {
            validationErrors.append(tr("%1: 変更後のファイル名に使用できない文字があります。").arg(newName));
            continue;
        }
        if (newName == QStringLiteral(".") || newName == QStringLiteral("..")) {
            validationErrors.append(tr("%1: このファイル名は使用できません。").arg(newName));
            continue;
        }

        const QString normalizedTarget = QFileInfo(targetPath).absoluteFilePath().toLower();
        targetPathCounts[normalizedTarget] += 1;

        const QString normalizedSource = sourceInfo.absoluteFilePath().toLower();
        if (QFileInfo::exists(targetPath)
            && QFileInfo(targetPath).absoluteFilePath().toLower() != normalizedSource) {
            validationErrors.append(tr("%1: 同名のファイルが既に存在します。").arg(newName));
        }
    }

    for (auto it = targetPathCounts.cbegin(); it != targetPathCounts.cend(); ++it) {
        if (it.value() > 1) {
            validationErrors.append(tr("変更後のファイル名が重複しています: %1").arg(it.key()));
        }
    }

    if (errors) {
        *errors = validationErrors;
    }
    return validationErrors.isEmpty();
}

void MainWindow::updateStructuredFilterOptions(const QList<FileInfo> &files)
{
    const QString previousEncoding = encodingFilterComboBox->currentText();
    const QString previousNewline = newlineFilterComboBox->currentText();

    QStringList encodings;
    QStringList newlines;
    for (const FileInfo &file : files) {
        if (!file.encoding.isEmpty() && !encodings.contains(file.encoding)) {
            encodings.append(file.encoding);
        }
        if (!file.newline.isEmpty() && !newlines.contains(file.newline)) {
            newlines.append(file.newline);
        }
    }
    encodings.sort(Qt::CaseInsensitive);
    newlines.sort(Qt::CaseInsensitive);

    const bool encodingBlocked = encodingFilterComboBox->blockSignals(true);
    encodingFilterComboBox->clear();
    encodingFilterComboBox->addItem(tr("すべて"));
    encodingFilterComboBox->addItems(encodings);
    const int encodingIndex = encodingFilterComboBox->findText(previousEncoding);
    encodingFilterComboBox->setCurrentIndex(encodingIndex >= 0 ? encodingIndex : 0);
    encodingFilterComboBox->blockSignals(encodingBlocked);

    const bool newlineBlocked = newlineFilterComboBox->blockSignals(true);
    newlineFilterComboBox->clear();
    newlineFilterComboBox->addItem(tr("すべて"));
    newlineFilterComboBox->addItems(newlines);
    const int newlineIndex = newlineFilterComboBox->findText(previousNewline);
    newlineFilterComboBox->setCurrentIndex(newlineIndex >= 0 ? newlineIndex : 0);
    newlineFilterComboBox->blockSignals(newlineBlocked);

    QSettings settings(configPath(), QSettings::IniFormat);
    settings.setValue(QStringLiteral("Filter/encoding"), encodingFilterComboBox->currentText());
    settings.setValue(QStringLiteral("Filter/newline"), newlineFilterComboBox->currentText());
}

void MainWindow::applyViewMode()
{
    const bool detailMode = detailViewAction && detailViewAction->isChecked();
    leftTable->setColumnHidden(2, !detailMode);
    leftTable->setColumnHidden(3, !detailMode);
    leftTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    leftTable->setColumnWidth(5, qMax(leftTable->columnWidth(5), 120));
    leftTable->setColumnWidth(6, qMax(leftTable->columnWidth(6), 90));
}

void MainWindow::saveViewMode()
{
    QSettings settings(configPath(), QSettings::IniFormat);
    settings.setValue(QStringLiteral("View/mode"), detailViewAction && detailViewAction->isChecked()
                                                   ? QStringLiteral("detail")
                                                   : QStringLiteral("standard"));
}

void MainWindow::updateSelectedPathLabel()
{
    const int row = leftTable->currentRow();
    const QTableWidgetItem *nameItem = row >= 0 ? leftTable->item(row, 1) : nullptr;
    const QString selectedPath = nameItem ? nameItem->toolTip() : QString();

    if (!selectedPath.isEmpty()) {
        statusBar()->showMessage(tr("対象: %1").arg(selectedPath));
        return;
    }

    if (!currentPath.isEmpty()) {
        statusBar()->showMessage(tr("対象: %1").arg(currentPath));
        return;
    }

    statusBar()->showMessage(tr("ファイルまたはフォルダを開くか、ここへドラッグ＆ドロップしてください。"));
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

    const QString encodingFilter = settings.value(QStringLiteral("Filter/encoding"), tr("すべて")).toString();
    const bool encodingBlocked = encodingFilterComboBox->blockSignals(true);
    if (encodingFilterComboBox->findText(encodingFilter) < 0) {
        encodingFilterComboBox->addItem(encodingFilter);
    }
    const int encodingIndex = encodingFilterComboBox->findText(encodingFilter);
    if (encodingIndex >= 0) {
        encodingFilterComboBox->setCurrentIndex(encodingIndex);
    }
    encodingFilterComboBox->blockSignals(encodingBlocked);

    const QString newlineFilter = settings.value(QStringLiteral("Filter/newline"), tr("すべて")).toString();
    const bool newlineBlocked = newlineFilterComboBox->blockSignals(true);
    if (newlineFilterComboBox->findText(newlineFilter) < 0) {
        newlineFilterComboBox->addItem(newlineFilter);
    }
    const int newlineIndex = newlineFilterComboBox->findText(newlineFilter);
    if (newlineIndex >= 0) {
        newlineFilterComboBox->setCurrentIndex(newlineIndex);
    }
    newlineFilterComboBox->blockSignals(newlineBlocked);

}

void MainWindow::loadViewMode()
{
    QSettings settings(configPath(), QSettings::IniFormat);
    const QString viewMode = settings.value(QStringLiteral("View/mode"), QStringLiteral("standard")).toString();
    if (detailViewAction && viewMode == QStringLiteral("detail")) {
        detailViewAction->setChecked(true);
    } else if (standardViewAction) {
        standardViewAction->setChecked(true);
    }
    applyViewMode();
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

QString MainWindow::currentEncodingFilter() const
{
    const QString filter = encodingFilterComboBox->currentText();
    return filter == tr("すべて") ? QString() : filter;
}

QString MainWindow::currentNewlineFilter() const
{
    const QString filter = newlineFilterComboBox->currentText();
    return filter == tr("すべて") ? QString() : filter;
}

QList<FileInfo> MainWindow::filteredCurrentFiles() const
{
    const QString encodingFilter = currentEncodingFilter();
    const QString newlineFilter = currentNewlineFilter();
    QList<FileInfo> files;

    for (const FileInfo &file : currentFiles) {
        if (!encodingFilter.isEmpty() && file.encoding != encodingFilter) {
            continue;
        }
        if (!newlineFilter.isEmpty() && file.newline != newlineFilter) {
            continue;
        }
        files.append(file);
    }

    return files;
}

QVector<EncodingChange> MainWindow::checkedEncodingChanges() const
{
    QVector<EncodingChange> changes;
    const QString toEncoding = targetEncodingComboBox->currentText();

    for (int row = 0; row < leftTable->rowCount(); ++row) {
        const QWidget *checkContainer = leftTable->cellWidget(row, 0);
        const QCheckBox *checkBox = checkContainer ? checkContainer->findChild<QCheckBox *>() : nullptr;
        if (!checkBox || !checkBox->isChecked()) {
            continue;
        }

        const QTableWidgetItem *nameItem = leftTable->item(row, 1);
        const QTableWidgetItem *encodingItem = leftTable->item(row, 5);
        if (!nameItem || !encodingItem) {
            continue;
        }

        const QString fromEncoding = encodingItem->text();
        if (isSameEncoding(fromEncoding, toEncoding)) {
            continue;
        }

        EncodingChange change;
        change.fileName = nameItem->text();
        change.fullPath = nameItem->data(Qt::UserRole).toString();
        if (change.fullPath.isEmpty()) {
            change.fullPath = nameItem->toolTip();
        }
        change.fromEncoding = fromEncoding;
        change.toEncoding = toEncoding;
        change.status = tr("変更予定");
        change.operation = PendingOperation::Encoding;
        changes.append(change);
    }

    return changes;
}

bool MainWindow::isSameEncoding(const QString &fromEncoding, const QString &toEncoding) const
{
    if (fromEncoding == toEncoding) {
        return true;
    }
    if (fromEncoding == QStringLiteral("UTF-8/ASCII") && toEncoding == QStringLiteral("UTF-8 BOMなし")) {
        return true;
    }
    return false;
}

QVector<EncodingChange> MainWindow::checkedRequestedChanges() const
{
    QVector<EncodingChange> changes;
    if (changeEncodingCheckBox && changeEncodingCheckBox->isChecked()) {
        changes += checkedEncodingChanges();
    }
    if (changeNewlineCheckBox && changeNewlineCheckBox->isChecked()) {
        changes += checkedNewlineChanges();
    }
    return changes;
}

QVector<EncodingChange> MainWindow::checkedNewlineChanges() const
{
    QVector<EncodingChange> changes;
    const QString toNewline = targetNewlineComboBox->currentText();

    for (int row = 0; row < leftTable->rowCount(); ++row) {
        const QWidget *checkContainer = leftTable->cellWidget(row, 0);
        const QCheckBox *checkBox = checkContainer ? checkContainer->findChild<QCheckBox *>() : nullptr;
        if (!checkBox || !checkBox->isChecked()) {
            continue;
        }

        const QTableWidgetItem *nameItem = leftTable->item(row, 1);
        const QTableWidgetItem *newlineItem = leftTable->item(row, 6);
        if (!nameItem || !newlineItem) {
            continue;
        }

        const QString fromNewline = newlineItem->text();
        if (isSameNewline(fromNewline, toNewline)) {
            continue;
        }

        EncodingChange change;
        change.fileName = nameItem->text();
        change.fullPath = nameItem->data(Qt::UserRole).toString();
        if (change.fullPath.isEmpty()) {
            change.fullPath = nameItem->toolTip();
        }
        change.fromEncoding = fromNewline;
        change.toEncoding = toNewline;
        change.status = tr("変更予定");
        change.operation = PendingOperation::Newline;
        changes.append(change);
    }

    return changes;
}

bool MainWindow::isSameNewline(const QString &fromNewline, const QString &toNewline) const
{
    return fromNewline == toNewline;
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
