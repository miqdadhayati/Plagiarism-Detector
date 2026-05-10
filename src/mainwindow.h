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

signals:
    void scanComplete(ScanReport report);

private:
    const PlagiarismEngine* engine_;
    std::string             text_;
    std::string             filename_;
    double                  radius_;
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
    void onLoadSynonymDemo();

    void onAutoDetectBoilerplate();
    void onAddWhitelistWord();
    void onRemoveWhitelistWord();

    void onStrictnessChanged(int value);

    void onScanFinished(ScanReport report);

    void onRescan();

private:
    void setupUI();
    void setupConnections();
    void updateDatabaseView();
    void updateBoilerplateView();
    void updateWhitelistView();
    void updateTreeStats();
    void applyHeatmap(const ScanReport& report);

    PlagiarismEngine engine_;
    std::string      currentQueryText_;
    std::string      currentQueryFile_;
    std::string      currentQueryName_;
    ScanWorker*      scanWorker_;


    QListWidget*  databaseList_;
    QPushButton*  addFileBtn_;
    QPushButton*  removeFileBtn_;
    QPushButton*  quickDemoBtn_;
    QPushButton*  synonymDemoBtn_;
    QListWidget*  boilerplateList_;
    QPushButton*  autoDetectBoilerplateBtn_;
    QListWidget*  whitelistList_;
    QLineEdit*    whitelistInput_;
    QPushButton*  addWhitelistBtn_;
    QPushButton*  removeWhitelistBtn_;
    QLabel*       treeStatsLabel_;
    QLabel*       synonymStatusLabel_;
    QTextEdit*    synonymDemoOutput_;

    QTextEdit*    heatmapDisplay_;
    QPushButton*  scanBtn_;
    QLabel*       scanFileLabel_;

    QSlider*      strictnessSlider_;
    QLabel*       strictnessLabel_;
    QLabel*       matchPercentLabel_;
    QListWidget*  matchFilesList_;
    QProgressBar* progressBar_;
    double        lastScanRadius_;
};

#endif // MAINWINDOW_H
