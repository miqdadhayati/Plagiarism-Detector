#include "mainwindow.hpp"

#include <QApplication>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QSplitter>
#include <QGroupBox>
#include <QMessageBox>
#include <QTextCharFormat>
#include <QTextCursor>
#include <QFont>
#include <QColor>
#include <QPalette>
#include <QStyle>
#include <QStatusBar>
#include <QMenuBar>
#include <QToolBar>

// ============================================================================
// ScanWorker
// ============================================================================

ScanWorker::ScanWorker(const PlagiarismEngine* engine,
                       const std::string& text,
                       const std::string& filename,
                       double radius)
    : engine_(engine), text_(text), filename_(filename), radius_(radius) {}

void ScanWorker::run() {
    ScanReport report = engine_->scan(text_, filename_, radius_);
    emit scanComplete(report);
}

// ============================================================================
// MainWindow — Construction
// ============================================================================

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
    , scanWorker_(nullptr)
{
    setupUI();
    setupConnections();
    updateTreeStats();

    setWindowTitle("VP-Tree Plagiarism Detector");
    resize(1280, 800);

    statusBar()->showMessage("Ready — Add database files and scan a document.");
}

MainWindow::~MainWindow() {
    if (scanWorker_ && scanWorker_->isRunning()) {
        scanWorker_->wait();
    }
    delete scanWorker_;
}

// ============================================================================
// UI SETUP
// ============================================================================

void MainWindow::setupUI() {
    // ---- Global stylesheet ----
    setStyleSheet(R"(
        QMainWindow {
            background-color: #1e1e2e;
            color: #cdd6f4;
        }
        QGroupBox {
            font-weight: bold;
            font-size: 13px;
            color: #89b4fa;
            border: 1px solid #45475a;
            border-radius: 6px;
            margin-top: 12px;
            padding-top: 16px;
        }
        QGroupBox::title {
            subcontrol-origin: margin;
            left: 12px;
            padding: 0 6px;
        }
        QPushButton {
            background-color: #313244;
            color: #cdd6f4;
            border: 1px solid #45475a;
            border-radius: 5px;
            padding: 6px 14px;
            font-size: 12px;
            min-height: 20px;
        }
        QPushButton:hover {
            background-color: #45475a;
            border-color: #89b4fa;
        }
        QPushButton:pressed {
            background-color: #585b70;
        }
        QPushButton#scanBtn {
            background-color: #89b4fa;
            color: #1e1e2e;
            font-weight: bold;
            font-size: 14px;
            padding: 10px 24px;
        }
        QPushButton#scanBtn:hover {
            background-color: #74c7ec;
        }
        QListWidget {
            background-color: #181825;
            color: #cdd6f4;
            border: 1px solid #45475a;
            border-radius: 4px;
            font-size: 12px;
            padding: 4px;
        }
        QListWidget::item {
            padding: 4px 8px;
            border-radius: 3px;
        }
        QListWidget::item:selected {
            background-color: #45475a;
            color: #89b4fa;
        }
        QTextEdit {
            background-color: #11111b;
            color: #cdd6f4;
            border: 1px solid #45475a;
            border-radius: 6px;
            font-family: 'Consolas', 'Courier New', monospace;
            font-size: 13px;
            padding: 8px;
            selection-background-color: #45475a;
        }
        QLineEdit {
            background-color: #181825;
            color: #cdd6f4;
            border: 1px solid #45475a;
            border-radius: 4px;
            padding: 5px 8px;
            font-size: 12px;
        }
        QLineEdit:focus {
            border-color: #89b4fa;
        }
        QSlider::groove:horizontal {
            height: 6px;
            background: #45475a;
            border-radius: 3px;
        }
        QSlider::handle:horizontal {
            background: #89b4fa;
            border: none;
            width: 18px;
            height: 18px;
            margin: -6px 0;
            border-radius: 9px;
        }
        QSlider::sub-page:horizontal {
            background: #89b4fa;
            border-radius: 3px;
        }
        QLabel {
            color: #cdd6f4;
            font-size: 12px;
        }
        QLabel#matchLabel {
            font-size: 28px;
            font-weight: bold;
            color: #a6e3a1;
            padding: 10px;
        }
        QLabel#sectionTitle {
            font-size: 11px;
            color: #6c7086;
            font-weight: bold;
            text-transform: uppercase;
            letter-spacing: 1px;
        }
        QProgressBar {
            background-color: #313244;
            border: none;
            border-radius: 3px;
            height: 4px;
            text-align: center;
        }
        QProgressBar::chunk {
            background-color: #89b4fa;
            border-radius: 3px;
        }
        QStatusBar {
            background-color: #181825;
            color: #6c7086;
            font-size: 11px;
        }
    )");

    QWidget* central = new QWidget(this);
    setCentralWidget(central);

    QHBoxLayout* mainLayout = new QHBoxLayout(central);
    mainLayout->setSpacing(0);
    mainLayout->setContentsMargins(0, 0, 0, 0);

    QSplitter* splitter = new QSplitter(Qt::Horizontal, central);
    mainLayout->addWidget(splitter);

    // ======================= LEFT SIDEBAR =======================
    QWidget* leftPanel = new QWidget();
    leftPanel->setMinimumWidth(260);
    leftPanel->setMaximumWidth(380);
    QVBoxLayout* leftLayout = new QVBoxLayout(leftPanel);
    leftLayout->setContentsMargins(12, 12, 6, 12);
    leftLayout->setSpacing(8);

    // --- Database Files Group ---
    QGroupBox* dbGroup = new QGroupBox("Database Files");
    QVBoxLayout* dbLayout = new QVBoxLayout(dbGroup);

    databaseList_ = new QListWidget();
    databaseList_->setMinimumHeight(120);
    dbLayout->addWidget(databaseList_);

    QHBoxLayout* dbBtnLayout = new QHBoxLayout();
    addFileBtn_    = new QPushButton("+ Add File");
    removeFileBtn_ = new QPushButton("− Remove");
    dbBtnLayout->addWidget(addFileBtn_);
    dbBtnLayout->addWidget(removeFileBtn_);
    dbLayout->addLayout(dbBtnLayout);

    leftLayout->addWidget(dbGroup);

    // --- Tree Stats ---
    treeStatsLabel_ = new QLabel("Tree: empty");
    treeStatsLabel_->setObjectName("sectionTitle");
    leftLayout->addWidget(treeStatsLabel_);

    // --- Whitelist / Boilerplate Group ---
    QGroupBox* wlGroup = new QGroupBox("Boilerplate & Whitelist");
    QVBoxLayout* wlLayout = new QVBoxLayout(wlGroup);

    whitelistList_ = new QListWidget();
    whitelistList_->setMaximumHeight(100);
    wlLayout->addWidget(whitelistList_);

    QHBoxLayout* wlInputLayout = new QHBoxLayout();
    whitelistInput_ = new QLineEdit();
    whitelistInput_->setPlaceholderText("Enter word/phrase to ignore...");
    addWhitelistBtn_ = new QPushButton("+");
    addWhitelistBtn_->setMaximumWidth(36);
    wlInputLayout->addWidget(whitelistInput_);
    wlInputLayout->addWidget(addWhitelistBtn_);
    wlLayout->addLayout(wlInputLayout);

    removeWhitelistBtn_ = new QPushButton("Remove Selected");
    wlLayout->addWidget(removeWhitelistBtn_);

    leftLayout->addWidget(wlGroup);

    // --- Strictness Slider ---
    QGroupBox* sliderGroup = new QGroupBox("Detection Strictness");
    QVBoxLayout* sliderLayout = new QVBoxLayout(sliderGroup);

    strictnessSlider_ = new QSlider(Qt::Horizontal);
    strictnessSlider_->setRange(1, 80);   // radius = value / 100.0
    strictnessSlider_->setValue(30);
    sliderLayout->addWidget(strictnessSlider_);

    strictnessLabel_ = new QLabel("Radius: 0.30  (lower = stricter)");
    sliderLayout->addWidget(strictnessLabel_);

    leftLayout->addWidget(sliderGroup);

    leftLayout->addStretch();
    splitter->addWidget(leftPanel);

    // ======================= CENTER PANEL =======================
    QWidget* centerPanel = new QWidget();
    QVBoxLayout* centerLayout = new QVBoxLayout(centerPanel);
    centerLayout->setContentsMargins(6, 12, 6, 12);
    centerLayout->setSpacing(8);

    // Scan controls
    QHBoxLayout* scanBarLayout = new QHBoxLayout();

    scanBtn_ = new QPushButton("Scan Document");
    scanBtn_->setObjectName("scanBtn");
    scanBarLayout->addWidget(scanBtn_);

    scanFileLabel_ = new QLabel("No file selected");
    scanFileLabel_->setObjectName("sectionTitle");
    scanBarLayout->addWidget(scanFileLabel_, 1);

    centerLayout->addLayout(scanBarLayout);

    progressBar_ = new QProgressBar();
    progressBar_->setMaximum(0);  // indeterminate
    progressBar_->setVisible(false);
    progressBar_->setFixedHeight(4);
    centerLayout->addWidget(progressBar_);

    // Heatmap text display
    QLabel* heatmapTitle = new QLabel("DOCUMENT HEATMAP");
    heatmapTitle->setObjectName("sectionTitle");
    centerLayout->addWidget(heatmapTitle);

    heatmapDisplay_ = new QTextEdit();
    heatmapDisplay_->setReadOnly(true);
    heatmapDisplay_->setPlaceholderText(
        "Scanned document will appear here.\n\n"
        "Red highlighted text = potential plagiarism detected.\n"
        "White text = original content.");
    centerLayout->addWidget(heatmapDisplay_, 1);

    splitter->addWidget(centerPanel);

    // ======================= RIGHT PANEL =======================
    QWidget* rightPanel = new QWidget();
    rightPanel->setMinimumWidth(220);
    rightPanel->setMaximumWidth(340);
    QVBoxLayout* rightLayout = new QVBoxLayout(rightPanel);
    rightLayout->setContentsMargins(6, 12, 12, 12);
    rightLayout->setSpacing(8);

    // Match report
    QGroupBox* reportGroup = new QGroupBox("Match Report");
    QVBoxLayout* reportLayout = new QVBoxLayout(reportGroup);

    matchPercentLabel_ = new QLabel("0.0%");
    matchPercentLabel_->setObjectName("matchLabel");
    matchPercentLabel_->setAlignment(Qt::AlignCenter);
    reportLayout->addWidget(matchPercentLabel_);

    QLabel* matchTitle = new QLabel("TOTAL MATCH");
    matchTitle->setObjectName("sectionTitle");
    matchTitle->setAlignment(Qt::AlignCenter);
    reportLayout->addWidget(matchTitle);

    rightLayout->addWidget(reportGroup);

    // Source files list
    QGroupBox* sourcesGroup = new QGroupBox("Matched Sources");
    QVBoxLayout* sourcesLayout = new QVBoxLayout(sourcesGroup);

    matchFilesList_ = new QListWidget();
    sourcesLayout->addWidget(matchFilesList_);

    rightLayout->addWidget(sourcesGroup, 1);

    splitter->addWidget(rightPanel);

    // Splitter proportions
    splitter->setStretchFactor(0, 1);   // left
    splitter->setStretchFactor(1, 3);   // center
    splitter->setStretchFactor(2, 1);   // right
}

// ============================================================================
// SIGNAL / SLOT CONNECTIONS
// ============================================================================

void MainWindow::setupConnections() {
    connect(addFileBtn_,        &QPushButton::clicked,
            this,               &MainWindow::onAddDatabaseFile);
    connect(removeFileBtn_,     &QPushButton::clicked,
            this,               &MainWindow::onRemoveDatabaseFile);
    connect(scanBtn_,           &QPushButton::clicked,
            this,               &MainWindow::onScanFile);

    connect(addWhitelistBtn_,   &QPushButton::clicked,
            this,               &MainWindow::onAddWhitelistWord);
    connect(removeWhitelistBtn_,&QPushButton::clicked,
            this,               &MainWindow::onRemoveWhitelistWord);

    connect(strictnessSlider_,  &QSlider::valueChanged,
            this,               &MainWindow::onStrictnessChanged);
}

// ============================================================================
// SLOT IMPLEMENTATIONS
// ============================================================================

void MainWindow::onAddDatabaseFile() {
    QStringList files = QFileDialog::getOpenFileNames(
        this, "Select Database Files", QString(),
        "Text Files (*.txt);;All Files (*)");

    for (const QString& f : files) {
        bool ok = engine_.addFile(f.toStdString());
        if (!ok) {
            statusBar()->showMessage("Failed to add: " + f, 3000);
        }
    }

    updateDatabaseView();
    updateTreeStats();
    statusBar()->showMessage(
        QString("Database: %1 files, %2 n-grams indexed")
            .arg(engine_.indexedFiles().count())
            .arg(engine_.treeSize()),
        5000);
}

void MainWindow::onRemoveDatabaseFile() {
    int row = databaseList_->currentRow();
    if (row < 0) return;

    engine_.removeFile(row);
    updateDatabaseView();
    updateTreeStats();
    statusBar()->showMessage("File removed, tree rebuilt.", 3000);
}

void MainWindow::onScanFile() {
    QString file = QFileDialog::getOpenFileName(
        this, "Select Document to Scan", QString(),
        "Text Files (*.txt);;All Files (*)");

    if (file.isEmpty()) return;

    currentQueryText_ = PlagiarismEngine::readFile(file.toStdString());
    if (currentQueryText_.empty()) {
        QMessageBox::warning(this, "Error", "Could not read the file.");
        return;
    }

    // Extract filename
    currentQueryFile_ = file.toStdString();
    size_t sep = currentQueryFile_.find_last_of("/\\");
    std::string fname = (sep != std::string::npos)
        ? currentQueryFile_.substr(sep + 1) : currentQueryFile_;

    scanFileLabel_->setText(QString::fromStdString("Scanning: " + fname));

    if (engine_.treeEmpty()) {
        // Show text but no matches
        heatmapDisplay_->setPlainText(
            QString::fromStdString(currentQueryText_));
        matchPercentLabel_->setText("N/A");
        statusBar()->showMessage(
            "No database files indexed. Add files first.", 5000);
        return;
    }

    // Launch scan on background thread
    double radius = strictnessSlider_->value() / 100.0;

    progressBar_->setVisible(true);
    scanBtn_->setEnabled(false);

    if (scanWorker_) {
        scanWorker_->wait();
        delete scanWorker_;
    }

    scanWorker_ = new ScanWorker(&engine_, currentQueryText_,
                                  fname, radius);
    connect(scanWorker_, &ScanWorker::scanComplete,
            this,        &MainWindow::onScanFinished);
    scanWorker_->start();
}

void MainWindow::onScanFinished(ScanReport report) {
    progressBar_->setVisible(false);
    scanBtn_->setEnabled(true);

    applyHeatmap(report);

    // Update match percentage
    QString pctText = QString::number(report.matchPercentage, 'f', 1) + "%";
    matchPercentLabel_->setText(pctText);

    // Color the percentage label based on severity
    if (report.matchPercentage > 50.0)
        matchPercentLabel_->setStyleSheet(
            "font-size:28px; font-weight:bold; color:#f38ba8; padding:10px;");
    else if (report.matchPercentage > 25.0)
        matchPercentLabel_->setStyleSheet(
            "font-size:28px; font-weight:bold; color:#fab387; padding:10px;");
    else
        matchPercentLabel_->setStyleSheet(
            "font-size:28px; font-weight:bold; color:#a6e3a1; padding:10px;");

    // Update matched files list
    matchFilesList_->clear();
    for (int i = 0; i < report.matchedFiles.count(); ++i) {
        matchFilesList_->addItem(
            QString::fromStdString(report.matchedFiles[i]));
    }

    statusBar()->showMessage(
        QString("Scan complete — %1 match, %2 source(s) found")
            .arg(pctText)
            .arg(report.matchedFiles.count()),
        10000);
}

void MainWindow::onAddWhitelistWord() {
    QString word = whitelistInput_->text().trimmed();
    if (word.isEmpty()) return;

    engine_.addWhitelistWord(word.toStdString());
    whitelistInput_->clear();
    updateWhitelistView();
}

void MainWindow::onRemoveWhitelistWord() {
    int row = whitelistList_->currentRow();
    if (row < 0) return;
    engine_.removeWhitelistWord(row);
    updateWhitelistView();
}

void MainWindow::onStrictnessChanged(int value) {
    double radius = value / 100.0;
    strictnessLabel_->setText(
        QString("Radius: %1  (%2)")
            .arg(radius, 0, 'f', 2)
            .arg(radius <= 0.15 ? "very strict" :
                 radius <= 0.30 ? "strict" :
                 radius <= 0.50 ? "moderate" : "lenient"));

    // Auto re-scan if we have a loaded document
    if (!currentQueryText_.empty() && !engine_.treeEmpty()) {
        onRescan();
    }
}

void MainWindow::onRescan() {
    if (currentQueryText_.empty() || engine_.treeEmpty()) return;
    if (scanWorker_ && scanWorker_->isRunning()) return;

    double radius = strictnessSlider_->value() / 100.0;
    std::string fname = currentQueryFile_;
    size_t sep = fname.find_last_of("/\\");
    if (sep != std::string::npos) fname = fname.substr(sep + 1);

    progressBar_->setVisible(true);
    scanBtn_->setEnabled(false);

    if (scanWorker_) {
        scanWorker_->wait();
        delete scanWorker_;
    }

    scanWorker_ = new ScanWorker(&engine_, currentQueryText_,
                                  fname, radius);
    connect(scanWorker_, &ScanWorker::scanComplete,
            this,        &MainWindow::onScanFinished);
    scanWorker_->start();
}

// ============================================================================
// HEATMAP RENDERING — Highlights plagiarized segments in red
// ============================================================================

void MainWindow::applyHeatmap(const ScanReport& report) {
    heatmapDisplay_->clear();

    QString fullText = QString::fromStdString(currentQueryText_);
    int textLen = fullText.length();

    // Build a per-character match flag array using raw allocation
    bool* isMatched = new bool[textLen + 1];
    for (int i = 0; i <= textLen; ++i)
        isMatched[i] = false;

    for (int s = 0; s < report.segments.count(); ++s) {
        int start = report.segments[s].start;
        int end   = report.segments[s].end;
        if (start < 0) start = 0;
        if (end > textLen) end = textLen;
        for (int c = start; c < end; ++c)
            isMatched[c] = true;
    }

    // Build the document with formatting by walking character-by-character,
    // batching consecutive same-state characters for efficiency.
    QTextCursor cursor(heatmapDisplay_->document());
    cursor.beginEditBlock();

    QTextCharFormat normalFmt;
    normalFmt.setForeground(QColor("#cdd6f4"));
    normalFmt.setBackground(Qt::transparent);

    QTextCharFormat matchFmt;
    matchFmt.setForeground(QColor("#1e1e2e"));
    matchFmt.setBackground(QColor("#f38ba8"));  // Red highlight
    matchFmt.setFontWeight(QFont::Bold);

    int i = 0;
    while (i < textLen) {
        bool state = isMatched[i];
        int start = i;

        // Batch consecutive characters with same state
        while (i < textLen && isMatched[i] == state) ++i;

        QString segment = fullText.mid(start, i - start);
        cursor.insertText(segment, state ? matchFmt : normalFmt);
    }

    cursor.endEditBlock();
    delete[] isMatched;

    // Scroll to top
    cursor.movePosition(QTextCursor::Start);
    heatmapDisplay_->setTextCursor(cursor);
}

// ============================================================================
// VIEW UPDATES
// ============================================================================

void MainWindow::updateDatabaseView() {
    databaseList_->clear();
    const auto& files = engine_.indexedFiles();
    for (int i = 0; i < files.count(); ++i) {
        QString item = QString::fromStdString(files[i].filename)
                       + "  (" + QString::number(files[i].ngramCount) + " n-grams)";
        databaseList_->addItem(item);
    }
}

void MainWindow::updateWhitelistView() {
    whitelistList_->clear();
    const auto& wl = engine_.whitelist();
    for (int i = 0; i < wl.count(); ++i)
        whitelistList_->addItem(QString::fromStdString(wl[i]));
}

void MainWindow::updateTreeStats() {
    if (engine_.treeEmpty()) {
        treeStatsLabel_->setText("VP-Tree: empty");
    } else {
        treeStatsLabel_->setText(
            QString("VP-Tree: %1 nodes, height %2")
                .arg(engine_.treeSize())
                .arg(engine_.treeHeight()));
    }
}
