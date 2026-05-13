#include "MainWindow.hpp"
#include "SplHighlighter.hpp"

#include <QAbstractItemView>
#include <QAction>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFileSystemModel>
#include <QFont>
#include <QFontDatabase>
#include <QHeaderView>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QProcess>
#include <QProcessEnvironment>
#include <QSplitter>
#include <QStatusBar>
#include <QTabWidget>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QTextBlock>
#include <QTextCursor>
#include <QTextStream>
#include <QToolBar>
#include <QTreeView>
#include <QVBoxLayout>
#include <QSysInfo>

namespace {

void jumpEditorToLine(QPlainTextEdit* ed, int oneBasedLine) {
    if (!ed || oneBasedLine < 1) return;
    const QTextBlock b = ed->document()->findBlockByLineNumber(oneBasedLine - 1);
    if (!b.isValid()) return;
    QTextCursor c(b);
    c.movePosition(QTextCursor::StartOfLine);
    ed->setTextCursor(c);
    ed->centerCursor();
    ed->setFocus();
}

} // namespace

bool MainWindow::sameFilePath(const QString& a, const QString& b) {
    if (a.isEmpty() || b.isEmpty()) return false;
    const QFileInfo fa(a), fb(b);
    const QString ca = fa.canonicalFilePath();
    const QString cb = fb.canonicalFilePath();
    if (!ca.isEmpty() && !cb.isEmpty())
        return QString::compare(ca, cb, Qt::CaseInsensitive) == 0;
#if defined(Q_OS_WIN)
    return QString::compare(QDir::cleanPath(a), QDir::cleanPath(b), Qt::CaseInsensitive) == 0;
#else
    return QDir::cleanPath(a) == QDir::cleanPath(b);
#endif
}

MainWindow::MainWindow() {
    createUi();
}

void MainWindow::configureEditor(QPlainTextEdit* ed) const {
    if (!ed) return;
    ed->setLineWrapMode(QPlainTextEdit::NoWrap);
    ed->setTabStopDistance(4 * ed->fontMetrics().horizontalAdvance(' '));
    QFont f = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    f.setStyleHint(QFont::Monospace);
    f.setFixedPitch(true);
    if (f.pointSizeF() <= 0) f.setPointSize(11);
    ed->setFont(f);
}

void MainWindow::createUi() {
    setWindowTitle(QStringLiteral("Kern IDE"));

    auto* fileMenu = menuBar()->addMenu(tr("&File"));
    fileMenu->addAction(tr("&Open..."), this, [this] {
        const QString dir = workspaceRoot_.isEmpty() ? QDir::currentPath() : workspaceRoot_;
        const QString f = QFileDialog::getOpenFileName(this, tr("Open Kern file"), dir,
            tr("Kern sources (*.kn);;All files (*)"));
        if (!f.isEmpty()) openFile(f);
    }, QKeySequence::Open);
    fileMenu->addAction(tr("Open &Folder..."), this, &MainWindow::openFolder, QKeySequence());
    fileMenu->addSeparator();
    fileMenu->addAction(tr("&Save"), this, &MainWindow::saveCurrent, QKeySequence::Save);
    fileMenu->addAction(tr("Save &As..."), this, &MainWindow::saveCurrentAs, QKeySequence::SaveAs);
    fileMenu->addSeparator();
    fileMenu->addAction(tr("E&xit"), this, &QWidget::close, QKeySequence::Quit);

    auto* bar = addToolBar(tr("Main"));
    bar->setMovable(false);
    bar->setIconSize(QSize(16, 16));

    bar->addAction(tr("Open Folder"), this, &MainWindow::openFolder);
    bar->addAction(tr("New"), this, &MainWindow::newTab);
    bar->addAction(tr("Save"), this, &MainWindow::saveCurrent);
    bar->addSeparator();
    bar->addAction(tr("Run"), this, &MainWindow::runCurrent);
    bar->addAction(tr("Check"), this, &MainWindow::checkCurrent);
    bar->addAction(tr("Format"), this, &MainWindow::formatCurrent);

    resize(1360, 860);

    auto* root = new QWidget(this);
    auto* layout = new QVBoxLayout(root);
    layout->setContentsMargins(0, 0, 0, 0);

    auto* hSplit = new QSplitter(Qt::Horizontal, root);
    fsModel_ = new QFileSystemModel(this);
    fsModel_->setNameFilters({QStringLiteral("*.kn"), QStringLiteral("*.txt"), QStringLiteral("*.md"),
        QStringLiteral("*.json"), QStringLiteral("*.yml"), QStringLiteral("*.yaml"),
        QStringLiteral("*.cpp"), QStringLiteral("*.hpp"), QStringLiteral("*.h"), QStringLiteral("CMakeLists.txt")});
    fsModel_->setNameFilterDisables(false);
    fsModel_->setRootPath(QDir::currentPath());

    tree_ = new QTreeView(hSplit);
    tree_->setModel(fsModel_);
    tree_->setRootIndex(fsModel_->index(QDir::currentPath()));
    tree_->setHeaderHidden(true);
    for (int c = 1; c <= 3; ++c) tree_->hideColumn(c);
    connect(tree_, &QTreeView::doubleClicked, this, &MainWindow::openFileFromTree);

    auto* rightColumn = new QWidget(hSplit);
    auto* rightOuter = new QVBoxLayout(rightColumn);
    rightOuter->setContentsMargins(0, 0, 0, 0);

    mainVerticalSplit_ = new QSplitter(Qt::Vertical, rightColumn);
    tabs_ = new QTabWidget(mainVerticalSplit_);
    tabs_->setTabsClosable(true);
    connect(tabs_, &QTabWidget::tabCloseRequested, tabs_, [this](int idx) {
        QWidget* w = tabs_->widget(idx);
        auto* ed = qobject_cast<QPlainTextEdit*>(w);
        if (ed) editorPaths_.remove(ed);
        tabs_->removeTab(idx);
        if (w) w->deleteLater();
    });
    connect(tabs_, &QTabWidget::currentChanged, this, &MainWindow::onTabChanged);

    auto* bottomPane = new QWidget(mainVerticalSplit_);
    auto* bottomLay = new QVBoxLayout(bottomPane);
    bottomLay->setContentsMargins(4, 2, 4, 4);
    bottomLay->setSpacing(4);

    diagHeader_ = new QLabel(tr("Diagnostics — use Check on the active saved .kn file (double-click row to jump)."), bottomPane);
    diagHeader_->setWordWrap(true);

    auto* bottomSplit = new QSplitter(Qt::Vertical, bottomPane);
    problems_ = new QTableWidget(0, 4, bottomSplit);
    problems_->setHorizontalHeaderLabels({tr("Kind"), tr("Code"), tr("Line"), tr("Message")});
    problems_->horizontalHeader()->setStretchLastSection(true);
    problems_->setSelectionBehavior(QAbstractItemView::SelectRows);
    problems_->setSelectionMode(QAbstractItemView::SingleSelection);
    problems_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    problems_->setMinimumHeight(96);
    problems_->verticalHeader()->setVisible(false);
    connect(problems_, &QTableWidget::cellDoubleClicked, this, &MainWindow::onProblemActivated);

    output_ = new QPlainTextEdit(bottomSplit);
    output_->setReadOnly(true);
    output_->setMinimumHeight(72);
    output_->setPlaceholderText(tr("Run / format log…"));
    configureEditor(output_);

    bottomSplit->addWidget(problems_);
    bottomSplit->addWidget(output_);
    bottomSplit->setStretchFactor(0, 3);
    bottomSplit->setStretchFactor(1, 2);
    bottomSplit->setSizes({160, 120});

    bottomLay->addWidget(diagHeader_, 0);
    bottomLay->addWidget(bottomSplit, 1);

    mainVerticalSplit_->addWidget(tabs_);
    mainVerticalSplit_->addWidget(bottomPane);
    mainVerticalSplit_->setStretchFactor(0, 1);
    mainVerticalSplit_->setStretchFactor(1, 0);
    mainVerticalSplit_->setCollapsible(0, false);
    mainVerticalSplit_->setCollapsible(1, true);
    mainVerticalSplit_->setSizes({560, 260});

    rightOuter->addWidget(mainVerticalSplit_, 1);

    hSplit->addWidget(tree_);
    hSplit->addWidget(rightColumn);
    hSplit->setStretchFactor(0, 0);
    hSplit->setStretchFactor(1, 1);
    hSplit->setSizes({280, 1080});

    layout->addWidget(hSplit);
    setCentralWidget(root);

    statusBar()->showMessage(tr("Ready"));

    process_ = new QProcess(this);
    connect(process_, &QProcess::readyReadStandardOutput, this, &MainWindow::onProcessOutput);
    connect(process_, &QProcess::readyReadStandardError, this, &MainWindow::onProcessOutput);
    connect(process_, qOverload<int, QProcess::ExitStatus>(&QProcess::finished), this, &MainWindow::onProcessFinished);
    connect(process_, &QProcess::errorOccurred, this, &MainWindow::onProcessError);

    newTab();
}

void MainWindow::onTabChanged(int) {
    const QString cur = currentFilePath();
    const QString canon = cur.isEmpty() ? QString() : QFileInfo(cur).canonicalFilePath();
    if (!checkTargetCanonical_.isEmpty() && !canon.isEmpty() &&
        QString::compare(canon, checkTargetCanonical_, Qt::CaseInsensitive) != 0) {
        problems_->setRowCount(0);
        diagnosticsClearedByTabSwitch_ = true;
        diagHeader_->setText(tr("Diagnostics cleared — active file is not the last Check target."));
    } else if (diagnosticsClearedByTabSwitch_ && !checkTargetCanonical_.isEmpty() && !canon.isEmpty() &&
               QString::compare(canon, checkTargetCanonical_, Qt::CaseInsensitive) == 0) {
        diagHeader_->setText(tr("Diagnostics were cleared while another tab was active — click Check to refresh."));
    }
    updateStatusForCurrentTab();
}

void MainWindow::updateStatusForCurrentTab() {
    const QString kb = locateKernBinary();
    const QString p = currentFilePath();
    if (p.isEmpty())
        statusBar()->showMessage(tr("kern: %1 — unsaved buffer").arg(kb));
    else
        statusBar()->showMessage(tr("%1 — kern: %2").arg(p, kb));
}

QPlainTextEdit* MainWindow::currentEditor() const {
    return qobject_cast<QPlainTextEdit*>(tabs_->currentWidget());
}

QString MainWindow::currentFilePath() const {
    auto* ed = currentEditor();
    if (!ed) return {};
    return editorPaths_.value(ed);
}

void MainWindow::setCurrentFilePath(const QString& filePath) {
    auto* ed = currentEditor();
    if (!ed) return;
    editorPaths_[ed] = filePath;
    const QString label = filePath.isEmpty() ? QStringLiteral("untitled.kn") : QFileInfo(filePath).fileName();
    tabs_->setTabText(tabs_->currentIndex(), label);
    updateStatusForCurrentTab();
}

void MainWindow::openFolder() {
    const QString dir = QFileDialog::getExistingDirectory(this, tr("Open Kern workspace"),
        workspaceRoot_.isEmpty() ? QDir::currentPath() : workspaceRoot_);
    if (dir.isEmpty()) return;
    workspaceRoot_ = dir;
    tree_->setRootIndex(fsModel_->setRootPath(dir));
}

void MainWindow::openFileFromTree(const QModelIndex& idx) {
    const QString p = fsModel_->filePath(idx);
    const QFileInfo fi(p);
    if (!fi.isFile()) return;
    openFile(p);
}

void MainWindow::openFile(const QString& filePath) {
    QFile f(filePath);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QMessageBox::warning(this, tr("Open failed"), tr("Cannot open %1").arg(filePath));
        return;
    }
    QTextStream ts(&f);
    ensureEditorForPath(filePath, ts.readAll(), true);
}

void MainWindow::ensureEditorForPath(const QString& filePath, const QString& content, bool forceReload) {
    for (int i = 0; i < tabs_->count(); ++i) {
        auto* ed = qobject_cast<QPlainTextEdit*>(tabs_->widget(i));
        if (!ed) continue;
        if (sameFilePath(editorPaths_.value(ed), filePath)) {
            tabs_->setCurrentIndex(i);
            if (forceReload)
                ed->setPlainText(content);
            return;
        }
    }
    auto* ed = new QPlainTextEdit(this);
    configureEditor(ed);
    ed->setPlainText(content);
    new SplHighlighter(ed->document());
    const int idx = tabs_->addTab(ed, QFileInfo(filePath).fileName());
    tabs_->setCurrentIndex(idx);
    editorPaths_[ed] = filePath;
    updateStatusForCurrentTab();
}

void MainWindow::newTab() {
    auto* ed = new QPlainTextEdit(this);
    configureEditor(ed);
    new SplHighlighter(ed->document());
    const int idx = tabs_->addTab(ed, QStringLiteral("untitled.kn"));
    tabs_->setCurrentIndex(idx);
    editorPaths_[ed] = {};
    updateStatusForCurrentTab();
}

void MainWindow::saveCurrent() {
    auto* ed = currentEditor();
    if (!ed) return;
    QString p = currentFilePath();
    if (p.isEmpty()) {
        p = QFileDialog::getSaveFileName(this, tr("Save Kern file"),
            workspaceRoot_.isEmpty() ? QDir::currentPath() : workspaceRoot_,
            tr("Kern files (*.kn);;All files (*)"));
        if (p.isEmpty()) return;
    }
    QFile f(p);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::warning(this, tr("Save failed"), tr("Cannot save %1").arg(p));
        return;
    }
    QTextStream ts(&f);
    ts << ed->toPlainText();
    editorPaths_[ed] = p;
    setCurrentFilePath(p);
}

void MainWindow::saveCurrentAs() {
    auto* ed = currentEditor();
    if (!ed) return;
    const QString p = QFileDialog::getSaveFileName(this, tr("Save Kern file as"),
        workspaceRoot_.isEmpty() ? QDir::currentPath() : workspaceRoot_,
        tr("Kern files (*.kn);;All files (*)"));
    if (p.isEmpty()) return;
    QFile f(p);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::warning(this, tr("Save failed"), tr("Cannot save %1").arg(p));
        return;
    }
    QTextStream ts(&f);
    ts << ed->toPlainText();
    editorPaths_[ed] = p;
    setCurrentFilePath(p);
}

QString MainWindow::locateKernBinary() const {
    const QString fromEnv = qEnvironmentVariable("KERN_EXE");
    if (!fromEnv.isEmpty() && QFileInfo::exists(fromEnv))
        return QDir::cleanPath(fromEnv);

    const QString exe = QSysInfo::productType() == QStringLiteral("windows") ? QStringLiteral("kern.exe") : QStringLiteral("kern");
    const QString appDir = QCoreApplication::applicationDirPath();
    const QString cwd = QDir::currentPath();

    const QStringList candidates = {
        cwd + QStringLiteral("/build/Release/") + exe,
        cwd + QStringLiteral("/build/Debug/") + exe,
        cwd + QStringLiteral("/BUILD/bin/") + exe,
        cwd + QStringLiteral("/build/") + exe,
        appDir + QStringLiteral("/") + exe,
        appDir + QStringLiteral("/../Release/") + exe,
        appDir + QStringLiteral("/../Debug/") + exe,
        exe
    };
    for (const QString& c : candidates) {
        if (QFileInfo::exists(c))
            return QDir::cleanPath(c);
    }
    return exe;
}

bool MainWindow::startKernProcess(const QStringList& args, ProcessKind kind) {
    if (process_->state() != QProcess::NotRunning) {
        output_->appendPlainText(tr("(kern is still running — wait for it to finish)\n"));
        return false;
    }
    processKind_ = kind;
    checkJsonBuffer_.clear();

    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert(QStringLiteral("NO_COLOR"), QStringLiteral("1"));
    process_->setProcessEnvironment(env);

    const QString bin = locateKernBinary();
    const QString wd = workspaceRoot_.isEmpty()
        ? (currentFilePath().isEmpty() ? QDir::currentPath() : QFileInfo(currentFilePath()).absolutePath())
        : workspaceRoot_;
    process_->setWorkingDirectory(wd);
    process_->start(bin, args);
    return true;
}

void MainWindow::runCurrent() {
    saveCurrent();
    const QString p = currentFilePath();
    if (p.isEmpty()) return;
    output_->appendPlainText(QStringLiteral("$ run %1\n").arg(p));
    startKernProcess({QFileInfo(p).absoluteFilePath()}, ProcessKind::Run);
}

void MainWindow::checkCurrent() {
    saveCurrent();
    const QString p = currentFilePath();
    if (p.isEmpty()) return;
    const QString abs = QFileInfo(p).canonicalFilePath();
    if (abs.isEmpty()) {
        output_->appendPlainText(tr("Check: save the buffer to a file on disk first.\n"));
        return;
    }
    checkTargetCanonical_ = abs;
    problems_->setRowCount(0);
    diagHeader_->setText(tr("Checking %1…").arg(abs));
    output_->appendPlainText(QStringLiteral("$ check --json %1\n").arg(abs));
    if (!startKernProcess({QStringLiteral("--check"), QStringLiteral("--json"), abs}, ProcessKind::CheckJson))
        diagHeader_->setText(tr("Diagnostics — check failed to start."));
}

void MainWindow::formatCurrent() {
    saveCurrent();
    const QString p = currentFilePath();
    if (p.isEmpty()) return;
    const QString abs = QFileInfo(p).absoluteFilePath();
    output_->appendPlainText(QStringLiteral("$ fmt %1\n").arg(abs));
    startKernProcess({QStringLiteral("--fmt"), abs}, ProcessKind::Format);
}

void MainWindow::applyCheckJsonResults(const QByteArray& jsonUtf8) {
    diagnosticsClearedByTabSwitch_ = false;
    problems_->setRowCount(0);
    QJsonParseError perr{};
    const QJsonDocument doc = QJsonDocument::fromJson(jsonUtf8.trimmed(), &perr);
    if (!doc.isObject()) {
        output_->appendPlainText(tr("Check JSON parse error: %1\n").arg(perr.errorString()));
        diagHeader_->setText(tr("Diagnostics — invalid JSON from kern."));
        return;
    }
    const QJsonObject root = doc.object();
    const int errN = root.value(QStringLiteral("errors")).toInt();
    const int warnN = root.value(QStringLiteral("warnings")).toInt();
    const QJsonArray items = root.value(QStringLiteral("items")).toArray();
    problems_->setRowCount(items.size());
    for (int i = 0; i < items.size(); ++i) {
        const QJsonObject it = items.at(i).toObject();
        const QString kind = it.value(QStringLiteral("kind")).toString();
        const QString code = it.value(QStringLiteral("code")).toString();
        const int line = it.value(QStringLiteral("line")).toInt();
        const QString msg = it.value(QStringLiteral("message")).toString();
        const QString file = it.value(QStringLiteral("filename")).toString();

        auto* k = new QTableWidgetItem(kind.isEmpty() ? QStringLiteral("error") : kind);
        k->setData(Qt::UserRole, file);
        k->setData(Qt::UserRole + 1, line);
        k->setData(Qt::UserRole + 2, it.value(QStringLiteral("column")).toInt());
        problems_->setItem(i, 0, k);
        problems_->setItem(i, 1, new QTableWidgetItem(code));
        problems_->setItem(i, 2, new QTableWidgetItem(line > 0 ? QString::number(line) : QString()));
        problems_->setItem(i, 3, new QTableWidgetItem(msg));
    }
    problems_->resizeColumnsToContents();
    if (problems_->columnWidth(3) > 520)
        problems_->setColumnWidth(3, 520);
    diagHeader_->setText(tr("Diagnostics for %1 — %2 error(s), %3 warning(s), %4 item(s).")
        .arg(checkTargetCanonical_).arg(errN).arg(warnN).arg(items.size()));
}

void MainWindow::onProblemActivated(int row, int) {
    QTableWidgetItem* k = problems_->item(row, 0);
    if (!k) return;
    const QString fileField = k->data(Qt::UserRole).toString();
    const int line = k->data(Qt::UserRole + 1).toInt();

    QString targetCanon;
    if (fileField.isEmpty())
        targetCanon = checkTargetCanonical_;
    else
        targetCanon = QFileInfo(fileField).canonicalFilePath();

    if (targetCanon.isEmpty())
        return;

    for (int i = 0; i < tabs_->count(); ++i) {
        auto* ed = qobject_cast<QPlainTextEdit*>(tabs_->widget(i));
        if (!ed) continue;
        const QString tabPath = editorPaths_.value(ed);
        if (tabPath.isEmpty()) continue;
        if (!sameFilePath(tabPath, targetCanon)) continue;
        tabs_->setCurrentIndex(i);
        jumpEditorToLine(ed, line);
        return;
    }

    openFile(targetCanon);
    jumpEditorToLine(currentEditor(), line);
}

void MainWindow::onProcessOutput() {
    if (processKind_ == ProcessKind::CheckJson) {
        checkJsonBuffer_ += process_->readAllStandardOutput();
        const QByteArray err = process_->readAllStandardError();
        if (!err.isEmpty()) {
            output_->moveCursor(QTextCursor::End);
            output_->insertPlainText(QString::fromUtf8(err));
            output_->moveCursor(QTextCursor::End);
        }
        return;
    }
    output_->moveCursor(QTextCursor::End);
    output_->insertPlainText(QString::fromUtf8(process_->readAllStandardOutput()));
    output_->insertPlainText(QString::fromUtf8(process_->readAllStandardError()));
    output_->moveCursor(QTextCursor::End);
}

void MainWindow::onProcessError(QProcess::ProcessError e) {
    QString why;
    switch (e) {
        case QProcess::FailedToStart:
            why = tr("Failed to start (kern not found — set KERN_EXE or build kern).");
            break;
        case QProcess::Crashed:
            why = tr("Process crashed.");
            break;
        case QProcess::Timedout:
            why = tr("Timed out.");
            break;
        case QProcess::WriteError:
        case QProcess::ReadError:
            why = tr("I/O error.");
            break;
        default:
            why = tr("Unknown error.");
            break;
    }
    output_->appendPlainText(tr("[kern] %1\n").arg(why));
    if (processKind_ == ProcessKind::CheckJson)
        diagHeader_->setText(tr("Diagnostics — kern process failed."));
    checkJsonBuffer_.clear();
    processKind_ = ProcessKind::None;
}

void MainWindow::onProcessFinished(int exitCode, QProcess::ExitStatus) {
    if (processKind_ == ProcessKind::CheckJson) {
        checkJsonBuffer_ += process_->readAllStandardOutput();
        const QByteArray errTail = process_->readAllStandardError();
        if (!errTail.isEmpty())
            output_->appendPlainText(QString::fromUtf8(errTail));
        applyCheckJsonResults(checkJsonBuffer_);
        output_->appendPlainText(tr("[check finished, exit %1]\n").arg(exitCode));
        processKind_ = ProcessKind::None;
        return;
    }
    if (processKind_ == ProcessKind::Format) {
        if (exitCode == 0) {
            const QString p = currentFilePath();
            if (!p.isEmpty()) {
                QFile f(p);
                if (f.open(QIODevice::ReadOnly | QIODevice::Text)) {
                    QTextStream ts(&f);
                    auto* ed = currentEditor();
                    if (ed)
                        ed->setPlainText(ts.readAll());
                }
            }
            output_->appendPlainText(tr("[format finished]\n"));
        } else {
            output_->appendPlainText(tr("[format failed, exit %1]\n").arg(exitCode));
        }
        processKind_ = ProcessKind::None;
        return;
    }
    output_->appendPlainText(tr("[process exited with code %1]\n").arg(exitCode));
    processKind_ = ProcessKind::None;
}
