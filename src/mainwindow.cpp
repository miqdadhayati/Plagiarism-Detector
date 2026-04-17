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
#include <QRegularExpression>

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
    traceLines_.clear();
    int traceBudget = 420;
    bool traceTruncated = false;
    engine_->setPipelineTraceCallback([this, &traceBudget, &traceTruncated](const std::string& line) {
        if (traceBudget > 0) {
            traceLines_.append(QString::fromStdString(line));
            --traceBudget;
            return;
        }
        if (!traceTruncated) {
            traceLines_.append("... trace truncated ...");
            traceTruncated = true;
        }
    });
    engine_->setDataStructureTraceEnabled(true);
    ScanReport report = engine_->scan(text_, filename_, radius_);
    engine_->setDataStructureTraceEnabled(false);
    engine_->clearPipelineTraceCallback();
    emit scanComplete(report);
}
// Function: MainWindow
MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
    , scanWorker_(nullptr)
    , opsPlaybackTimer_(nullptr)
    , opsTraceIndex_(0)
    , opsPlaybackPaused_(false)
{
    setupUI();
    opsPlaybackTimer_ = new QTimer(this);
    opsPlaybackTimer_->setInterval(700);
    setupConnections();
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
    opsDemoBtn_ = new QPushButton("Visualize DS Ops");
    dbLayout->addWidget(opsDemoBtn_);
    leftLayout->addWidget(dbGroup);
    treeStatsLabel_ = new QLabel("Tree: empty");
    treeStatsLabel_->setObjectName("sectionTitle");
    leftLayout->addWidget(treeStatsLabel_);
    QGroupBox* wlGroup = new QGroupBox("Boilerplate & Whitelist");
    QVBoxLayout* wlLayout = new QVBoxLayout(wlGroup);
    QLabel* boilerplateTitle = new QLabel("BOILERPLATE PHRASES");
    boilerplateTitle->setObjectName("sectionTitle");
    wlLayout->addWidget(boilerplateTitle);
    boilerplateList_ = new QListWidget();
    boilerplateList_->setMaximumHeight(72);
    wlLayout->addWidget(boilerplateList_);
    QHBoxLayout* bpInputLayout = new QHBoxLayout();
    boilerplateInput_ = new QLineEdit();
    boilerplateInput_->setPlaceholderText("Enter boilerplate phrase...");
    addBoilerplateBtn_ = new QPushButton("+");
    addBoilerplateBtn_->setMaximumWidth(36);
    bpInputLayout->addWidget(boilerplateInput_);
    bpInputLayout->addWidget(addBoilerplateBtn_);
    wlLayout->addLayout(bpInputLayout);
    removeBoilerplateBtn_ = new QPushButton("Remove Boilerplate");
    wlLayout->addWidget(removeBoilerplateBtn_);

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
    QGroupBox* sourcesGroup = new QGroupBox("Matched Sources");
    QVBoxLayout* sourcesLayout = new QVBoxLayout(sourcesGroup);
    matchFilesList_ = new QListWidget();
    sourcesLayout->addWidget(matchFilesList_);
    rightLayout->addWidget(sourcesGroup, 1);
    QGroupBox* traceGroup = new QGroupBox("Backend Operation Trace");
    QVBoxLayout* traceLayout = new QVBoxLayout(traceGroup);
    opsStepLabel_ = new QLabel("Step 0 / 0");
    opsStepLabel_->setObjectName("sectionTitle");
    traceLayout->addWidget(opsStepLabel_);
    QHBoxLayout* playbackControls = new QHBoxLayout();
    opsPlayPauseBtn_ = new QPushButton("Pause");
    opsNextStepBtn_ = new QPushButton("Next Step");
    playbackControls->addWidget(opsPlayPauseBtn_);
    playbackControls->addWidget(opsNextStepBtn_);
    traceLayout->addLayout(playbackControls);
    QHBoxLayout* speedLayout = new QHBoxLayout();
    opsSpeedLabel_ = new QLabel("Step delay: 700 ms");
    opsSpeedSlider_ = new QSlider(Qt::Horizontal);
    opsSpeedSlider_->setRange(200, 1600);
    opsSpeedSlider_->setValue(700);
    speedLayout->addWidget(opsSpeedLabel_);
    speedLayout->addWidget(opsSpeedSlider_);
    traceLayout->addLayout(speedLayout);
    opsTraceProgress_ = new QProgressBar();
    opsTraceProgress_->setRange(0, 100);
    opsTraceProgress_->setValue(0);
    opsTraceProgress_->setTextVisible(false);
    traceLayout->addWidget(opsTraceProgress_);
    opsExplainDisplay_ = new QTextEdit();
    opsExplainDisplay_->setReadOnly(true);
    opsExplainDisplay_->setMinimumHeight(120);
    opsExplainDisplay_->setPlaceholderText(
        "Step-by-step explanation appears here.\n"
        "Math and storage notes will update each step.");
    traceLayout->addWidget(opsExplainDisplay_);
    opsTraceDisplay_ = new QTextEdit();
    opsTraceDisplay_->setReadOnly(true);
    opsTraceDisplay_->setMinimumHeight(220);
    opsTraceDisplay_->setPlaceholderText(
        "Backend trace appears here.\n"
        "Click 'Visualize DS Ops' to replay tree internals.");
    traceLayout->addWidget(opsTraceDisplay_);
    rightLayout->addWidget(traceGroup, 2);
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
    connect(opsDemoBtn_,        &QPushButton::clicked,
            this,               &MainWindow::onVisualizeOperationsDemo);
        connect(addBoilerplateBtn_, &QPushButton::clicked,
            this,               &MainWindow::onAddBoilerplatePhrase);
        connect(removeBoilerplateBtn_, &QPushButton::clicked,
            this,               &MainWindow::onRemoveBoilerplatePhrase);
    connect(addWhitelistBtn_,   &QPushButton::clicked,
            this,               &MainWindow::onAddWhitelistWord);
    connect(removeWhitelistBtn_,&QPushButton::clicked,
            this,               &MainWindow::onRemoveWhitelistWord);
    connect(strictnessSlider_,  &QSlider::valueChanged,
            this,               &MainWindow::onStrictnessChanged);
    connect(opsPlaybackTimer_,  &QTimer::timeout,
            this,               &MainWindow::onTracePlaybackTick);
    connect(opsPlayPauseBtn_,   &QPushButton::clicked,
            this,               &MainWindow::onTracePlayPause);
    connect(opsNextStepBtn_,    &QPushButton::clicked,
            this,               &MainWindow::onTraceNextStep);
    connect(opsSpeedSlider_,    &QSlider::valueChanged,
            this,               &MainWindow::onTraceSpeedChanged);
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
    QString db1 = findSampleFilePath("database_doc1.txt");
    QString db2 = findSampleFilePath("database_doc2.txt");
    QString query = findSampleFilePath("query_suspicious.txt");
    if (db1.isEmpty() || db2.isEmpty() || query.isEmpty()) {
        QMessageBox::warning(
            this,
            "Quick Demo",
            "Could not locate sample_data files. Ensure sample_data is available near the project root.");
        return;
    }
    while (engine_.indexedFiles().count() > 0) {
        engine_.removeFile(engine_.indexedFiles().count() - 1);
    }
    bool ok1 = engine_.addFile(db1.toStdString());
    bool ok2 = engine_.addFile(db2.toStdString());
    if (!ok1 || !ok2) {
        QMessageBox::warning(this, "Quick Demo", "Failed to index sample database files.");
        return;
    }
    currentQueryText_ = PlagiarismEngine::readFile(query.toStdString());
    if (currentQueryText_.empty()) {
        QMessageBox::warning(this, "Quick Demo", "Failed to read sample query file.");
        return;
    }
    currentQueryFile_ = query.toStdString();
    scanFileLabel_->setText("Demo: query_suspicious.txt");
    updateDatabaseView();
    updateTreeStats();
    statusBar()->showMessage("Quick demo loaded. Running scan...", 3000);
    onRescan();
}
// Function: onVisualizeOperationsDemo
void MainWindow::onVisualizeOperationsDemo() {
    QStringList traceLines;
    auto addSection = [&traceLines](const QString& title) {
        traceLines << "";
        traceLines << ("=== " + title + " ===");
    };
    VPTree demoTree;
    demoTree.setTraceCallback([&traceLines](const std::string& message) {
        traceLines << ("  " + QString::fromStdString(message));
    });
    NGram a("graph search tree", "viz_a.txt", 0, 17, 0);
    NGram b("graph shortest path", "viz_b.txt", 0, 19, 0);
    NGram c("neural network model", "viz_c.txt", 0, 20, 0);
    NGram d("machine learning model", "viz_d.txt", 0, 22, 0);
    NGram e("dynamic programming", "viz_e.txt", 0, 19, 0);
    NGram items[5] = {a, b, c, d, e};
    addSection("CREATE / BUILD TREE");
    demoTree.build_tree(items, 5);
    bool createOk = !demoTree.empty() && demoTree.size() == 5;
    traceLines << QString("  summary: size=%1 height=%2")
        .arg(demoTree.size())
        .arg(VPTree::get_height(demoTree.root()));
    traceLines << "  snapshot:";
    appendTreeSnapshotLines(traceLines, demoTree.root(), 1, 2);
    addSection("READ / CONTAINS");
    bool readOk = demoTree.contains(c);
    traceLines << QString("  contains(\"%1\") => %2")
        .arg(compactNgramText(c.text))
        .arg(readOk ? "true" : "false");
    addSection("RANGE QUERY");
    NGram query("graph search tree", "viz_query.txt", 0, 17, 0);
    RawBuffer<SearchResult> rangeResults = demoTree.range_query(query, 0.25);
    bool rangeOk = rangeResults.count() > 0;
    traceLines << QString("  range matches=%1").arg(rangeResults.count());
    int rangePreview = (rangeResults.count() < 3) ? rangeResults.count() : 3;
    for (int i = 0; i < rangePreview; ++i) {
        traceLines << QString("    #%1 dist=%2 src=%3 text=\"%4\"")
            .arg(i + 1)
            .arg(rangeResults[i].distance, 0, 'f', 2)
            .arg(QString::fromStdString(rangeResults[i].ngram.sourceFile))
            .arg(compactNgramText(rangeResults[i].ngram.text));
    }
    addSection("KNN QUERY");
    RawBuffer<SearchResult> knnResults = demoTree.search_knn(query, 2);
    bool knnOk = knnResults.count() == 2;
    traceLines << QString("  knn neighbors=%1").arg(knnResults.count());
    for (int i = 0; i < knnResults.count(); ++i) {
        traceLines << QString("    #%1 dist=%2 src=%3 text=\"%4\"")
            .arg(i + 1)
            .arg(knnResults[i].distance, 0, 'f', 2)
            .arg(QString::fromStdString(knnResults[i].ngram.sourceFile))
            .arg(compactNgramText(knnResults[i].ngram.text));
    }
    addSection("UPDATE");
    NGram updatedD("machine learning systems", "viz_d.txt", 0, 24, 0);
    bool updateOk = demoTree.update(d, updatedD)
                 && demoTree.contains(updatedD)
                 && !demoTree.contains(d);
    traceLines << QString("  update result=%1")
        .arg(updateOk ? "true" : "false");
    traceLines << "  snapshot:";
    appendTreeSnapshotLines(traceLines, demoTree.root(), 1, 2);
    addSection("DELETE");
    bool deleteOk = demoTree.remove(e) && !demoTree.contains(e) && demoTree.size() == 4;
    traceLines << QString("  delete result=%1 size=%2")
        .arg(deleteOk ? "true" : "false")
        .arg(demoTree.size());
    addSection("REBUILD / TREE INSPECTION");
    int beforeRebuild = demoTree.size();
    demoTree.rebuild();
    bool rebuildOk = !demoTree.empty() && demoTree.size() == beforeRebuild;
    bool heightOk = VPTree::get_height(demoTree.root()) >= 1;
    bool leafOk = VPTree::is_leaf(demoTree.root()) == false;
    traceLines << QString("  rebuild result=%1 size=%2 height=%3")
        .arg(rebuildOk ? "true" : "false")
        .arg(demoTree.size())
        .arg(VPTree::get_height(demoTree.root()));
    traceLines << QString("  root is leaf=%1")
        .arg(leafOk ? "false (expected)" : "true (unexpected)");
    traceLines << "  snapshot:";
    appendTreeSnapshotLines(traceLines, demoTree.root(), 1, 2);
    demoTree.clearTraceCallback();
    struct OperationView {
        const char* name;
        bool ok;
    };
    OperationView ops[9] = {
        {"CREATE", createOk},
        {"READ", readOk},
        {"RANGE QUERY", rangeOk},
        {"KNN QUERY", knnOk},
        {"UPDATE", updateOk},
        {"DELETE", deleteOk},
        {"REBUILD", rebuildOk},
        {"HEIGHT", heightOk},
        {"IS_LEAF", leafOk}
    };
    int passCount = 0;
    int opCount = 9;
    QString report = "VP-Tree Operations Visualisation\n\n";
    matchFilesList_->clear();
    for (int i = 0; i < opCount; ++i) {
        if (ops[i].ok) ++passCount;
        QString bar = ops[i].ok ? "[##########]" : "[----------]";
        QString status = ops[i].ok ? "PASS" : "FAIL";
        QString line = QString("%1 %2 %3")
            .arg(QString::fromUtf8(ops[i].name), -12)
            .arg(bar)
            .arg(status);
        report += line + "\n";
        matchFilesList_->addItem(
            QString::fromUtf8(ops[i].name) + ": " + status);
    }
    double pct = (static_cast<double>(passCount) * 100.0)
               / static_cast<double>(opCount);
    matchPercentLabel_->setText(QString::number(pct, 'f', 1) + "%");
    if (pct < 100.0) {
        matchPercentLabel_->setStyleSheet(
            "font-size:28px; font-weight:bold; color:#fab387; padding:10px;");
    } else {
        matchPercentLabel_->setStyleSheet(
            "font-size:28px; font-weight:bold; color:#a6e3a1; padding:10px;");
    }
    scanFileLabel_->setText("DS Ops Visualisation");
    heatmapDisplay_->setPlainText(
        "Operation summary updated.\n"
        "Right panel now plays backend steps one by one.\n"
        "Use Pause / Next Step and the delay slider to explain each step clearly.");
    startTracePlayback(traceLines);
    statusBar()->showMessage(
        QString("Operations demo running: %1/%2 checks passed")
            .arg(passCount)
            .arg(opCount),
        8000);
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
    if (scanWorker_) {
        QStringList scanTrace = scanWorker_->traceLines();
        if (!scanTrace.isEmpty()) {
            startTracePlayback(scanTrace);
            statusBar()->showMessage(
                QString("Scan complete — %1 match. Backend scan trace loaded (%2 steps).")
                    .arg(pctText)
                    .arg(scanTrace.count()),
                10000);
        }
    }
}
// Function: onAddBoilerplatePhrase
void MainWindow::onAddBoilerplatePhrase() {
    if (scanWorker_ && scanWorker_->isRunning()) {
        statusBar()->showMessage("Wait for current scan to finish before editing filters.", 3000);
        return;
    }
    QString phrase = boilerplateInput_->text().trimmed();
    if (phrase.isEmpty()) return;
    engine_.addBoilerplatePhrase(phrase.toStdString());
    boilerplateInput_->clear();
    updateBoilerplateView();

    if (engine_.indexedFiles().count() > 0) {
        bool rebuilt = engine_.rebuildIndexWithFilters();
        updateDatabaseView();
        updateTreeStats();
        statusBar()->showMessage(
            rebuilt
                ? "Boilerplate added. Existing index rebuilt with active filters."
                : "Boilerplate added, but some indexed files could not be rebuilt.",
            5000);
    }
    onRescan();
}

// Function: onRemoveBoilerplatePhrase
void MainWindow::onRemoveBoilerplatePhrase() {
    if (scanWorker_ && scanWorker_->isRunning()) {
        statusBar()->showMessage("Wait for current scan to finish before editing filters.", 3000);
        return;
    }
    int row = boilerplateList_->currentRow();
    if (row < 0) return;
    engine_.removeBoilerplatePhrase(row);
    updateBoilerplateView();

    if (engine_.indexedFiles().count() > 0) {
        bool rebuilt = engine_.rebuildIndexWithFilters();
        updateDatabaseView();
        updateTreeStats();
        statusBar()->showMessage(
            rebuilt
                ? "Boilerplate removed. Existing index rebuilt with active filters."
                : "Boilerplate removed, but some indexed files could not be rebuilt.",
            5000);
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
// Function: explainTraceLine
QString MainWindow::explainTraceLine(const QString& line) const {
    QString t = line.trimmed();
    bool treeForwarded = false;
    if (t.startsWith("tree ")) {
        t = t.mid(5).trimmed();
        treeForwarded = true;
    }
    if (t.isEmpty()) {
        return "Section break. Next operation phase starts below.";
    }
    if (t.startsWith("index start ")) {
        return "Indexing pipeline begins: file is read, normalized, tokenized into n-grams, then inserted into the VP-tree.";
    }
    if (t.startsWith("index preprocess ")) {
        return "Text preprocessing: punctuation removed, lowercased, whitelist applied, and n-grams generated for storage.";
    }
    if (t.startsWith("index done ")) {
        return "Database update complete: n-grams are stored in VP-tree nodes with updated tree size/height.";
    }
    if (t.startsWith("remove-file start ")) {
        return "Delete pipeline begins: selected file's n-grams are filtered out before tree rebuild.";
    }
    if (t.startsWith("remove-file done ")) {
        return "Delete pipeline complete: tree rebuilt and structural stats updated.";
    }
    if (t.startsWith("scan start ")) {
        return "Plagiarism scan begins: query text enters the backend pipeline with chosen strictness radius.";
    }
    if (t.startsWith("scan preprocess ")) {
        return "Preprocess stage: normalize text and remove whitelist phrases before querying the tree.";
    }
    if (t.startsWith("scan tokenize ")) {
        QRegularExpression rx("words=([0-9]+).*ngramSize=([0-9]+)");
        QRegularExpressionMatch m = rx.match(t);
        if (m.hasMatch()) {
            int words = m.captured(1).toInt();
            int n = m.captured(2).toInt();
            int total = words - n + 1;
            if (total < 0) total = 0;
            return QString("Token stage math: words=%1, n=%2, total query n-grams=max(words-n+1,0)=%3.")
                .arg(words)
                .arg(n)
                .arg(total);
        }
        return "Token stage: split cleaned query into overlapping n-word windows.";
    }
    if (t.startsWith("scan query-ngrams total=")) {
        return "This is the number of query windows that will be searched against the VP-tree.";
    }
    if (t.startsWith("scan step i=") && t.contains("effectiveRadius=")) {
        QRegularExpression rx("i=([0-9]+).*effectiveRadius=([0-9eE.+-]+)");
        QRegularExpressionMatch m = rx.match(t);
        if (m.hasMatch()) {
            int idx = m.captured(1).toInt();
            double r = m.captured(2).toDouble();
            return QString("Step %1: convert normalized strictness to edit-distance radius r=%2 for this n-gram.")
                .arg(idx)
                .arg(r, 0, 'f', 2);
        }
        return "Per n-gram scan step: compute radius and query the VP-tree.";
    }
    if (t.startsWith("scan step i=") && t.contains("rangeResults=")) {
        QRegularExpression rx("i=([0-9]+).*rangeResults=([0-9]+)");
        QRegularExpressionMatch m = rx.match(t);
        if (m.hasMatch()) {
            return QString("Step %1 result: VP-tree range query returned %2 candidate matches.")
                .arg(m.captured(1).toInt())
                .arg(m.captured(2).toInt());
        }
        return "Range query result count for this scan step.";
    }
    if (t.startsWith("scan step i=") && t.contains("bestDistance=")) {
        return "Best candidate selected by minimum edit distance; this drives source attribution and segment marking.";
    }
    if (t.startsWith("scan summary ")) {
        QRegularExpression rx("matchedNgrams=([0-9]+).*totalNgrams=([0-9]+).*matchedChars=([0-9]+).*textLen=([0-9]+)");
        QRegularExpressionMatch m = rx.match(t);
        if (m.hasMatch()) {
            int mg = m.captured(1).toInt();
            int tg = m.captured(2).toInt();
            int mc = m.captured(3).toInt();
            int tl = m.captured(4).toInt();
            double pct = (tl > 0) ? (100.0 * mc / tl) : 0.0;
            return QString("Summary math: n-gram hits=%1/%2, char hits=%3/%4, plagiarism%%≈%5.")
                .arg(mg)
                .arg(tg)
                .arg(mc)
                .arg(tl)
                .arg(pct, 0, 'f', 1);
        }
        return "Scan summary combines n-gram and character-level match statistics.";
    }
    if (t.startsWith("scan done ")) {
        return "Scan complete: final percentage and matched source-file count are emitted to the UI.";
    }
    if (t == "... trace truncated ...") {
        return "Trace budget reached. Visualization keeps key steps while avoiding an overwhelming log.";
    }
    if (t.startsWith("=== ")) {
        return "Starting a new data-structure operation phase.";
    }
    if (t.startsWith("build_tree(start")) {
        return "Create phase begins: allocate root recursively using raw pointers and partition by median distance.";
    }
    if (t.startsWith("buildRecursive(count=")) {
        return "Recursive build: choose 1 vantage point, then split remaining elements into inside/outside subsets.";
    }
    if (t.startsWith("selected VP:")) {
        return "Vantage point is selected to maximize distance spread (high variance gives better partitions).";
    }
    if (t.startsWith("median distance:")) {
        return "Math: mu = median_i d(vp, x_i). This threshold drives left/right partitioning.";
    }
    if (t.startsWith("partition inside=")) {
        return "Storage split: left subtree stores {x | d(vp,x) <= mu}, right subtree stores {x | d(vp,x) > mu}.";
    }
    if (t.startsWith("leaf node:")) {
        return "Leaf allocation: node.left = nullptr, node.right = nullptr.";
    }
    if (t.startsWith("range_query(start")) {
        return treeForwarded
            ? "VP-tree internal call during scan: return all x with d(query,x)<=radius."
            : "Range query math: return all x satisfying d(query, x) <= radius.";
    }
    if (t.startsWith("range visit ")) {
        QRegularExpression rx("dist=([0-9eE.+-]+).*radius=([0-9eE.+-]+).*mu=([0-9eE.+-]+)");
        QRegularExpressionMatch m = rx.match(t);
        if (m.hasMatch()) {
            double d = m.captured(1).toDouble();
            double r = m.captured(2).toDouble();
            double mu = m.captured(3).toDouble();
            return QString("Math: d=%1, r=%2, mu=%3. Explore left if d-r<=mu (%4<=%5). Explore right if d+r>mu (%6>%7).")
                .arg(d, 0, 'f', 2)
                .arg(r, 0, 'f', 2)
                .arg(mu, 0, 'f', 2)
                .arg(d - r, 0, 'f', 2)
                .arg(mu, 0, 'f', 2)
                .arg(d + r, 0, 'f', 2)
                .arg(mu, 0, 'f', 2);
        }
        return "Range traversal uses pruning inequalities with mu to skip impossible branches.";
    }
    if (t == "-> match accepted" || t == "  -> match accepted") {
        return "A stored n-gram satisfies the distance threshold and is returned as a match.";
    }
    if (t.startsWith("search_knn(start")) {
        return "KNN begins: maintain up to k nearest neighbors and track tau (current worst distance in the top-k set).";
    }
    if (t.startsWith("knn visit ")) {
        QRegularExpression rx("dist=([0-9eE.+-]+).*tau=([0-9eE.+-]+).*mu=([0-9eE.+-]+)");
        QRegularExpressionMatch m = rx.match(t);
        if (m.hasMatch()) {
            double d = m.captured(1).toDouble();
            double tau = m.captured(2).toDouble();
            double mu = m.captured(3).toDouble();
            return QString("Math: d=%1, tau=%2, mu=%3. Branch checks use d-tau<=mu and d+tau>mu.")
                .arg(d, 0, 'f', 2)
                .arg(tau, 0, 'f', 2)
                .arg(mu, 0, 'f', 2);
        }
        return "KNN traversal prunes branches using tau to avoid unnecessary comparisons.";
    }
    if (t.startsWith("knn insert candidate")) {
        return "Candidate neighbor inserted, then farthest item may be dropped to keep only k elements.";
    }
    if (t.startsWith("removed farthest neighbor")) {
        return "Top-k maintenance: current farthest neighbor is removed to preserve best k matches.";
    }
    if (t.startsWith("tau updated=")) {
        return "Tau update: tau = max distance among retained k neighbors.";
    }
    if (t.startsWith("contains(check")) {
        return "Read operation: linear scan of stored nodes checks exact NGram identity.";
    }
    if (t.startsWith("update(start")) {
        return "Update operation: locate old record, replace content, then rebuild to preserve VP-tree invariants.";
    }
    if (t.startsWith("remove(start")) {
        return "Delete operation: filter out target element, then rebuild tree to keep partition consistency.";
    }
    if (t.startsWith("rebuild(start")) {
        return "Rebuild operation: collect all nodes, clear pointers, reconstruct balanced metric partitions.";
    }
    if (t.startsWith("clear(start")) {
        return "Clear operation: post-order delete of nodes releases raw-pointer tree memory.";
    }
    if (t.startsWith("- vp=\"")) {
        return "Storage snapshot: each node stores (vantage point text, mu, left pointer, right pointer).";
    }
    if (t == "- null") {
        return "Null child pointer encountered in the current tree snapshot.";
    }
    return treeForwarded
        ? "VP-tree internal event generated while plagiarism pipeline is scanning n-grams."
        : "Backend trace event for this operation step.";
}
// Function: appendTraceStep
void MainWindow::appendTraceStep(int index) {
    if (index < 0 || index >= opsTraceLines_.count()) return;
    opsTraceDisplay_->append(opsTraceLines_[index]);
    opsExplainDisplay_->setPlainText(opsTraceExplainLines_[index]);
    opsStepLabel_->setText(
        QString("Step %1 / %2")
            .arg(index + 1)
            .arg(opsTraceLines_.count()));
    int pct = ((index + 1) * 100) / opsTraceLines_.count();
    opsTraceProgress_->setValue(pct);
    QTextCursor cursor = opsTraceDisplay_->textCursor();
    cursor.movePosition(QTextCursor::End);
    opsTraceDisplay_->setTextCursor(cursor);
}
// Function: startTracePlayback
void MainWindow::startTracePlayback(const QStringList& lines) {
    if (!opsPlaybackTimer_) return;
    if (opsPlaybackTimer_->isActive()) {
        opsPlaybackTimer_->stop();
    }
    opsTraceLines_ = lines;
    opsTraceExplainLines_.clear();
    for (int i = 0; i < opsTraceLines_.count(); ++i) {
        opsTraceExplainLines_.append(explainTraceLine(opsTraceLines_[i]));
    }
    opsTraceIndex_ = 0;
    opsPlaybackPaused_ = false;
    opsTraceDisplay_->clear();
    opsExplainDisplay_->clear();
    opsTraceProgress_->setValue(0);
    opsStepLabel_->setText(
        QString("Step 0 / %1").arg(opsTraceLines_.count()));
    opsPlayPauseBtn_->setText("Pause");
    if (opsTraceLines_.isEmpty()) return;
    appendTraceStep(0);
    opsTraceIndex_ = 1;
    if (opsTraceIndex_ >= opsTraceLines_.count()) {
        opsTraceProgress_->setValue(100);
        statusBar()->showMessage("Backend trace playback complete.", 4000);
        return;
    }
    opsPlaybackTimer_->setInterval(opsSpeedSlider_->value());
    opsPlaybackTimer_->start();
}
// Function: onTracePlaybackTick
void MainWindow::onTracePlaybackTick() {
    if (!opsPlaybackTimer_) return;
    if (opsTraceIndex_ >= opsTraceLines_.count()) {
        opsPlaybackTimer_->stop();
        opsPlaybackPaused_ = true;
        opsPlayPauseBtn_->setText("Replay");
        opsTraceProgress_->setValue(100);
        statusBar()->showMessage("Backend trace playback complete.", 4000);
        return;
    }
    appendTraceStep(opsTraceIndex_);
    ++opsTraceIndex_;
    if (opsTraceIndex_ >= opsTraceLines_.count()) {
        opsPlaybackTimer_->stop();
        opsPlaybackPaused_ = true;
        opsPlayPauseBtn_->setText("Replay");
        opsTraceProgress_->setValue(100);
        statusBar()->showMessage("Backend trace playback complete.", 4000);
    }
}
// Function: onTracePlayPause
void MainWindow::onTracePlayPause() {
    if (!opsPlaybackTimer_) return;
    if (opsTraceLines_.isEmpty()) return;
    if (opsTraceIndex_ >= opsTraceLines_.count()) {
        startTracePlayback(opsTraceLines_);
        return;
    }
    if (opsPlaybackTimer_->isActive()) {
        opsPlaybackTimer_->stop();
        opsPlaybackPaused_ = true;
        opsPlayPauseBtn_->setText("Play");
        statusBar()->showMessage("Backend trace paused.", 2000);
        return;
    }
    opsPlaybackTimer_->setInterval(opsSpeedSlider_->value());
    opsPlaybackTimer_->start();
    opsPlaybackPaused_ = false;
    opsPlayPauseBtn_->setText("Pause");
}
// Function: onTraceNextStep
void MainWindow::onTraceNextStep() {
    if (opsTraceLines_.isEmpty()) return;
    if (opsPlaybackTimer_ && opsPlaybackTimer_->isActive()) {
        opsPlaybackTimer_->stop();
    }
    if (opsTraceIndex_ >= opsTraceLines_.count()) {
        statusBar()->showMessage("Trace is already complete.", 2000);
        return;
    }
    appendTraceStep(opsTraceIndex_);
    ++opsTraceIndex_;
    opsPlaybackPaused_ = true;
    opsPlayPauseBtn_->setText("Play");
    if (opsTraceIndex_ >= opsTraceLines_.count()) {
        opsTraceProgress_->setValue(100);
        opsPlayPauseBtn_->setText("Replay");
        statusBar()->showMessage("Backend trace playback complete.", 4000);
    }
}
// Function: onTraceSpeedChanged
void MainWindow::onTraceSpeedChanged(int value) {
    opsSpeedLabel_->setText(QString("Step delay: %1 ms").arg(value));
    if (opsPlaybackTimer_) {
        opsPlaybackTimer_->setInterval(value);
    }
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
