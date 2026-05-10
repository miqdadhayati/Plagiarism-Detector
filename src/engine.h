#ifndef ENGINE_H
#define ENGINE_H

#include "ngram.h"
#include "vptree.h"
#include "rawbuffer.h"
#include <string>
#include <functional>
#include <unordered_map>
#include <vector>

// Indexed file metadata.
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

// Scan result summary.
struct ScanReport {
    double                    matchPercentage;
    RawBuffer<MatchSegment>   segments;        // Highlighted regions
    RawBuffer<std::string>    matchedFiles;    // Unique source files matched

    ScanReport() : matchPercentage(0.0) {}
};

// Per-source ranking score.
struct SourceScore {
    std::string sourceFile;
    int         matchedNgrams;
    double      coveragePercent;
    double      avgSimilarity;

    SourceScore()
        : matchedNgrams(0), coveragePercent(0.0), avgSimilarity(0.0) {}
    explicit SourceScore(const std::string& f)
        : sourceFile(f), matchedNgrams(0), coveragePercent(0.0), avgSimilarity(0.0) {}
};

// A single synonym substitution record for display.
struct SynonymMapping {
    std::string originalWord;   // Surface form in the text
    std::string rootToken;      // Canonical root it was mapped to

    SynonymMapping() {}
    SynonymMapping(const std::string& orig, const std::string& root)
        : originalWord(orig), rootToken(root) {}
};

// One entry in the synonym dictionary: root + all its synonyms.
struct SynonymDictEntry {
    std::string              rootToken;
    std::vector<std::string> synonyms;
};

#if defined(__has_include)
#  if __has_include(<QMetaType>)
#    include <QMetaType>
#    define PLAGIARISM_HAVE_QT_META 1
#  endif
#endif

#if defined(PLAGIARISM_HAVE_QT_META)
Q_DECLARE_METATYPE(ScanReport)
#endif

// Main plagiarism indexing and scanning engine.
class PlagiarismEngine {
private:
    VPTree                  tree_;
    RawBuffer<FileRecord>   indexedFiles_;
    RawBuffer<std::string>  boilerplate_;   // Auto-detected phrases from document commonalities
    RawBuffer<std::string>  whitelist_;     // User-specified words/phrases to ignore
    int                     ngramSize_;     // Number of words per n-gram
    double                  boilerplateThreshold_;  // Fraction of docs a phrase must appear in
    mutable std::function<void(const std::string&)> traceCallback_;
    mutable bool            traceDataStructure_;

    // Thesaurus Catcher: synonym -> root-token table.
    // Lookup is O(1) average via std::unordered_map; total space is O(V),
    // where V is the loaded vocabulary. Keys are stored lower-cased so that
    // applySynonyms() can hash incoming tokens directly without copying the
    // dictionary at query time.
    std::unordered_map<std::string, std::string> synonymMap_;

    void emitTrace(const std::string& message) const;

    // Normalize text.
    static std::string normalizeText(const std::string& raw);

    // Remove whitelist terms.
    std::string applyWhitelist(const std::string& text) const;

    // Remove boilerplate phrases.
    std::string applyBoilerplate(const std::string& text) const;

    // Remove terms with strict word/phrase boundaries.
    std::string removeTermsStrict(
        const std::string& text,
        const RawBuffer<std::string>& terms) const;

    // Split text into words. Each token is normalized through applySynonyms()
    // before it is appended, so semantically-equivalent surface forms collapse
    // to a shared root token before n-gram construction. This is a non-static
    // const member because it must read synonymMap_.
    RawBuffer<std::string> tokenize(const std::string& text) const;

    // Synonym normalization function T : W -> R.
    // Returns the root token associated with `word` if w in W; otherwise
    // returns `word` unchanged (the identity branch of T). Lookup is O(1)
    // average. The input is lower-cased internally before hashing so the
    // function is case-insensitive at the boundary.
    std::string applySynonyms(const std::string& word) const;

    // Build n-grams with position mapping.
    RawBuffer<NGram> createNgrams(const std::string& originalText,
                                   const std::string& cleanedText,
                                   const std::string& sourceFile) const;

    // Map cleaned positions to original text offsets.
    static void mapPositions(const std::string& original,
                             const std::string& cleaned,
                             int cleanStart, int cleanEnd,
                             int& origStart, int& origEnd);

public:
    PlagiarismEngine();
    ~PlagiarismEngine() = default;

    // Set n-gram window size.
    void setNgramSize(int n);
    int  ngramSize() const { return ngramSize_; }

    // Whitelist operations.
    void addWhitelistWord(const std::string& word);
    void removeWhitelistWord(int index);
    void clearWhitelist();
    const RawBuffer<std::string>& whitelist() const { return whitelist_; }

    // Boilerplate: auto-detected from document commonalities.
    // Scans all indexed files and finds n-grams that appear in >= threshold
    // fraction of documents. Those common phrases are treated as boilerplate.
    // Returns the number of boilerplate phrases detected.
    int detectBoilerplate();
    const RawBuffer<std::string>& boilerplate() const { return boilerplate_; }
    void setBoilerplateThreshold(double t);
    double boilerplateThreshold() const { return boilerplateThreshold_; }

    // Load a synonym dictionary from disk. Each line of the file is a CSV
    // record formatted as:
    //     root_token,synonym1,synonym2,synonym3,...
    // Blank lines and lines beginning with '#' are ignored. Synonym keys are
    // folded to lower case before insertion. The first column is preserved
    // verbatim as the value, so callers may use canonical sentinels such as
    // "SPEED_ADJ" without case mangling. Returns true if the file was opened
    // and parsed successfully; false on I/O failure.
    bool loadSynonymDictionary(const std::string& filePath);

    // Synonym dictionary stats.
    int  synonymCount() const { return static_cast<int>(synonymMap_.size()); }
    bool synonymsLoaded() const { return !synonymMap_.empty(); }

    // Get the full synonym dictionary grouped by root token, for display.
    std::vector<SynonymDictEntry> getSynonymDictionary() const;

    // Analyse text and report every word that gets synonym-mapped.
    // Returns one SynonymMapping per substitution found.
    std::vector<SynonymMapping> getSynonymMappingReport(const std::string& text) const;

    // Add and index a file.
    bool addFile(const std::string& filePath);

    // Rebuild index from the currently indexed file paths using active filters.
    bool rebuildIndexWithFilters();

    // Remove indexed file and rebuild tree.
    void removeFile(int index);

    // Access indexed files.
    const RawBuffer<FileRecord>& indexedFiles() const { return indexedFiles_; }

    // Read text file.
    static std::string readFile(const std::string& path);

    // Normalize + boilerplate strip + whitelist strip.
    std::string preprocessText(const std::string& raw) const;

    // Scan query text against indexed data.
    ScanReport scan(const std::string& queryText,
                    const std::string& queryFilename,
                    double radius) const;

    // Rank matched sources by query n-gram hits.
    RawBuffer<SourceScore> rankSources(const std::string& queryText,
                                       const std::string& queryFilename,
                                       double radius,
                                       int topK) const;

    // Set trace callback.
    void setPipelineTraceCallback(const std::function<void(const std::string&)>& cb) const;

    // Clear trace callback.
    void clearPipelineTraceCallback() const;

    // Enable or disable tree-level tracing.
    void setDataStructureTraceEnabled(bool enabled) const;

    // Tree stats.
    int  treeSize()   const { return tree_.size(); }
    int  treeHeight() const { return VPTree::get_height(tree_.root()); }
    bool treeEmpty()  const { return tree_.empty(); }
};

#endif // ENGINE_H
