#ifndef NGRAM_H
#define NGRAM_H

#include <string>

// ============================================================================
// NGram — A document segment (shingle) object stored within VP-Tree nodes.
// Each NGram captures a contiguous window of words from a source document,
// along with metadata for tracing matches back to the original text.
// ============================================================================
class NGram {
public:
    std::string text;           // The n-gram content (concatenated words)
    std::string sourceFile;     // Originating filename
    int         startPos;       // Character offset in the original document
    int         endPos;         // End character offset
    int         ngramIndex;     // Sequential index of this n-gram in its document

    NGram();
    NGram(const std::string& text,
          const std::string& sourceFile,
          int startPos, int endPos,
          int ngramIndex);

    bool operator==(const NGram& o) const;
    bool operator!=(const NGram& o) const;
};

// ============================================================================
// SearchResult — Returned by VP-Tree queries, pairs an NGram with its distance.
// ============================================================================
struct SearchResult {
    NGram  ngram;
    double distance;

    SearchResult() : distance(0.0) {}
    SearchResult(const NGram& n, double d) : ngram(n), distance(d) {}
};

// ============================================================================
// MatchSegment — A highlighted region in the query document.
// ============================================================================
struct MatchSegment {
    int         start;          // Character start in query document
    int         end;            // Character end in query document
    std::string matchedFile;    // Source file of the match
    double      similarity;     // 1.0 - normalized_distance

    MatchSegment() : start(0), end(0), similarity(0.0) {}
    MatchSegment(int s, int e, const std::string& f, double sim)
        : start(s), end(e), matchedFile(f), similarity(sim) {}
};

#endif // NGRAM_H
