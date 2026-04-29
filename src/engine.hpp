#ifndef ENGINE_H
#define ENGINE_H

#include "ngram.hpp"
#include "vptree.hpp"
#include "rawbuffer.hpp"
#include <string>

// ============================================================================
// FileRecord — Metadata for an indexed document in the database.
// ============================================================================
struct FileRecord {
    std::string filename;
    std::string fullPath;
    int         ngramCount;

    FileRecord() : ngramCount(0) {}
    FileRecord(const std::string& fn, const std::string& fp, int c)
        : filename(fn), fullPath(fp), ngramCount(c) {}

    bool operator==(const FileRecord& o) const {
        return fullPath == o.fullPath;
    }
};

// ============================================================================
// ScanReport — Results of a plagiarism scan.
// ============================================================================
struct ScanReport {
    double                    matchPercentage;
    RawBuffer<MatchSegment>   segments;        // Highlighted regions
    RawBuffer<std::string>    matchedFiles;    // Unique source files matched

    ScanReport() : matchPercentage(0.0) {}
};

// ============================================================================
// PlagiarismEngine — Orchestrates document indexing and plagiarism detection.
//
// Workflow:
//   1. Add database files → parsed into n-grams → inserted into VP-Tree
//   2. Scan a query file  → parsed into n-grams → range_query each
//   3. Matched n-grams    → mapped to character positions → heatmap + report
// ============================================================================
class PlagiarismEngine {
private:
    VPTree                  tree_;
    RawBuffer<FileRecord>   indexedFiles_;
    RawBuffer<std::string>  whitelist_;     // Words/phrases to ignore
    int                     ngramSize_;     // Number of words per n-gram

    /// Normalize text: lowercase, strip punctuation, collapse whitespace
    static std::string normalizeText(const std::string& raw);

    /// Remove whitelisted words/phrases from text
    std::string applyWhitelist(const std::string& text) const;

    /// Tokenize text into words (returns via raw buffer)
    static RawBuffer<std::string> tokenize(const std::string& text);

    /// Build n-grams from text, tracking positions in the original
    RawBuffer<NGram> createNgrams(const std::string& originalText,
                                   const std::string& cleanedText,
                                   const std::string& sourceFile) const;

    /// Map cleaned-text positions back to original-text positions
    static void mapPositions(const std::string& original,
                             const std::string& cleaned,
                             int cleanStart, int cleanEnd,
                             int& origStart, int& origEnd);

public:
    PlagiarismEngine();
    ~PlagiarismEngine() = default;

    /// Set the n-gram window size (default: 4 words)
    void setNgramSize(int n);
    int  ngramSize() const { return ngramSize_; }

    /// Whitelist management
    void addWhitelistWord(const std::string& word);
    void removeWhitelistWord(int index);
    void clearWhitelist();
    const RawBuffer<std::string>& whitelist() const { return whitelist_; }

    /// Add a document to the database (index it)
    bool addFile(const std::string& filePath);

    /// Remove a file from the database (requires rebuild)
    void removeFile(int index);

    /// Get indexed file list
    const RawBuffer<FileRecord>& indexedFiles() const { return indexedFiles_; }

    /// Read a text file from disk
    static std::string readFile(const std::string& path);

    /// Scan a query document against the database
    ScanReport scan(const std::string& queryText,
                    const std::string& queryFilename,
                    double radius) const;

    /// Tree stats
    int  treeSize()   const { return tree_.size(); }
    int  treeHeight() const { return VPTree::get_height(tree_.root()); }
    bool treeEmpty()  const { return tree_.empty(); }
};

#endif // ENGINE_H
