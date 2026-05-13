#pragma once

#include <QByteArray>
#include <QMainWindow>
#include <QHash>
#include <QProcess>

class QFileSystemModel;
class QTreeView;
class QTabWidget;
class QPlainTextEdit;
class QTableWidget;
class QLabel;
class QSplitter;

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    MainWindow();

private slots:
    void openFolder();
    void openFileFromTree(const QModelIndex& idx);
    void onTabChanged(int index);
    void newTab();
    void saveCurrent();
    void saveCurrentAs();
    void runCurrent();
    void checkCurrent();
    void formatCurrent();
    void onProcessOutput();
    void onProcessFinished(int exitCode, QProcess::ExitStatus status);
    void onProcessError(QProcess::ProcessError error);
    void onProblemActivated(int row, int column);

private:
    enum class ProcessKind { None, Run, CheckJson, Format };

    static bool sameFilePath(const QString& a, const QString& b);

    QPlainTextEdit* currentEditor() const;
    void configureEditor(QPlainTextEdit* ed) const;
    QString currentFilePath() const;
    void setCurrentFilePath(const QString& filePath);
    void updateStatusForCurrentTab();
    void openFile(const QString& filePath);
    /* * opens or focuses a tab; if \a content is non-empty and new tab, sets text. If tab exists, only switches unless \a forceReload.*/
    void ensureEditorForPath(const QString& filePath, const QString& content, bool forceReload = false);
    QString locateKernBinary() const;
    bool startKernProcess(const QStringList& args, ProcessKind kind);
    void applyCheckJsonResults(const QByteArray& jsonUtf8);
    void createUi();

    QFileSystemModel* fsModel_ = nullptr;
    QTreeView* tree_ = nullptr;
    QTabWidget* tabs_ = nullptr;
    QSplitter* mainVerticalSplit_ = nullptr;
    QTableWidget* problems_ = nullptr;
    QLabel* diagHeader_ = nullptr;
    QPlainTextEdit* output_ = nullptr;
    QProcess* process_ = nullptr;
    QString workspaceRoot_;
    QHash<QPlainTextEdit*, QString> editorPaths_;

    ProcessKind processKind_ = ProcessKind::None;
    QByteArray checkJsonBuffer_;
    QString checkTargetCanonical_;
    bool diagnosticsClearedByTabSwitch_ = false;
};
