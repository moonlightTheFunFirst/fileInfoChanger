#include "MainWindow.h"

#include "FileScanner.h"
#include "TextConverter.h"

#include <QAction>
#include <QApplication>
#include <QBrush>
#include <QCheckBox>
#include <QColor>
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
#include <QMessageBox>
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
    filterComboBox->setMinimumWidth(180);
    filterComboBox->setMaximumWidth(260);
    filterComboBox->lineEdit()->setPlaceholderText(tr("例: *.h, *.c, *.cpp"));
    filterLayout->addWidget(filterComboBox);

    auto *applyFilterButton = new QPushButton(tr("適用"), central);
    connect(applyFilterButton, &QPushButton::clicked, this, &MainWindow::applyFilter);
    connect(filterComboBox->lineEdit(), &QLineEdit::returnPressed, this, &MainWindow::applyFilter);
    connect(filterComboBox, &QComboBox::textActivated, this, &MainWindow::applyFilter);
    filterLayout->addWidget(applyFilterButton);

    filterLayout->addSpacing(12);
    filterLayout->addWidget(new QLabel(tr("エンコード:"), central));
    encodingFilterComboBox = new QComboBox(central);
    encodingFilterComboBox->addItems({
        tr("すべて"),
        tr("UTF-8/ASCII"),
        tr("UTF-8 BOMなし"),
        tr("UTF-8 BOMあり"),
        tr("UTF-16"),
        tr("SJIS/CP932"),
        tr("不明"),
        tr("読込不可"),
    });
    connect(encodingFilterComboBox, &QComboBox::currentTextChanged, this, &MainWindow::applyStructuredFilters);
    filterLayout->addWidget(encodingFilterComboBox);

    filterLayout->addWidget(new QLabel(tr("改行:"), central));
    newlineFilterComboBox = new QComboBox(central);
    newlineFilterComboBox->addItems({
        tr("すべて"),
        tr("CRLF"),
        tr("LF"),
        tr("CR"),
        tr("混在"),
        tr("なし"),
        tr("-"),
    });
    connect(newlineFilterComboBox, &QComboBox::currentTextChanged, this, &MainWindow::applyStructuredFilters);
    filterLayout->addWidget(newlineFilterComboBox);

    filterLayout->addSpacing(16);
    filterLayout->addWidget(new QLabel(tr("変更先エンコード:"), central));

    targetEncodingComboBox = new QComboBox(central);
    targetEncodingComboBox->addItems({
        tr("UTF-8 BOMなし"),
        tr("UTF-8 BOMあり"),
        tr("UTF-16"),
        tr("SJIS/CP932"),
    });
    filterLayout->addWidget(targetEncodingComboBox);

    auto *previewEncodingButton = new QPushButton(tr("エンコード変更"), central);
    connect(previewEncodingButton, &QPushButton::clicked, this, &MainWindow::previewEncodingChanges);
    filterLayout->addWidget(previewEncodingButton);

    filterLayout->addSpacing(12);
    filterLayout->addWidget(new QLabel(tr("変更先改行:"), central));

    targetNewlineComboBox = new QComboBox(central);
    targetNewlineComboBox->addItems({
        tr("CRLF"),
        tr("LF"),
    });
    filterLayout->addWidget(targetNewlineComboBox);

    auto *previewNewlineButton = new QPushButton(tr("改行変更"), central);
    connect(previewNewlineButton, &QPushButton::clicked, this, &MainWindow::previewNewlineChanges);
    filterLayout->addWidget(previewNewlineButton);

    commitEncodingButton = new QPushButton(tr("決定"), central);
    commitEncodingButton->setEnabled(false);
    connect(commitEncodingButton, &QPushButton::clicked, this, &MainWindow::commitEncodingChanges);
    filterLayout->addWidget(commitEncodingButton);
    filterLayout->addStretch(1);

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
    rightTable->setHorizontalHeaderLabels({tr("ファイル名"), tr("現在のエンコード"), tr("変更内容"), tr("状態")});
    rightTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    rightTable->setEditTriggers(QAbstractItemView::NoEditTriggers);

    splitter->addWidget(leftTable);
    splitter->addWidget(arrowLabel);
    splitter->addWidget(rightTable);
    splitter->setStretchFactor(0, 1);
    splitter->setStretchFactor(1, 0);
    splitter->setStretchFactor(2, 1);
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
                                                   currentNameFilters(),
                                                   currentEncodingFilter(),
                                                   currentNewlineFilter());
    populateLeftPane(files);
    updateSelectedPathLabel();
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
    if (includeSubfolders && target.isDir()) {
        if (filterSummary.isEmpty()) {
            statusBar()->showMessage(tr("%1 件を表示（サブフォルダ深さ %2 まで）").arg(files.size()).arg(depth));
        } else {
            statusBar()->showMessage(tr("%1 件を表示（%2 / サブフォルダ深さ %3 まで）")
                                         .arg(files.size())
                                         .arg(filterSummary)
                                         .arg(depth));
        }
    } else {
        if (filterSummary.isEmpty()) {
            statusBar()->showMessage(tr("%1 件を表示").arg(files.size()));
        } else {
            statusBar()->showMessage(tr("%1 件を表示（%2）").arg(files.size()).arg(filterSummary));
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

void MainWindow::applyStructuredFilters()
{
    QSettings settings(configPath(), QSettings::IniFormat);
    settings.setValue(QStringLiteral("Filter/encoding"), encodingFilterComboBox->currentText());
    settings.setValue(QStringLiteral("Filter/newline"), newlineFilterComboBox->currentText());
    refreshCurrentPath();
}

void MainWindow::previewEncodingChanges()
{
    pendingOperation = PendingOperation::Encoding;
    pendingEncodingChanges = checkedEncodingChanges();
    rightTable->setHorizontalHeaderLabels({tr("ファイル名"), tr("現在のエンコード"), tr("変更内容"), tr("状態")});
    populateRightPane(pendingEncodingChanges);
    commitEncodingButton->setEnabled(!pendingEncodingChanges.isEmpty());

    if (pendingEncodingChanges.isEmpty()) {
        QMessageBox::information(this, tr("エンコード変更"), tr("変更対象がありません。"));
    }
}

void MainWindow::previewNewlineChanges()
{
    pendingOperation = PendingOperation::Newline;
    pendingEncodingChanges = checkedNewlineChanges();
    rightTable->setHorizontalHeaderLabels({tr("ファイル名"), tr("現在の改行"), tr("変更内容"), tr("状態")});
    populateRightPane(pendingEncodingChanges);
    commitEncodingButton->setEnabled(!pendingEncodingChanges.isEmpty());

    if (pendingEncodingChanges.isEmpty()) {
        QMessageBox::information(this, tr("改行変更"), tr("変更対象がありません。"));
    }
}

void MainWindow::commitEncodingChanges()
{
    if (pendingEncodingChanges.isEmpty()) {
        QMessageBox::information(this, tr("変更"), tr("変更対象がありません。"));
        return;
    }

    const bool isNewlineOperation = pendingOperation == PendingOperation::Newline;
    const QString operationTitle = isNewlineOperation ? tr("改行変更") : tr("エンコード変更");
    const QString operationLabel = isNewlineOperation ? tr("改行") : tr("エンコード");

    const QMessageBox::StandardButton answer = QMessageBox::question(
        this,
        operationTitle,
        tr("%1 件の%2を変更します。続行しますか？").arg(pendingEncodingChanges.size()).arg(operationLabel));
    if (answer != QMessageBox::Yes) {
        return;
    }

    QVector<EncodingChange> failedChanges;
    QVector<EncodingChange> succeededChanges;

    for (EncodingChange change : pendingEncodingChanges) {
        const TextConverter::Result result = isNewlineOperation
            ? TextConverter::convertNewline(change.fullPath, change.toEncoding)
            : TextConverter::convertEncoding(change.fullPath, change.toEncoding);
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
    pendingEncodingChanges += succeededChanges;
    populateRightPane(pendingEncodingChanges);
    commitEncodingButton->setEnabled(false);

    QMessageBox::information(
        this,
        operationTitle,
        tr("%1件の%2を変更しました。\n%3件の%2変更に失敗しました。")
            .arg(succeededChanges.size())
            .arg(operationLabel)
            .arg(failedChanges.size()));

    refreshCurrentPath();
    populateRightPane(pendingEncodingChanges);
    pendingOperation = PendingOperation::None;
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
        nameItem->setData(Qt::UserRole, file.fullPath);
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

void MainWindow::populateRightPane(const QVector<EncodingChange> &changes)
{
    rightTable->setRowCount(changes.size());

    for (int row = 0; row < changes.size(); ++row) {
        const EncodingChange &change = changes.at(row);
        const QColor textColor = change.failed
            ? QColor(220, 53, 69)
            : (change.succeeded ? QColor(25, 135, 84) : QColor(184, 134, 11));

        auto *nameItem = new QTableWidgetItem(change.fileName);
        nameItem->setToolTip(change.fullPath);
        rightTable->setItem(row, 0, nameItem);

        auto *fromItem = new QTableWidgetItem(change.fromEncoding);
        rightTable->setItem(row, 1, fromItem);

        auto *changeItem = new QTableWidgetItem(tr("%1 -> %2").arg(change.fromEncoding, change.toEncoding));
        changeItem->setData(Qt::ForegroundRole, QBrush(textColor));
        rightTable->setItem(row, 2, changeItem);

        auto *statusItem = new QTableWidgetItem(change.status);
        statusItem->setData(Qt::ForegroundRole, QBrush(textColor));
        rightTable->setItem(row, 3, statusItem);
    }

    rightTable->resizeColumnsToContents();
    rightTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
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

    const QString encodingFilter = settings.value(QStringLiteral("Filter/encoding"), tr("すべて")).toString();
    const int encodingIndex = encodingFilterComboBox->findText(encodingFilter);
    if (encodingIndex >= 0) {
        encodingFilterComboBox->setCurrentIndex(encodingIndex);
    }

    const QString newlineFilter = settings.value(QStringLiteral("Filter/newline"), tr("すべて")).toString();
    const int newlineIndex = newlineFilterComboBox->findText(newlineFilter);
    if (newlineIndex >= 0) {
        newlineFilterComboBox->setCurrentIndex(newlineIndex);
    }
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
