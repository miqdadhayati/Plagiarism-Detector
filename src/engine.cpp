#include "engine.hpp"
#include <fstream>
#include <sstream>
#include <cctype>
#include <algorithm>
#include <cstring>

// ============================================================================
// TEXT PREPROCESSING
// ============================================================================

std::string PlagiarismEngine::normalizeText(const std::string& raw) {
    std::string result;
    result.reserve(raw.size());

    bool lastWasSpace = false;
    for (char c : raw) {
        if (std::isalnum(static_cast<unsigned char>(c))) {
            result += static_cast<char>(
                std::tolower(static_cast<unsigned char>(c)));
            lastWasSpace = false;
        } else if (std::isspace(static_cast<unsigned char>(c)) ||
                   c == '-' || c == '\'') {
            if (!lastWasSpace && !result.empty()) {
                result += ' ';
                lastWasSpace = true;
            }
        }
        // Other punctuation is stripped
    }

    // Trim trailing space
    if (!result.empty() && result.back() == ' ')
        result.pop_back();

    return result;
}

std::string PlagiarismEngine::applyWhitelist(const std::string& text) const {
    if (whitelist_.count() == 0) return text;

    std::string result = text;
    for (int i = 0; i < whitelist_.count(); ++i) {
        std::string lower;
        for (char c : whitelist_[i])
            lower += static_cast<char>(
                std::tolower(static_cast<unsigned char>(c)));

        size_t pos = 0;
        while ((pos = result.find(lower, pos)) != std::string::npos) {
            result.erase(pos, lower.size());
        }
    }

    // Collapse multiple spaces
    std::string collapsed;
    bool lastSpace = false;
    for (char c : result) {
        if (c == ' ') {
            if (!lastSpace) { collapsed += ' '; lastSpace = true; }
        } else {
            collapsed += c;
            lastSpace = false;
        }
    }
    if (!collapsed.empty() && collapsed.back() == ' ')
        collapsed.pop_back();
    if (!collapsed.empty() && collapsed.front() == ' ')
        collapsed.erase(collapsed.begin());

    return collapsed;
}

RawBuffer<std::string> PlagiarismEngine::tokenize(const std::string& text) {
    RawBuffer<std::string> words;
    std::string word;

    for (size_t i = 0; i <= text.size(); ++i) {
        if (i == text.size() || text[i] == ' ') {
            if (!word.empty()) {
                words.append(std::move(word));
                word.clear();
            }
        } else {
            word += text[i];
        }
    }
    return words;
}

// ============================================================================
// N-GRAM CREATION
// ============================================================================

RawBuffer<NGram> PlagiarismEngine::createNgrams(
    const std::string& originalText,
    const std::string& cleanedText,
    const std::string& sourceFile) const
{
    RawBuffer<NGram> ngrams;
    RawBuffer<std::string> words = tokenize(cleanedText);

    if (words.count() < ngramSize_) {
        // If document is shorter than n-gram size, treat whole doc as one gram
        if (words.count() > 0) {
            ngrams.append(NGram(cleanedText, sourceFile, 0,
                                static_cast<int>(originalText.size()), 0));
        }
        return ngrams;
    }

    // Build a position map: for each word index, find its start in cleanedText
    int* wordStarts = new int[words.count()];
    int pos = 0;
    for (int i = 0; i < words.count(); ++i) {
        wordStarts[i] = pos;
        pos += static_cast<int>(words[i].size()) + 1; // +1 for space
    }

    for (int i = 0; i <= words.count() - ngramSize_; ++i) {
        // Build n-gram text
        std::string ngramText;
        for (int j = 0; j < ngramSize_; ++j) {
            if (j > 0) ngramText += ' ';
            ngramText += words[i + j];
        }

        int cleanStart = wordStarts[i];
        int lastWordIdx = i + ngramSize_ - 1;
        int cleanEnd = wordStarts[lastWordIdx]
                       + static_cast<int>(words[lastWordIdx].size());

        // Map back to original text positions
        int origStart = 0, origEnd = static_cast<int>(originalText.size());
        mapPositions(originalText, cleanedText,
                     cleanStart, cleanEnd, origStart, origEnd);

        ngrams.append(NGram(ngramText, sourceFile, origStart, origEnd, i));
    }

    delete[] wordStarts;
    return ngrams;
}

void PlagiarismEngine::mapPositions(const std::string& original,
                                     const std::string& cleaned,
                                     int cleanStart, int cleanEnd,
                                     int& origStart, int& origEnd) {
    // Build a mapping from cleaned-text indices to original-text indices.
    // We walk both strings in parallel to establish correspondence.
    std::string normOrig = normalizeText(original);

    // Simple heuristic: use ratio-based mapping
    double ratio = (cleaned.empty()) ? 0.0
        : static_cast<double>(original.size()) / cleaned.size();

    origStart = static_cast<int>(cleanStart * ratio);
    origEnd   = static_cast<int>(cleanEnd * ratio);

    // Clamp
    if (origStart < 0) origStart = 0;
    if (origEnd > static_cast<int>(original.size()))
        origEnd = static_cast<int>(original.size());

    // Snap to word boundaries in original
    while (origStart > 0 &&
           !std::isspace(static_cast<unsigned char>(original[origStart - 1])))
        --origStart;

    while (origEnd < static_cast<int>(original.size()) &&
           !std::isspace(static_cast<unsigned char>(original[origEnd])))
        ++origEnd;
}

// ============================================================================
// FILE I/O
// ============================================================================

std::string PlagiarismEngine::readFile(const std::string& path) {
    std::ifstream ifs(path);
    if (!ifs.is_open()) return "";

    std::ostringstream oss;
    oss << ifs.rdbuf();
    return oss.str();
}

// ============================================================================
// PUBLIC INTERFACE
// ============================================================================

PlagiarismEngine::PlagiarismEngine()
    : ngramSize_(4) {}

void PlagiarismEngine::setNgramSize(int n) {
    if (n >= 2 && n <= 10) ngramSize_ = n;
}

void PlagiarismEngine::addWhitelistWord(const std::string& word) {
    if (!word.empty())
        whitelist_.append(word);
}

void PlagiarismEngine::removeWhitelistWord(int index) {
    whitelist_.removeAt(index);
}

void PlagiarismEngine::clearWhitelist() {
    whitelist_.clear();
}

bool PlagiarismEngine::addFile(const std::string& filePath) {
    // Read file
    std::string raw = readFile(filePath);
    if (raw.empty()) return false;

    // Extract filename from path
    std::string filename = filePath;
    size_t sep = filePath.find_last_of("/\\");
    if (sep != std::string::npos)
        filename = filePath.substr(sep + 1);

    // Normalize and clean
    std::string normalized = normalizeText(raw);
    std::string cleaned    = applyWhitelist(normalized);

    // Create n-grams
    RawBuffer<NGram> ngrams = createNgrams(raw, cleaned, filename);

    if (ngrams.count() == 0) return false;

    // Insert into VP-Tree
    if (tree_.empty() && ngrams.count() > 1) {
        // Batch build for first file
        tree_.build_tree(ngrams.rawData(), ngrams.count());
    } else {
        // Incremental insert
        for (int i = 0; i < ngrams.count(); ++i)
            tree_.insert(ngrams[i]);
    }

    indexedFiles_.append(FileRecord(filename, filePath, ngrams.count()));
    return true;
}

void PlagiarismEngine::removeFile(int index) {
    if (index < 0 || index >= indexedFiles_.count()) return;

    std::string fileToRemove = indexedFiles_[index].filename;
    indexedFiles_.removeAt(index);

    // Must rebuild tree without removed file's n-grams
    // Collect all remaining items
    RawBuffer<NGram> all;
    // Helper lambda captured manually: traverse tree, keep non-matching
    struct Collector {
        static void collect(VPTreeNode* node, const std::string& exclude,
                            RawBuffer<NGram>& out) {
            if (!node) return;
            if (node->vantagePoint.sourceFile != exclude)
                out.append(node->vantagePoint);
            collect(node->left, exclude, out);
            collect(node->right, exclude, out);
        }
    };

    Collector::collect(tree_.root(), fileToRemove, all);
    tree_.clear();
    if (all.count() > 0)
        tree_.build_tree(all.rawData(), all.count());
}

// ============================================================================
// SCAN — The main plagiarism detection routine
// ============================================================================
ScanReport PlagiarismEngine::scan(const std::string& queryText,
                                   const std::string& queryFilename,
                                   double radius) const {
    ScanReport report;

    if (tree_.empty() || queryText.empty()) return report;

    // Preprocess query
    std::string normalized = normalizeText(queryText);
    std::string cleaned    = applyWhitelist(normalized);

    // Create query n-grams
    // We need a non-const engine for createNgrams since it uses ngramSize_
    // but the method is const — it reads ngramSize_ which is fine.
    RawBuffer<std::string> words = tokenize(cleaned);
    if (words.count() < ngramSize_) return report;

    // Build position map
    int* wordStarts = new int[words.count()];
    int pos = 0;
    for (int i = 0; i < words.count(); ++i) {
        wordStarts[i] = pos;
        pos += static_cast<int>(words[i].size()) + 1;
    }

    // Track which characters are matched
    int textLen = static_cast<int>(queryText.size());
    bool* matched = new bool[textLen];
    for (int i = 0; i < textLen; ++i)
        matched[i] = false;

    // For each n-gram, do range query
    int totalNgrams   = 0;
    int matchedNgrams = 0;

    for (int i = 0; i <= words.count() - ngramSize_; ++i) {
        ++totalNgrams;

        std::string ngramText;
        for (int j = 0; j < ngramSize_; ++j) {
            if (j > 0) ngramText += ' ';
            ngramText += words[i + j];
        }

        NGram queryNgram(ngramText, queryFilename, wordStarts[i], 0, i);

        RawBuffer<SearchResult> results = tree_.range_query(queryNgram, radius);

        if (results.count() > 0) {
            ++matchedNgrams;

            // Find best match (smallest distance)
            int bestIdx = 0;
            for (int r = 1; r < results.count(); ++r) {
                if (results[r].distance < results[bestIdx].distance)
                    bestIdx = r;
            }

            // Map to original text positions
            int cleanStart = wordStarts[i];
            int lastWordIdx = i + ngramSize_ - 1;
            int cleanEnd = wordStarts[lastWordIdx]
                           + static_cast<int>(words[lastWordIdx].size());

            int origStart = 0, origEnd = textLen;
            mapPositions(queryText, cleaned, cleanStart, cleanEnd,
                         origStart, origEnd);

            // Clamp to valid range
            if (origStart < 0) origStart = 0;
            if (origEnd > textLen) origEnd = textLen;

            // Mark characters as matched
            for (int c = origStart; c < origEnd; ++c)
                matched[c] = true;

            // Record segment
            double similarity = 1.0 - results[bestIdx].distance;
            report.segments.append(
                MatchSegment(origStart, origEnd,
                             results[bestIdx].ngram.sourceFile, similarity));

            // Track unique matched files
            bool found = false;
            for (int f = 0; f < report.matchedFiles.count(); ++f) {
                if (report.matchedFiles[f] ==
                    results[bestIdx].ngram.sourceFile) {
                    found = true;
                    break;
                }
            }
            if (!found)
                report.matchedFiles.append(results[bestIdx].ngram.sourceFile);
        }
    }

    // Calculate match percentage based on characters
    int matchedChars = 0;
    for (int i = 0; i < textLen; ++i)
        if (matched[i]) ++matchedChars;

    report.matchPercentage = (textLen > 0)
        ? (static_cast<double>(matchedChars) / textLen) * 100.0
        : 0.0;

    delete[] wordStarts;
    delete[] matched;

    return report;
}
