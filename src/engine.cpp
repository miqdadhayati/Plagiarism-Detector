#include "engine.h"
#include <fstream>
#include <sstream>
#include <cctype>
#include <algorithm>
#include <cstring>

// Function: emitTrace
void PlagiarismEngine::emitTrace(const std::string& message) const {
    if (traceCallback_) {
        traceCallback_(message);
    }
}

// Function: normalizeText
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
    }
    if (!result.empty() && result.back() == ' ')
        result.pop_back();
    return result;
}

// Function: applyWhitelist
std::string PlagiarismEngine::applyWhitelist(const std::string& text) const {
    return removeTermsStrict(text, whitelist_);
}

// Function: applyBoilerplate
std::string PlagiarismEngine::applyBoilerplate(const std::string& text) const {
    return removeTermsStrict(text, boilerplate_);
}

// Function: removeTermsStrict
std::string PlagiarismEngine::removeTermsStrict(
    const std::string& text,
    const RawBuffer<std::string>& terms) const {
    if (terms.count() == 0) return text;

    std::string result = text;
    for (int i = 0; i < terms.count(); ++i) {
        std::string term = normalizeText(terms[i]);
        if (term.empty()) continue;

        size_t pos = 0;
        while ((pos = result.find(term, pos)) != std::string::npos) {
            bool leftBoundary = (pos == 0) || (result[pos - 1] == ' ');
            size_t endPos = pos + term.size();
            bool rightBoundary =
                (endPos == result.size()) || (result[endPos] == ' ');

            if (leftBoundary && rightBoundary) {
                result.erase(pos, term.size());
            } else {
                ++pos;
            }
        }
    }

    std::string collapsed;
    collapsed.reserve(result.size());
    bool lastSpace = false;
    for (char c : result) {
        if (c == ' ') {
            if (!lastSpace && !collapsed.empty()) {
                collapsed += ' ';
                lastSpace = true;
            }
        } else {
            collapsed += c;
            lastSpace = false;
        }
    }
    if (!collapsed.empty() && collapsed.back() == ' ')
        collapsed.pop_back();
    return collapsed;
}

// Function: tokenize
// The Thesaurus Catcher interception point. Each whitespace-delimited token
// is passed through applySynonyms() the moment it is extracted, so the n-gram
// buffer downstream sees only root tokens. By collapsing synonyms before the
// VP-Tree ever computes a Levenshtein distance, we guarantee
//     D_lev(T(A), T(B)) <= D_lev(A, B)
// for any pair of n-grams A, B that are semantically identical under T.
RawBuffer<std::string> PlagiarismEngine::tokenize(const std::string& text) const {
    RawBuffer<std::string> words;
    std::string word;
    for (size_t i = 0; i <= text.size(); ++i) {
        if (i == text.size() || text[i] == ' ') {
            if (!word.empty()) {
                // Synonym normalization happens here, exactly once per token,
                // so neither createNgrams() nor scan() needs to know about
                // the dictionary. Unknown words fall through unchanged.
                std::string token = applySynonyms(word);
                words.append(std::move(token));
                word.clear();
            }
        } else {
            word += text[i];
        }
    }
    return words;
}

// Function: applySynonyms
// Implements T : W -> R. Hash lookup is O(1) on average, O(V) space, where V
// is the size of the loaded vocabulary. The function never mutates the
// caller's string and is safe to call from const contexts.
std::string PlagiarismEngine::applySynonyms(const std::string& word) const {
    // Fast path: no dictionary loaded, identity mapping.
    if (synonymMap_.empty()) return word;

    // Edge case 1 (case sensitivity): the dictionary stores lower-cased keys
    // at load time, so the lookup token must also be lower-cased before it
    // is hashed. We build the key into a local string rather than mutating
    // the input so that the original surface form survives a miss.
    std::string key;
    key.reserve(word.size());
    for (char c : word) {
        key += static_cast<char>(
            std::tolower(static_cast<unsigned char>(c)));
    }

    // Edge case 2 (missing keys): a miss yields end(); we return the original
    // word so the n-gram buffer sees it unchanged. This is the identity
    // branch of T:  w not in W  =>  T(w) = w.
    auto it = synonymMap_.find(key);
    if (it == synonymMap_.end()) {
        return word;
    }
    return it->second;
}

// Function: loadSynonymDictionary
// Parses a CSV-style thesaurus file. Each non-comment, non-blank line is
//     root_token,synonym1,synonym2,...
// and produces N entries in the hash map (one per synonym) all pointing to
// the root token string. Duplicate synonyms across lines follow last-write-
// wins semantics, which lets a caller override a mapping by re-declaring it
// later in the file.
bool PlagiarismEngine::loadSynonymDictionary(const std::string& filePath) {
    std::ifstream ifs(filePath);
    if (!ifs.is_open()) {
        emitTrace("synonym-dict load failed reason=cannot-open path=\""
                  + filePath + "\"");
        return false;
    }

    int linesParsed   = 0;
    int entriesAdded  = 0;
    std::string line;

    while (std::getline(ifs, line)) {
        // Strip trailing CR for Windows-authored files (CRLF line endings).
        if (!line.empty() && line.back() == '\r') line.pop_back();

        // Skip leading whitespace; classify the line.
        size_t lstart = 0;
        while (lstart < line.size() &&
               std::isspace(static_cast<unsigned char>(line[lstart])))
            ++lstart;
        if (lstart >= line.size()) continue;   // blank line
        if (line[lstart] == '#')   continue;   // comment

        // Split the line on commas. Each field is trimmed of surrounding
        // whitespace so dictionaries can be authored with human-friendly
        // spacing like "SPEED_ADJ, quick, fast, rapid".
        RawBuffer<std::string> fields;
        std::string field;
        for (size_t i = lstart; i <= line.size(); ++i) {
            if (i == line.size() || line[i] == ',') {
                size_t fs = 0, fe = field.size();
                while (fs < fe &&
                       std::isspace(static_cast<unsigned char>(field[fs])))
                    ++fs;
                while (fe > fs &&
                       std::isspace(static_cast<unsigned char>(field[fe - 1])))
                    --fe;
                fields.append(field.substr(fs, fe - fs));
                field.clear();
            } else {
                field += line[i];
            }
        }

        // Need at least one root + one synonym for the line to be useful.
        if (fields.count() < 2) continue;

        const std::string& rootToken = fields[0];
        if (rootToken.empty()) continue;

        // Insert each synonym -> root mapping. Keys are folded to lower
        // case (edge case 1). The root value is stored verbatim so
        // callers can use uppercase sentinels such as "SPEED_ADJ" that
        // are guaranteed not to collide with real lowercased input tokens.
        for (int j = 1; j < fields.count(); ++j) {
            const std::string& syn = fields[j];
            if (syn.empty()) continue;

            std::string key;
            key.reserve(syn.size());
            for (char c : syn) {
                key += static_cast<char>(
                    std::tolower(static_cast<unsigned char>(c)));
            }
            synonymMap_[key] = rootToken;   // last-write-wins on duplicates
            ++entriesAdded;
        }
        ++linesParsed;
    }

    emitTrace("synonym-dict loaded path=\"" + filePath + "\""
              + " lines=" + std::to_string(linesParsed)
              + " entries=" + std::to_string(entriesAdded)
              + " mapSize=" + std::to_string(synonymMap_.size()));
    return true;
}

// Function: createNgrams
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
        std::string ngramText;
        for (int j = 0; j < ngramSize_; ++j) {
            if (j > 0) ngramText += ' ';
            ngramText += words[i + j];
        }
        int cleanStart = wordStarts[i];
        int lastWordIdx = i + ngramSize_ - 1;
        int cleanEnd = wordStarts[lastWordIdx]
                       + static_cast<int>(words[lastWordIdx].size());
        int origStart = 0, origEnd = static_cast<int>(originalText.size());
        mapPositions(originalText, cleanedText,
                     cleanStart, cleanEnd, origStart, origEnd);
        ngrams.append(NGram(ngramText, sourceFile, origStart, origEnd, i));
    }
    delete[] wordStarts;
    return ngrams;
}

// Function: mapPositions
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
    while (origStart > 0 &&
           !std::isspace(static_cast<unsigned char>(original[origStart - 1])))
        --origStart;
    while (origEnd < static_cast<int>(original.size()) &&
           !std::isspace(static_cast<unsigned char>(original[origEnd])))
        ++origEnd;
}

// Function: readFile
std::string PlagiarismEngine::readFile(const std::string& path) {
    std::ifstream ifs(path);
    if (!ifs.is_open()) return "";
    std::ostringstream oss;
    oss << ifs.rdbuf();
    return oss.str();
}

// Function: preprocessText
std::string PlagiarismEngine::preprocessText(const std::string& raw) const {
    std::string normalized = normalizeText(raw);
    std::string noBoilerplate = applyBoilerplate(normalized);
    return applyWhitelist(noBoilerplate);
}

// Function: PlagiarismEngine
PlagiarismEngine::PlagiarismEngine()
    : ngramSize_(4), traceCallback_(nullptr), traceDataStructure_(false) {}
// Function: setNgramSize
void PlagiarismEngine::setNgramSize(int n) {
    if (n >= 2 && n <= 10) ngramSize_ = n;
}
// Function: addWhitelistWord
void PlagiarismEngine::addWhitelistWord(const std::string& word) {
    if (!word.empty())
        whitelist_.append(word);
}
// Function: removeWhitelistWord
void PlagiarismEngine::removeWhitelistWord(int index) {
    whitelist_.removeAt(index);
}
// Function: clearWhitelist
void PlagiarismEngine::clearWhitelist() {
    whitelist_.clear();
}

// Function: addBoilerplatePhrase
void PlagiarismEngine::addBoilerplatePhrase(const std::string& phrase) {
    if (!phrase.empty())
        boilerplate_.append(phrase);
}

// Function: removeBoilerplatePhrase
void PlagiarismEngine::removeBoilerplatePhrase(int index) {
    boilerplate_.removeAt(index);
}

// Function: clearBoilerplate
void PlagiarismEngine::clearBoilerplate() {
    boilerplate_.clear();
}
// Function: addFile
bool PlagiarismEngine::addFile(const std::string& filePath) {
    emitTrace("index start file=\"" + filePath + "\"");
    std::string raw = readFile(filePath);
    if (raw.empty()) {
        emitTrace("index failed reason=empty-or-unreadable-file");
        return false;
    }
    std::string filename = filePath;
    size_t sep = filePath.find_last_of("/\\");
    if (sep != std::string::npos)
        filename = filePath.substr(sep + 1);
    std::string cleaned    = preprocessText(raw);
    RawBuffer<NGram> ngrams = createNgrams(raw, cleaned, filename);
    emitTrace("index preprocess rawLen=" + std::to_string(raw.size())
            + " cleanedLen=" + std::to_string(cleaned.size())
            + " ngrams=" + std::to_string(ngrams.count()));
    if (ngrams.count() == 0) {
        emitTrace("index failed reason=no-ngrams-generated");
        return false;
    }
    if (tree_.empty()) {
        tree_.build_tree(ngrams.rawData(), ngrams.count());
    } else {
        // Rebuild once with existing + new n-grams to preserve VP invariants.
        RawBuffer<NGram> all;
        struct Collector {
            static void collect(VPTreeNode* node, RawBuffer<NGram>& out) {
                if (!node) return;
                out.append(node->vantagePoint);
                collect(node->left, out);
                collect(node->right, out);
            }
        };
        Collector::collect(tree_.root(), all);
        for (int i = 0; i < ngrams.count(); ++i)
            all.append(ngrams[i]);
        tree_.clear();
        tree_.build_tree(all.rawData(), all.count());
    }
    indexedFiles_.append(FileRecord(filename, filePath, ngrams.count()));
    emitTrace("index done files=" + std::to_string(indexedFiles_.count())
            + " treeSize=" + std::to_string(tree_.size())
            + " treeHeight=" + std::to_string(VPTree::get_height(tree_.root())));
    return true;
}

// Function: rebuildIndexWithFilters
bool PlagiarismEngine::rebuildIndexWithFilters() {
    RawBuffer<FileRecord> previous = indexedFiles_;
    indexedFiles_.clear();
    tree_.clear();

    bool allOk = true;
    for (int i = 0; i < previous.count(); ++i) {
        if (!addFile(previous[i].fullPath)) {
            allOk = false;
        }
    }
    return allOk;
}
// Function: removeFile
void PlagiarismEngine::removeFile(int index) {
    if (index < 0 || index >= indexedFiles_.count()) return;
    emitTrace("remove-file start index=" + std::to_string(index));
    std::string fileToRemove = indexedFiles_[index].filename;
    indexedFiles_.removeAt(index);
    // Must rebuild tree without removed file's n-grams
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
    if (all.count() > 0) {
        tree_.build_tree(all.rawData(), all.count());
    }
        emitTrace("remove-file done files=" + std::to_string(indexedFiles_.count())
            + " treeSize=" + std::to_string(tree_.size())
            + " treeHeight=" + std::to_string(VPTree::get_height(tree_.root())));
}
// Function: scan
ScanReport PlagiarismEngine::scan(const std::string& queryText,
                                   const std::string& queryFilename,
                                   double radius) const {
    ScanReport report;
    emitTrace("scan start file=\"" + queryFilename + "\" textLen="
            + std::to_string(queryText.size())
            + " radius=" + std::to_string(radius));
    if (tree_.empty() || queryText.empty()) {
        emitTrace("scan aborted reason=empty-tree-or-query");
        return report;
    }
    std::string normalized = normalizeText(queryText);
    std::string cleaned    = applyWhitelist(applyBoilerplate(normalized));
    emitTrace("scan preprocess normalizedLen=" + std::to_string(normalized.size())
            + " cleanedLen=" + std::to_string(cleaned.size()));
    // Tokenize query text before building query n-grams.
    RawBuffer<std::string> words = tokenize(cleaned);
    emitTrace("scan tokenize words=" + std::to_string(words.count())
            + " ngramSize=" + std::to_string(ngramSize_));
    if (words.count() < ngramSize_) {
        emitTrace("scan aborted reason=query-shorter-than-ngram-window");
        return report;
    }
    int totalQueryNgrams = words.count() - ngramSize_ + 1;
    emitTrace("scan query-ngrams total=" + std::to_string(totalQueryNgrams));
    int treeTraceBudget = 180;
    bool treeTraceTruncated = false;
    if (traceDataStructure_) {
        tree_.setTraceCallback([this, &treeTraceBudget, &treeTraceTruncated](const std::string& msg) {
            if (treeTraceBudget > 0) {
                emitTrace("tree " + msg);
                --treeTraceBudget;
            } else if (!treeTraceTruncated) {
                emitTrace("tree ... trace truncated ...");
                treeTraceTruncated = true;
            }
        });
    }
    int* wordStarts = new int[words.count()];
    int pos = 0;
    for (int i = 0; i < words.count(); ++i) {
        wordStarts[i] = pos;
        pos += static_cast<int>(words[i].size()) + 1;
    }
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
        // Radius is provided as normalized strictness [0,1]. Convert to
        // edit-distance units for VP-tree metric search.
        double normalizedRadius = radius;
        if (normalizedRadius < 0.0) normalizedRadius = 0.0;
        double effectiveRadius = normalizedRadius
                     * static_cast<double>(queryNgram.text.size());
        bool traceThisStep = (i < 3) || (i == totalQueryNgrams - 1)
                          || ((i % 25) == 0);
        if (traceThisStep) {
            emitTrace("scan step i=" + std::to_string(i)
                    + " effectiveRadius=" + std::to_string(effectiveRadius)
                    + " ngram=\"" + queryNgram.text + "\"");
        }
        RawBuffer<SearchResult> results = tree_.range_query(queryNgram,
                                     effectiveRadius);
        if (traceThisStep) {
            emitTrace("scan step i=" + std::to_string(i)
                    + " rangeResults=" + std::to_string(results.count()));
        }
        if (results.count() > 0) {
            ++matchedNgrams;
            int bestIdx = 0;
            for (int r = 1; r < results.count(); ++r) {
                if (results[r].distance < results[bestIdx].distance)
                    bestIdx = r;
            }
            if (traceThisStep) {
                emitTrace("scan step i=" + std::to_string(i)
                        + " bestDistance=" + std::to_string(results[bestIdx].distance)
                        + " bestSource=\"" + results[bestIdx].ngram.sourceFile + "\"");
            }
            int cleanStart = wordStarts[i];
            int lastWordIdx = i + ngramSize_ - 1;
            int cleanEnd = wordStarts[lastWordIdx]
                           + static_cast<int>(words[lastWordIdx].size());
            int origStart = 0, origEnd = textLen;
            mapPositions(queryText, cleaned, cleanStart, cleanEnd,
                         origStart, origEnd);
            if (origStart < 0) origStart = 0;
            if (origEnd > textLen) origEnd = textLen;
            for (int c = origStart; c < origEnd; ++c)
                matched[c] = true;
            int queryLen = static_cast<int>(queryNgram.text.size());
            int matchLen = static_cast<int>(results[bestIdx].ngram.text.size());
            double maxLen = static_cast<double>(queryLen > matchLen
                                                ? queryLen : matchLen);
            double normDist = (maxLen > 0.0)
                ? (results[bestIdx].distance / maxLen)
                : 0.0;
            if (normDist > 1.0) normDist = 1.0;
            double similarity = 1.0 - normDist;
            report.segments.append(
                MatchSegment(origStart, origEnd,
                             results[bestIdx].ngram.sourceFile, similarity));
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
    int matchedChars = 0;
    for (int i = 0; i < textLen; ++i)
        if (matched[i]) ++matchedChars;
    report.matchPercentage = (textLen > 0)
        ? (static_cast<double>(matchedChars) / textLen) * 100.0
        : 0.0;
    emitTrace("scan summary matchedNgrams=" + std::to_string(matchedNgrams)
            + " totalNgrams=" + std::to_string(totalNgrams)
            + " matchedChars=" + std::to_string(matchedChars)
            + " textLen=" + std::to_string(textLen));
    emitTrace("scan done matchPercentage=" + std::to_string(report.matchPercentage)
            + " matchedFiles=" + std::to_string(report.matchedFiles.count()));
    if (traceDataStructure_) {
        tree_.clearTraceCallback();
    }
    delete[] wordStarts;
    delete[] matched;
    return report;
}
// Function: setPipelineTraceCallback
void PlagiarismEngine::setPipelineTraceCallback(const std::function<void(const std::string&)>& cb) const {
    traceCallback_ = cb;
}
// Function: clearPipelineTraceCallback
void PlagiarismEngine::clearPipelineTraceCallback() const {
    traceCallback_ = nullptr;
}
// Function: setDataStructureTraceEnabled
void PlagiarismEngine::setDataStructureTraceEnabled(bool enabled) const {
    traceDataStructure_ = enabled;
}
