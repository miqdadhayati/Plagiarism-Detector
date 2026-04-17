#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QTextEdit>
#include <QListWidget>
#include <QSlider>
#include <QLabel>
#include <QPushButton>
#include <QLineEdit>
#include <QProgressBar>
#include <QThread>
#include <QMutex>
#include <QTimer>
#include <QStringList>

#include "engine.h"

// Worker thread for background scans.
class ScanWorker : public QThread {
    Q_OBJECT
public:
    ScanWorker(const PlagiarismEngine* engine,
               const std::string& text,
               const std::string& filename,
               double radius);

    void run() override;
    QStringList traceLines() const { return traceLines_; }

signals:
    void scanComplete(ScanReport report);

private:
    const PlagiarismEngine* engine_;
    std::string             text_;
    std::string             filename_;
    double                  radius_;
    QStringList             traceLines_;
};

// Main GUI window.
class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

private slots:
    void onAddDatabaseFile();
    void onRemoveDatabaseFile();
    void onScanFile();
    void onLoadQuickDemo();
    void onVisualizeOperationsDemo();

    void onAddBoilerplatePhrase();
    void onRemoveBoilerplatePhrase();
    void onAddWhitelistWord();
    void onRemoveWhitelistWord();

    void onStrictnessChanged(int value);

    void onScanFinished(ScanReport report);

    void onRescan();

    void onTracePlaybackTick();
    void onTracePlayPause();
    void onTraceNextStep();
    void onTraceSpeedChanged(int value);

private:
    void setupUI();
    void setupConnections();
    void updateDatabaseView();
    void updateBoilerplateView();
    void updateWhitelistView();
    void updateTreeStats();
    void applyHeatmap(const ScanReport& report);
    void startTracePlayback(const QStringList& lines);
    QString explainTraceLine(const QString& line) const;
    void appendTraceStep(int index);

    PlagiarismEngine engine_;
    std::string      currentQueryText_;
    std::string      currentQueryFile_;
    ScanWorker*      scanWorker_;


    QListWidget*  databaseList_;
    QPushButton*  addFileBtn_;
    QPushButton*  removeFileBtn_;
    QPushButton*  quickDemoBtn_;
    QPushButton*  opsDemoBtn_;
    QListWidget*  boilerplateList_;
    QLineEdit*    boilerplateInput_;
    QPushButton*  addBoilerplateBtn_;
    QPushButton*  removeBoilerplateBtn_;
    QListWidget*  whitelistList_;
    QLineEdit*    whitelistInput_;
    QPushButton*  addWhitelistBtn_;
    QPushButton*  removeWhitelistBtn_;
    QLabel*       treeStatsLabel_;

    QTextEdit*    heatmapDisplay_;
    QPushButton*  scanBtn_;
    QLabel*       scanFileLabel_;

    QSlider*      strictnessSlider_;
    QLabel*       strictnessLabel_;
    QLabel*       matchPercentLabel_;
    QListWidget*  matchFilesList_;
    QProgressBar* progressBar_;
    QTextEdit*    opsTraceDisplay_;
    QProgressBar* opsTraceProgress_;
    QTextEdit*    opsExplainDisplay_;
    QPushButton*  opsPlayPauseBtn_;
    QPushButton*  opsNextStepBtn_;
    QSlider*      opsSpeedSlider_;
    QLabel*       opsSpeedLabel_;
    QLabel*       opsStepLabel_;

    QTimer*       opsPlaybackTimer_;
    QStringList   opsTraceLines_;
    QStringList   opsTraceExplainLines_;
    int           opsTraceIndex_;
    bool          opsPlaybackPaused_;
};

#endif // MAINWINDOW_H
