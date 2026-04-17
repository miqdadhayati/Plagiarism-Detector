# VP-Tree Plagiarism Detector

A standalone C++17 desktop application for plagiarism detection, powered by a **Vantage-Point Tree (VP-Tree)** built entirely with raw pointers and manual memory management — no `std::vector`, `std::list`, `std::map`, or any other forbidden container.

---

## Architecture

```
┌──────────────────────────────────────────────────────────────┐
│                        MainWindow (Qt)                       │
│  ┌──────────┐  ┌────────────────────┐  ┌──────────────────┐  │
│  │ Database  │  │   Heatmap Display  │  │  Match Report    │  │
│  │ File List │  │  (red = plagiarism)│  │  % + Sources     │  │
│  │           │  │                    │  │                  │  │
│  │ Whitelist │  │                    │  │  Matched Files   │  │
│  │ Input     │  │                    │  │  List            │  │
│  │           │  │                    │  │                  │  │
│  │ Strictness│  │                    │  │                  │  │
│  │ Slider    │  │                    │  │                  │  │
│  └──────────┘  └────────────────────┘  └──────────────────┘  │
└───────────────────────┬──────────────────────────────────────┘
                        │
              ┌─────────▼──────────┐
              │ PlagiarismEngine   │
              │  - Text normalize  │
              │  - Whitelist strip │
              │  - N-gram creation │
              │  - Scan orchestrate│
              └─────────┬──────────┘
                        │
                ┌───────▼────────┐
                │    VPTree      │
                │  - build_tree  │   ◄── All nodes use VPTreeNode*
                │  - range_query │       (raw pointers, new/delete)
                │  - search_knn  │
                │  - insert      │   ◄── RawBuffer<T> for temp storage
                │  - distance    │       (manual memory management)
                └────────────────┘
```

### Core Components

| File | Role |
|------|------|
| `rawbuffer.h` | Template dynamic buffer using `new[]`/`delete[]` — replaces `std::vector` |
| `ngram.h/cpp` | `NGram` class — document segment objects stored in tree nodes |
| `vptree.h/cpp` | Full VP-Tree: select VP, median partition, build, range + KNN search, CRUD, rebuild |
| `engine.h/cpp` | Document parsing, whitelist, n-gram creation, scan orchestration |
| `mainwindow.h/cpp` | Qt GUI with heatmap, slider, database view, report panel |

## Assessment Demo (Cut-Down Flow)

This project now includes a quick demo path focused on a testable and visual proof-of-function.

1. Launch the app.
2. Click **Load Quick Demo**.
3. The app auto-loads sample database files and scans the sample suspicious file.
4. Show the heatmap, match %, source list, and tree stats.
5. Click **Visualize DS Ops** to show pass/fail bars and a step-by-step backend trace replay.
6. Use the **Backend Operation Trace** panel to explain recursion, partitioning,
   range/KNN traversal, and tree rebuild behavior.
7. Use **Pause**, **Next Step**, and the **Step delay** slider to control pace,
   and read the per-step math/storage explanation box while presenting.

This keeps the live demo short while still showing working core behavior.

### Data Structure CRUD Coverage (VP-Tree)

| CRUD | Method | Status |
|------|--------|--------|
| Create | `build_tree()`, `insert()` | Complete |
| Read | `contains()`, `range_query()`, `search_knn()` | Complete |
| Update | `update(oldItem, newItem)` | Complete |
| Delete | `remove(item)`, `clear()` | Complete |

### Demo Feature Subset (30%+ Running)

The demo-ready subset includes:

1. Database indexing from files.
2. VP-tree search and plagiarism scoring.
3. Heatmap visualization of matched regions.
4. Source file match report.
5. Strictness slider with re-scan.

### VP-Tree Methods

| Method | Description |
|--------|-------------|
| `select_vantage_point()` | Samples candidates, picks highest-variance spread |
| `calculate_median_distance()` | Quickselect-based median of distances from VP |
| `build_tree()` | Recursive partitioning into left (≤μ) / right (>μ) |
| `distance_metric()` | Levenshtein edit distance (number of edits) |
| `range_query()` | Returns all items within radius, with branch pruning |
| `search_knn()` | K-nearest-neighbors with dynamic tau pruning |
| `insert()` | Inserts one item and rebuilds to preserve VP medians |
| `is_leaf()` / `get_height()` | Tree inspection utilities |
| `rebuild()` | Reconstructs tree to refresh partitions |

---

## Build Instructions

### Prerequisites

- **C++17** compatible compiler (GCC 8+, Clang 7+, MSVC 2019+)
- **Qt5** or **Qt6** development libraries (Widgets module)
- **moc** (Qt Meta-Object Compiler)
- **pkg-config**

### Install Qt (if not already installed)

**Ubuntu/Debian:**
```bash
sudo apt install qt6-base-dev qtbase5-dev build-essential pkg-config
```

**macOS (Homebrew):**
```bash
brew install qt@6 pkg-config
```

**Windows:** Install Qt from https://www.qt.io/download-qt-installer

### Build

**Windows batch files (recommended):**

```bat
cd plagiarism_detector
run_all.bat
```

`run_all.bat` builds app + tests, runs tests, then launches the app.

If you only want build + tests (no app launch):

```bat
cd plagiarism_detector
run_all.bat --no-launch
```

Or build separately:

```bat
cd plagiarism_detector
build_qt6_app.bat
build_tests.bat
run_qt6_app.bat
```

`build_qt6_app.bat` runs `windeployqt6` and copies matching Qt runtime DLLs to `build_terminal` to avoid startup DLL mismatch errors.

**Simple PowerShell g++ commands:**

```powershell
Set-Location plagiarism_detector
if (-not (Test-Path build_terminal)) { New-Item -ItemType Directory build_terminal | Out-Null }
$qtprefix = (qmake6 -query QT_INSTALL_PREFIX) -replace '/', '\\'
& "$qtprefix\share\qt6\bin\moc.exe" src/mainwindow.h -o build_terminal/moc_mainwindow.cpp
g++ -std=c++17 -O2 -Wall -Wextra -Isrc $(pkg-config --cflags Qt6Widgets) src/main.cpp src/ngram.cpp src/vptree.cpp src/engine.cpp src/mainwindow.cpp build_terminal/moc_mainwindow.cpp -o build_terminal/VPTreePlagiarismDetector.exe $(pkg-config --libs Qt6Widgets)
g++ -std=c++17 -O2 -Wall -Wextra -Isrc src/test_engine.cpp src/ngram.cpp src/vptree.cpp src/engine.cpp -o build_terminal/VPTreePlagiarismDetector_tests.exe
```

### Run

```powershell
Set-Location plagiarism_detector/build_terminal
.\VPTreePlagiarismDetector_tests.exe
.\VPTreePlagiarismDetector.exe
```

---

## Usage

1. **Add Database Files** — Click "Add File" in the left panel to index `.txt` files.
   Each file is parsed into overlapping 4-word n-grams and inserted into the VP-Tree.

2. **Configure Whitelist** — Enter common phrases (university headers, boilerplate)
   in the whitelist field. These are stripped before n-gram creation.

3. **Adjust Strictness** — Move the slider to change normalized strictness.
   Internally, this is converted to an edit-distance radius per query n-gram.
   - **Low values (0.05–0.15):** Very strict — only near-exact matches
   - **Medium (0.20–0.35):** Balanced detection
   - **High (0.40+):** Lenient — catches paraphrased content

4. **Scan a Document** — Click "Scan Document" and select a file.
   The scan runs on a background thread (GUI stays responsive).

5. **Read Results:**
   - **Heatmap:** Red-highlighted text = matched segments; white = original
   - **Match %:** Overall plagiarism percentage shown in the right panel
   - **Sources:** List of database files where matches were found
   - **Backend Trace:** During scan completion, the trace panel shows pipeline
     steps (tokenization, n-gram loops, range queries, match selection) with
     one-by-one math/storage explanations.

### Extra Sample Documents

Use these files in `sample_data` for different test scenarios:

1. `database_doc3.txt` / `database_doc4.txt` / `database_doc5.txt`:
   Extra database sources (graphs, testing, distributed systems).
2. `query_high_plag_ml_ds.txt`:
   High plagiarism query with direct overlap from doc1/doc2.
3. `query_near_copy_graph.txt`:
   Near-copy plagiarism query for graph content.
4. `query_mixed_graph_testing.txt`:
   Mixed query with partial overlap + original text.
5. `query_low_plag_original.txt`:
   Mostly original query for low-match behavior.

---

## How It Works

1. **Tokenization:** Documents are normalized (lowercase, strip punctuation)
   and split into overlapping n-grams (sliding window of N words).

2. **VP-Tree Indexing:** All n-grams are stored in a VP-Tree using
   Levenshtein edit distance as the metric.

3. **Scanning:** Each n-gram from the query document is searched against
   the tree via `range_query(radius)`. The VP-Tree's branch-and-bound
   pruning makes this significantly faster than brute-force comparison.

4. **Heatmap Generation:** Matched n-gram positions are mapped back to
   original text coordinates, and a per-character match array drives
   the red/white highlighting in the QTextEdit widget.

---

## Constraints Compliance

✅ **No forbidden containers used anywhere in logic/search/storage:**
- No `std::vector`, `std::list`, `std::stack`, `std::queue`
- No `std::unordered_map`, `std::map`, `std::set`
- No `std::priority_queue` (heap)
- All internal buffers use `RawBuffer<T>` with `new[]`/`delete[]`
- Tree nodes use raw `VPTreeNode*` pointers

✅ **OOP design** with encapsulated classes  
✅ **C++17** standard  
✅ **Threaded scanning** via `QThread` for GUI responsiveness
