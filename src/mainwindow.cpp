#include "mainwindow.h"

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
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QPalette>
#include <QStyle>
#include <QStatusBar>
#include <QMenuBar>
#include <QToolBar>
#include <cctype>

namespace {
// Function: findSampleFilePath
QString findSampleFilePath(const QString& filename) {
    QStringList candidates;
    QString cwd = QDir::currentPath();
    QString appDir = QCoreApplication::applicationDirPath();
    candidates << QDir(cwd).filePath("sample_data/" + filename);
    candidates << QDir(cwd).filePath("../sample_data/" + filename);
    candidates << QDir(cwd).filePath("../../sample_data/" + filename);
    candidates << QDir(appDir).filePath("sample_data/" + filename);
    candidates << QDir(appDir).filePath("../sample_data/" + filename);
    candidates << QDir(appDir).filePath("../../sample_data/" + filename);
    candidates << QDir(appDir).filePath("../../../sample_data/" + filename);
    for (const QString& path : candidates) {
        QFileInfo info(path);
        if (info.exists() && info.isFile()) {
            return info.absoluteFilePath();
        }
    }
    return QString();
}

// Function: compactNgramText
QString compactNgramText(const std::string& text, int maxLen = 36) {
    QString q = QString::fromStdString(text);
    if (q.size() <= maxLen) return q;
    return q.left(maxLen - 3) + "...";
}

// Function: appendTreeSnapshotLines
void appendTreeSnapshotLines(QStringList& lines,
                             const VPTreeNode* node,
                             int depth,
                             int maxDepth) {
    QString indent(depth * 2, ' ');
    if (!node) {
        lines << indent + "- null";
        return;
    }
    lines << indent
          + "- vp=\"" + compactNgramText(node->vantagePoint.text)
          + "\" mu=" + QString::number(node->medianDistance, 'f', 2);
    if (depth >= maxDepth) return;
    appendTreeSnapshotLines(lines, node->left, depth + 1, maxDepth);
    appendTreeSnapshotLines(lines, node->right, depth + 1, maxDepth);
}
}

// Function: ScanWorker
ScanWorker::ScanWorker(const PlagiarismEngine* engine,
                       const std::string& text,
                       const std::string& filename,
                       double radius)
    : engine_(engine), text_(text), filename_(filename), radius_(radius) {}
// Function: run
void ScanWorker::run() {
    ScanReport report = engine_->scan(text_, filename_, radius_);
    emit scanComplete(report);
}
// Function: MainWindow
MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
    , scanWorker_(nullptr)
    , lastScanRadius_(0.30)
{
    setupUI();
    setupConnections();

    // Load synonym dictionary.
    QString synPath = findSampleFilePath("synonyms.csv");
    bool synonymsLoaded = false;
    if (!synPath.isEmpty()) {
        synonymsLoaded = engine_.loadSynonymDictionary(synPath.toStdString());
    }
    if (synonymStatusLabel_) {
        synonymStatusLabel_->setText(
            synonymsLoaded
                ? ("Synonym Dictionary: loaded ("
                   + QString::number(engine_.synonymCount()) + " entries)")
                : "Synonym Dictionary: not loaded");
    }

    updateTreeStats();
    setWindowTitle("VP-Tree Plagiarism Detector");
    resize(1280, 800);
    statusBar()->showMessage("Ready — Add database files and scan a document.");
}
// Function: ~MainWindow
MainWindow::~MainWindow() {
    if (scanWorker_ && scanWorker_->isRunning()) {
        scanWorker_->wait();
    }
    delete scanWorker_;
}
// Function: setupUI
void MainWindow::setupUI() {
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
    QWidget* leftPanel = new QWidget();
    leftPanel->setMinimumWidth(260);
    leftPanel->setMaximumWidth(380);
    QVBoxLayout* leftLayout = new QVBoxLayout(leftPanel);
    leftLayout->setContentsMargins(12, 12, 6, 12);
    leftLayout->setSpacing(8);
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
    quickDemoBtn_ = new QPushButton("Load Quick Demo");
    dbLayout->addWidget(quickDemoBtn_);
    leftLayout->addWidget(dbGroup);
    treeStatsLabel_ = new QLabel("Tree: empty");
    treeStatsLabel_->setObjectName("sectionTitle");
    leftLayout->addWidget(treeStatsLabel_);
    QGroupBox* wlGroup = new QGroupBox("Boilerplate & Whitelist");
    QVBoxLayout* wlLayout = new QVBoxLayout(wlGroup);
    QLabel* boilerplateTitle = new QLabel("BOILERPLATE (auto-detected)");
    boilerplateTitle->setObjectName("sectionTitle");
    wlLayout->addWidget(boilerplateTitle);
    boilerplateList_ = new QListWidget();
    boilerplateList_->setMaximumHeight(90);
    wlLayout->addWidget(boilerplateList_);
    autoDetectBoilerplateBtn_ = new QPushButton("Auto-Detect from Documents");
    wlLayout->addWidget(autoDetectBoilerplateBtn_);

    QLabel* whitelistTitle = new QLabel("WHITELIST TERMS");
    whitelistTitle->setObjectName("sectionTitle");
    wlLayout->addWidget(whitelistTitle);
    whitelistList_ = new QListWidget();
    whitelistList_->setMaximumHeight(72);
    wlLayout->addWidget(whitelistList_);
    QHBoxLayout* wlInputLayout = new QHBoxLayout();
    whitelistInput_ = new QLineEdit();
    whitelistInput_->setPlaceholderText("Enter word/phrase to ignore...");
    addWhitelistBtn_ = new QPushButton("+");
    addWhitelistBtn_->setMaximumWidth(36);
    wlInputLayout->addWidget(whitelistInput_);
    wlInputLayout->addWidget(addWhitelistBtn_);
    wlLayout->addLayout(wlInputLayout);
    removeWhitelistBtn_ = new QPushButton("Remove Whitelist Term");
    wlLayout->addWidget(removeWhitelistBtn_);
    leftLayout->addWidget(wlGroup);
    QGroupBox* sliderGroup = new QGroupBox("Detection Strictness");
    QVBoxLayout* sliderLayout = new QVBoxLayout(sliderGroup);
    strictnessSlider_ = new QSlider(Qt::Horizontal);
    strictnessSlider_->setRange(1, 80);   // normalized strictness in [0.01, 0.80]
    strictnessSlider_->setValue(30);
    sliderLayout->addWidget(strictnessSlider_);
    strictnessLabel_ = new QLabel("Strictness: 0.30  (lower = stricter)");
    sliderLayout->addWidget(strictnessLabel_);
    leftLayout->addWidget(sliderGroup);
    leftLayout->addStretch();
    splitter->addWidget(leftPanel);
    QWidget* centerPanel = new QWidget();
    QVBoxLayout* centerLayout = new QVBoxLayout(centerPanel);
    centerLayout->setContentsMargins(6, 12, 6, 12);
    centerLayout->setSpacing(8);
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
    QWidget* rightPanel = new QWidget();
    rightPanel->setMinimumWidth(220);
    rightPanel->setMaximumWidth(340);
    QVBoxLayout* rightLayout = new QVBoxLayout(rightPanel);
    rightLayout->setContentsMargins(6, 12, 12, 12);
    rightLayout->setSpacing(8);
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
    QGroupBox* sourcesGroup = new QGroupBox("Source Ranking");
    QVBoxLayout* sourcesLayout = new QVBoxLayout(sourcesGroup);
    matchFilesList_ = new QListWidget();
    sourcesLayout->addWidget(matchFilesList_);
    rightLayout->addWidget(sourcesGroup, 2);
    QGroupBox* synonymGroup = new QGroupBox("Synonym Dictionary");
    QVBoxLayout* synonymLayout = new QVBoxLayout(synonymGroup);
    synonymStatusLabel_ = new QLabel("Synonym Dictionary: not loaded");
    synonymStatusLabel_->setObjectName("sectionTitle");
    synonymLayout->addWidget(synonymStatusLabel_);
    synonymDemoBtn_ = new QPushButton("Load Synonym Demo");
    synonymLayout->addWidget(synonymDemoBtn_);
    synonymDemoOutput_ = new QTextEdit();
    synonymDemoOutput_->setReadOnly(true);
    synonymDemoOutput_->setMinimumHeight(120);
    synonymDemoOutput_->setPlaceholderText(
        "Click 'Load Synonym Demo' to see the full synonym dictionary\n"
        "and which words in the query map to which root tokens.");
    synonymLayout->addWidget(synonymDemoOutput_);
    rightLayout->addWidget(synonymGroup, 1);
    splitter->addWidget(rightPanel);
    splitter->setStretchFactor(0, 1);   // left
    splitter->setStretchFactor(1, 3);   // center
    splitter->setStretchFactor(2, 1);   // right
}
// Function: setupConnections
void MainWindow::setupConnections() {
    connect(addFileBtn_,        &QPushButton::clicked,
            this,               &MainWindow::onAddDatabaseFile);
    connect(removeFileBtn_,     &QPushButton::clicked,
            this,               &MainWindow::onRemoveDatabaseFile);
    connect(scanBtn_,           &QPushButton::clicked,
            this,               &MainWindow::onScanFile);
    connect(quickDemoBtn_,      &QPushButton::clicked,
            this,               &MainWindow::onLoadQuickDemo);
        connect(synonymDemoBtn_,    &QPushButton::clicked,
            this,               &MainWindow::onLoadSynonymDemo);
        connect(autoDetectBoilerplateBtn_, &QPushButton::clicked,
            this,               &MainWindow::onAutoDetectBoilerplate);
    connect(addWhitelistBtn_,   &QPushButton::clicked,
            this,               &MainWindow::onAddWhitelistWord);
    connect(removeWhitelistBtn_,&QPushButton::clicked,
            this,               &MainWindow::onRemoveWhitelistWord);
    connect(strictnessSlider_,  &QSlider::valueChanged,
            this,               &MainWindow::onStrictnessChanged);
}
// Function: onAddDatabaseFile
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
// Function: onRemoveDatabaseFile
void MainWindow::onRemoveDatabaseFile() {
    int row = databaseList_->currentRow();
    if (row < 0) return;
    engine_.removeFile(row);
    updateDatabaseView();
    updateTreeStats();
    statusBar()->showMessage("File removed, tree rebuilt.", 3000);
}
// Function: onScanFile
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
    currentQueryFile_ = file.toStdString();
    size_t sep = currentQueryFile_.find_last_of("/\\");
    std::string fname = (sep != std::string::npos)
        ? currentQueryFile_.substr(sep + 1) : currentQueryFile_;
    currentQueryName_ = fname;
    scanFileLabel_->setText(QString::fromStdString("Scanning: " + fname));
    if (engine_.treeEmpty()) {
        heatmapDisplay_->setPlainText(
            QString::fromStdString(currentQueryText_));
        matchPercentLabel_->setText("N/A");
        statusBar()->showMessage(
            "No database files indexed. Add files first.", 5000);
        return;
    }
    double radius = strictnessSlider_->value() / 100.0;
    lastScanRadius_ = radius;
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
// Function: onLoadQuickDemo
void MainWindow::onLoadQuickDemo() {
    QStringList dbFiles;
    dbFiles << "database_doc1.txt"
            << "database_doc2.txt"
            << "database_doc3.txt"
            << "database_doc4.txt"
            << "database_doc5.txt"
            << "database_doc6.txt"
            << "database_doc7.txt"
            << "database_doc8.txt"
            << "database_doc9.txt";
    QStringList dbPaths;
    QStringList missing;
    for (const QString& name : dbFiles) {
        QString path = findSampleFilePath(name);
        if (path.isEmpty())
            missing << name;
        else
            dbPaths << path;
    }
    QString query = findSampleFilePath("query_suspicious.txt");
    if (!missing.isEmpty() || query.isEmpty()) {
        QString details = missing.isEmpty()
            ? "Missing demo query file."
            : ("Missing demo files: " + missing.join(", "));
        QMessageBox::warning(
            this,
            "Quick Demo",
            details + " Ensure sample_data is available near the project root.");
        return;
    }
    while (engine_.indexedFiles().count() > 0) {
        engine_.removeFile(engine_.indexedFiles().count() - 1);
    }
    bool allOk = true;
    for (const QString& path : dbPaths) {
        if (!engine_.addFile(path.toStdString())) {
            allOk = false;
        }
    }
    if (!allOk) {
        QMessageBox::warning(this, "Quick Demo", "Failed to index sample database files.");
        return;
    }
    currentQueryText_ = PlagiarismEngine::readFile(query.toStdString());
    if (currentQueryText_.empty()) {
        QMessageBox::warning(this, "Quick Demo", "Failed to read sample query file.");
        return;
    }
    currentQueryFile_ = query.toStdString();
    currentQueryName_ = "query_suspicious.txt";
    scanFileLabel_->setText("Demo: query_suspicious.txt");
    updateDatabaseView();
    updateTreeStats();
    statusBar()->showMessage("Quick demo loaded. Running scan...", 3000);
    onRescan();
}

// Function: onLoadSynonymDemo
void MainWindow::onLoadSynonymDemo() {
    QString db = findSampleFilePath("database_urban.txt");
    QString query = findSampleFilePath("query_thesaurus.txt");
    QString syn = findSampleFilePath("synonyms.csv");
    if (db.isEmpty() || query.isEmpty()) {
        QMessageBox::warning(
            this,
            "Synonym Demo",
            "Could not locate synonym demo files. Ensure database_urban.txt and query_thesaurus.txt exist in sample_data.");
        return;
    }
    if (!engine_.synonymsLoaded() && !syn.isEmpty()) {
        engine_.loadSynonymDictionary(syn.toStdString());
    }
    if (synonymStatusLabel_) {
        synonymStatusLabel_->setText(
            engine_.synonymsLoaded()
                ? ("Synonym Dictionary: loaded (" + QString::number(engine_.synonymCount()) + " entries)")
                : "Synonym Dictionary: not loaded");
    }
    while (engine_.indexedFiles().count() > 0) {
        engine_.removeFile(engine_.indexedFiles().count() - 1);
    }
    bool ok = engine_.addFile(db.toStdString());
    if (!ok) {
        QMessageBox::warning(this, "Synonym Demo", "Failed to index database_urban.txt.");
        return;
    }
    currentQueryText_ = PlagiarismEngine::readFile(query.toStdString());
    if (currentQueryText_.empty()) {
        QMessageBox::warning(this, "Synonym Demo", "Failed to read query_thesaurus.txt.");
        return;
    }
    currentQueryFile_ = query.toStdString();
    currentQueryName_ = "query_thesaurus.txt";
    scanFileLabel_->setText("Synonym Demo: query_thesaurus.txt");
    updateDatabaseView();
    updateTreeStats();
    if (synonymDemoOutput_) {
        // Build the detailed synonym dictionary display.
        QString output;
        output += QString::fromUtf8("\u2550\u2550\u2550 Synonym Dictionary \u2550\u2550\u2550\n");
        auto dict = engine_.getSynonymDictionary();
        for (const auto& entry : dict) {
            QString root = QString::fromStdString(entry.rootToken);
            QString syns;
            for (size_t s = 0; s < entry.synonyms.size(); ++s) {
                if (s > 0) syns += ", ";
                syns += QString::fromStdString(entry.synonyms[s]);
            }
            output += root + QString::fromUtf8("  \u2190 ") + syns + "\n";
        }

        // Show which words in the query get mapped.
        output += QString::fromUtf8("\n\u2550\u2550\u2550 Query Word Mappings \u2550\u2550\u2550\n");
        auto mappings = engine_.getSynonymMappingReport(currentQueryText_);
        if (mappings.empty()) {
            output += "(no synonym substitutions found in query)\n";
        } else {
            for (const auto& m : mappings) {
                output += QString::fromUtf8("\"")
                    + QString::fromStdString(m.originalWord)
                    + QString::fromUtf8("\"  \u2192  ")
                    + QString::fromStdString(m.rootToken)
                    + "  (synonym match)\n";
            }
        }

        synonymDemoOutput_->setPlainText(output);
    }
    statusBar()->showMessage("Synonym demo loaded. Running scan...", 3000);
    onRescan();
}
// Function: onScanFinished
void MainWindow::onScanFinished(ScanReport report) {
    progressBar_->setVisible(false);
    scanBtn_->setEnabled(true);
    applyHeatmap(report);
    QString pctText = QString::number(report.matchPercentage, 'f', 1) + "%";
    matchPercentLabel_->setText(pctText);
    if (report.matchPercentage > 50.0)
        matchPercentLabel_->setStyleSheet(
            "font-size:28px; font-weight:bold; color:#f38ba8; padding:10px;");
    else if (report.matchPercentage > 25.0)
        matchPercentLabel_->setStyleSheet(
            "font-size:28px; font-weight:bold; color:#fab387; padding:10px;");
    else
        matchPercentLabel_->setStyleSheet(
            "font-size:28px; font-weight:bold; color:#a6e3a1; padding:10px;");
    std::string queryName = currentQueryName_;
    if (queryName.empty()) {
        queryName = currentQueryFile_;
        size_t sep = queryName.find_last_of("/\\");
        if (sep != std::string::npos)
            queryName = queryName.substr(sep + 1);
    }
    RawBuffer<SourceScore> scores = engine_.rankSources(
        currentQueryText_, queryName, lastScanRadius_, 12);
    matchFilesList_->clear();
    if (scores.count() > 0) {
        for (int i = 0; i < scores.count(); ++i) {
            QString src = QString::fromStdString(scores[i].sourceFile);
            QString hits = QString::number(scores[i].matchedNgrams);
            QString cover = QString::number(scores[i].coveragePercent, 'f', 1);
            QString sim = QString::number(scores[i].avgSimilarity, 'f', 2);
            matchFilesList_->addItem(
                src + " | hits:" + hits + " | cover:" + cover + "% | sim:" + sim);
        }
    } else {
        for (int i = 0; i < report.matchedFiles.count(); ++i) {
            matchFilesList_->addItem(
                QString::fromStdString(report.matchedFiles[i]));
        }
    }
    int sourceCount = (scores.count() > 0)
        ? scores.count()
        : report.matchedFiles.count();
    statusBar()->showMessage(
        QString("Scan complete — %1 match, %2 source(s) ranked")
            .arg(pctText)
            .arg(sourceCount),
        10000);
    if (synonymDemoOutput_ && queryName == "query_thesaurus.txt") {
        // Append the match results below the already-displayed dictionary.
        QString existing = synonymDemoOutput_->toPlainText();
        existing += QString::fromUtf8("\n\u2550\u2550\u2550 Match Results \u2550\u2550\u2550\n");

        bool found = false;
        for (int i = 0; i < scores.count(); ++i) {
            if (scores[i].sourceFile == "database_urban.txt") {
                found = true;
                existing += "database_urban.txt MATCHED via synonyms\n"
                    + QString("  Hits: %1 | Cover: %2% | Avg sim: %3\n")
                        .arg(scores[i].matchedNgrams)
                        .arg(scores[i].coveragePercent, 0, 'f', 1)
                        .arg(scores[i].avgSimilarity, 0, 'f', 2);
                break;
            }
        }
        if (!found) {
            existing += "database_urban.txt: no match found in ranking.\n";
        }
        synonymDemoOutput_->setPlainText(existing);
    }
}
// Function: onAutoDetectBoilerplate
void MainWindow::onAutoDetectBoilerplate() {
    if (scanWorker_ && scanWorker_->isRunning()) {
        statusBar()->showMessage("Wait for current scan to finish before detecting boilerplate.", 3000);
        return;
    }
    if (engine_.indexedFiles().count() < 2) {
        QMessageBox::information(this, "Auto-Detect Boilerplate",
            "Need at least 2 indexed documents to detect boilerplate.\n"
            "Add more database files first.");
        return;
    }
    int detected = engine_.detectBoilerplate();
    updateBoilerplateView();

    if (detected > 0 && engine_.indexedFiles().count() > 0) {
        engine_.rebuildIndexWithFilters();
        updateDatabaseView();
        updateTreeStats();
        statusBar()->showMessage(
            QString("Auto-detected %1 boilerplate phrase(s). Index rebuilt.")
                .arg(detected),
            5000);
    } else {
        statusBar()->showMessage(
            "No common boilerplate phrases found across documents.", 5000);
    }
    onRescan();
}
// Function: onAddWhitelistWord
void MainWindow::onAddWhitelistWord() {
    if (scanWorker_ && scanWorker_->isRunning()) {
        statusBar()->showMessage("Wait for current scan to finish before editing filters.", 3000);
        return;
    }
    QString word = whitelistInput_->text().trimmed();
    if (word.isEmpty()) return;
    engine_.addWhitelistWord(word.toStdString());
    whitelistInput_->clear();
    updateWhitelistView();

    if (engine_.indexedFiles().count() > 0) {
        bool rebuilt = engine_.rebuildIndexWithFilters();
        updateDatabaseView();
        updateTreeStats();
        statusBar()->showMessage(
            rebuilt
                ? "Whitelist updated. Existing index rebuilt with active filters."
                : "Whitelist updated, but some indexed files could not be rebuilt.",
            5000);
    }
    onRescan();
}
// Function: onRemoveWhitelistWord
void MainWindow::onRemoveWhitelistWord() {
    if (scanWorker_ && scanWorker_->isRunning()) {
        statusBar()->showMessage("Wait for current scan to finish before editing filters.", 3000);
        return;
    }
    int row = whitelistList_->currentRow();
    if (row < 0) return;
    engine_.removeWhitelistWord(row);
    updateWhitelistView();

    if (engine_.indexedFiles().count() > 0) {
        bool rebuilt = engine_.rebuildIndexWithFilters();
        updateDatabaseView();
        updateTreeStats();
        statusBar()->showMessage(
            rebuilt
                ? "Whitelist updated. Existing index rebuilt with active filters."
                : "Whitelist updated, but some indexed files could not be rebuilt.",
            5000);
    }
    onRescan();
}
// Function: onStrictnessChanged
void MainWindow::onStrictnessChanged(int value) {
    double strictness = value / 100.0;
    strictnessLabel_->setText(
        QString("Strictness: %1  (%2)")
            .arg(strictness, 0, 'f', 2)
            .arg(strictness <= 0.15 ? "very strict" :
                 strictness <= 0.30 ? "strict" :
                 strictness <= 0.50 ? "moderate" : "lenient"));
    if (!currentQueryText_.empty() && !engine_.treeEmpty()) {
        onRescan();
    }
}
// Function: onRescan
void MainWindow::onRescan() {
    if (currentQueryText_.empty() || engine_.treeEmpty()) return;
    if (scanWorker_ && scanWorker_->isRunning()) return;
    double radius = strictnessSlider_->value() / 100.0;
    lastScanRadius_ = radius;
    std::string fname = currentQueryName_;
    if (fname.empty()) {
        fname = currentQueryFile_;
        size_t sep = fname.find_last_of("/\\");
        if (sep != std::string::npos) fname = fname.substr(sep + 1);
        currentQueryName_ = fname;
    }
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
// Function: applyHeatmap
void MainWindow::applyHeatmap(const ScanReport& report) {
    heatmapDisplay_->clear();
    QString fullText = QString::fromStdString(currentQueryText_);
    int textLen = fullText.length();
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

    // Un-mark characters belonging to whitelist or boilerplate terms
    // so they are NOT highlighted in the heatmap. We search in a
    // normalized copy of the query (not raw lowercase) because the
    // original may contain punctuation that normalizeText() strips.
    {
        std::string origText = currentQueryText_;

        // Build normalized query and position map (normIdx -> origIdx).
        std::string normQuery;
        normQuery.reserve(origText.size());
        int* normToOrig = new int[origText.size() + 1];
        int normLen = 0;
        {
            bool lastSp = false;
            for (int ci = 0; ci < textLen; ++ci) {
                unsigned char uc = static_cast<unsigned char>(origText[ci]);
                if (std::isalnum(uc)) {
                    normQuery += static_cast<char>(std::tolower(uc));
                    normToOrig[normLen++] = ci;
                    lastSp = false;
                } else if (std::isspace(uc) ||
                           origText[ci] == '-' || origText[ci] == '\'') {
                    if (!lastSp && !normQuery.empty()) {
                        normQuery += ' ';
                        normToOrig[normLen++] = ci;
                        lastSp = true;
                    }
                }
            }
            if (!normQuery.empty() && normQuery.back() == ' ') {
                normQuery.pop_back();
                --normLen;
            }
        }

        auto unmarkTerms = [&](const RawBuffer<std::string>& terms) {
            for (int t = 0; t < terms.count(); ++t) {
                // Normalize term the same way.
                std::string term;
                const std::string& raw = terms[t];
                bool lastSpace = false;
                for (char ch : raw) {
                    if (std::isalnum(static_cast<unsigned char>(ch))) {
                        term += static_cast<char>(
                            std::tolower(static_cast<unsigned char>(ch)));
                        lastSpace = false;
                    } else if (!term.empty() && !lastSpace) {
                        term += ' ';
                        lastSpace = true;
                    }
                }
                if (!term.empty() && term.back() == ' ') term.pop_back();
                if (term.empty()) continue;

                size_t p = 0;
                while ((p = normQuery.find(term, p)) != std::string::npos) {
                    bool leftOk  = (p == 0) || (normQuery[p - 1] == ' ');
                    size_t ep = p + term.size();
                    bool rightOk = (ep >= static_cast<size_t>(normLen)) ||
                                   (normQuery[ep] == ' ');
                    if (leftOk && rightOk) {
                        int origStart = normToOrig[p];
                        int origEnd   = (ep < static_cast<size_t>(normLen))
                                            ? normToOrig[ep] : textLen;
                        for (int c = origStart; c < origEnd && c < textLen; ++c)
                            isMatched[c] = false;
                    }
                    ++p;
                }
            }
        };

        unmarkTerms(engine_.whitelist());
        unmarkTerms(engine_.boilerplate());
        delete[] normToOrig;
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
        while (i < textLen && isMatched[i] == state) ++i;
        QString segment = fullText.mid(start, i - start);
        cursor.insertText(segment, state ? matchFmt : normalFmt);
    }
    cursor.endEditBlock();
    delete[] isMatched;
    cursor.movePosition(QTextCursor::Start);
    heatmapDisplay_->setTextCursor(cursor);
}
// Function: updateDatabaseView
void MainWindow::updateDatabaseView() {
    databaseList_->clear();
    const auto& files = engine_.indexedFiles();
    for (int i = 0; i < files.count(); ++i) {
        QString item = QString::fromStdString(files[i].filename)
                       + "  (" + QString::number(files[i].ngramCount) + " n-grams)";
        databaseList_->addItem(item);
    }
}
// Function: updateBoilerplateView
void MainWindow::updateBoilerplateView() {
    boilerplateList_->clear();
    const auto& bp = engine_.boilerplate();
    for (int i = 0; i < bp.count(); ++i)
        boilerplateList_->addItem(QString::fromStdString(bp[i]));
}
// Function: updateWhitelistView
void MainWindow::updateWhitelistView() {
    whitelistList_->clear();
    const auto& wl = engine_.whitelist();
    for (int i = 0; i < wl.count(); ++i)
        whitelistList_->addItem(QString::fromStdString(wl[i]));
}
// Function: updateTreeStats
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
