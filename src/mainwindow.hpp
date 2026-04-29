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

#include "engine.hpp"

// ============================================================================
// ScanWorker — Runs the VP-Tree scan on a separate thread so the GUI
//              remains responsive during heavy searches.
// ============================================================================
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

// ============================================================================
// MainWindow — Primary application window with all required GUI elements.
// ============================================================================
class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

private slots:
    // File operations
    void onAddDatabaseFile();
    void onRemoveDatabaseFile();
    void onScanFile();

    // Whitelist
    void onAddWhitelistWord();
    void onRemoveWhitelistWord();

    // Slider
    void onStrictnessChanged(int value);

    // Scan result
    void onScanFinished(ScanReport report);

    // Re-scan with updated radius
    void onRescan();

private:
    void setupUI();
    void setupConnections();
    void updateDatabaseView();
    void updateWhitelistView();
    void updateTreeStats();
    void applyHeatmap(const ScanReport& report);

    // Engine
    PlagiarismEngine engine_;
    std::string      currentQueryText_;
    std::string      currentQueryFile_;
    ScanWorker*      scanWorker_;

    // ---- GUI Widgets ----

    // Left sidebar
    QListWidget*  databaseList_;
    QPushButton*  addFileBtn_;
    QPushButton*  removeFileBtn_;
    QListWidget*  whitelistList_;
    QLineEdit*    whitelistInput_;
    QPushButton*  addWhitelistBtn_;
    QPushButton*  removeWhitelistBtn_;
    QLabel*       treeStatsLabel_;

    // Center
    QTextEdit*    heatmapDisplay_;
    QPushButton*  scanBtn_;
    QLabel*       scanFileLabel_;

    // Right / Bottom
    QSlider*      strictnessSlider_;
    QLabel*       strictnessLabel_;
    QLabel*       matchPercentLabel_;
    QListWidget*  matchFilesList_;
    QProgressBar* progressBar_;
};

#endif // MAINWINDOW_H
