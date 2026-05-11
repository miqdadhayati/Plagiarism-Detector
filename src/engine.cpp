#include "engine.h"
#include <fstream>
#include <sstream>
#include <cctype>
#include <algorithm>
#include <cstring>
#include <set>
#include <map>
#include <vector>
#include <unordered_set>

namespace {

// Function: findWordBoundaries
// Walks `text` the way tokenize() splits — on ' ' delimiters, dropping empty
// runs — but records the byte offsets of each word BEFORE any synonym
// substitution is applied. The returned starts[i] is the position of the
// first character of word i; ends[i] is one past the last character.
//
// Why this exists:  tokenize() applies applySynonyms() to each token before
// appending it, so the post-synonym word strings stored in the returned
// RawBuffer no longer have the same lengths as the regions they came from
// in `text`. Any code that needs to translate a (word_index, ngram_size)
// pair back into a character span in `text` must use these PRE-synonym
// offsets, not the post-synonym string lengths.
//
// Postcondition: starts.size() == ends.size() == tokenize(text).count().
void findWordBoundaries(const std::string& text,
                        std::vector<int>& starts,
                        std::vector<int>& ends) {
    starts.clear();
    ends.clear();
    int wordStart = -1;
    int len = static_cast<int>(text.size());
    for (int i = 0; i <= len; ++i) {
        if (i == len || text[i] == ' ') {
            if (wordStart >= 0) {
                starts.push_back(wordStart);
                ends.push_back(i);
                wordStart = -1;
            }
        } else {
            if (wordStart < 0) wordStart = i;
        }
    }
}

// Function: tokenizeNoSynonym
// Pure whitespace tokenizer with NO synonym substitution. Used by
// detectBoilerplate() so the phrases stored in boilerplate_ match the
// pre-synonym surface forms that applyBoilerplate() later searches for.
// (PlagiarismEngine::tokenize() applies synonyms inline; using it here
// would store "QUICK_ADV brown fox" as boilerplate, which after
// normalizeText() collapses to "quickadv brown fox" and never matches
// anything in the input pipeline because the input never sees the
// synonym substitution before applyBoilerplate runs.)
std::vector<std::string> tokenizeNoSynonym(const std::string& text) {
    std::vector<std::string> out;
    std::string word;
    for (size_t i = 0; i <= text.size(); ++i) {
        if (i == text.size() || text[i] == ' ') {
            if (!word.empty()) {
                out.push_back(word);
                word.clear();
            }
        } else {
            word += text[i];
        }
    }
    return out;
}

} // anonymous namespace

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
//
// FIX: word-boundary table is now derived from the PRE-synonym cleanedText
// via findWordBoundaries(), not from the lengths of the post-synonym word
// strings produced by tokenize(). The old code summed post-synonym word
// sizes, so whenever a synonym substitution changed a word's length
// (e.g. "fast" -> "QUICK_ADV") all subsequent cleanStart/cleanEnd values
// pointed past the actual text and mapPositions() returned garbage offsets.
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

    // Build a position map keyed off the PRE-synonym text. wordStarts[i]
    // is the offset of word i in cleanedText; wordEnds[i] is one past
    // its last character.
    std::vector<int> wordStarts, wordEnds;
    findWordBoundaries(cleanedText, wordStarts, wordEnds);

    // Sanity check: tokenize() and findWordBoundaries() must agree on the
    // word count because they apply the same split rule. A mismatch would
    // indicate the synonym pipeline silently dropped or inserted tokens.
    int wordCount = words.count();
    if (static_cast<int>(wordStarts.size()) < wordCount) {
        // Defensive: degrade gracefully rather than scribble past the
        // boundary table. We just bail out without creating ngrams.
        return ngrams;
    }

    for (int i = 0; i <= wordCount - ngramSize_; ++i) {
        std::string ngramText;
        for (int j = 0; j < ngramSize_; ++j) {
            if (j > 0) ngramText += ' ';
            ngramText += words[i + j];
        }
        int cleanStart = wordStarts[i];
        int lastWordIdx = i + ngramSize_ - 1;
        int cleanEnd = wordEnds[lastWordIdx];
        int origStart = 0, origEnd = static_cast<int>(originalText.size());
        mapPositions(originalText, cleanedText,
                     cleanStart, cleanEnd, origStart, origEnd);
        ngrams.append(NGram(ngramText, sourceFile, origStart, origEnd, i));
    }
    return ngrams;
}

// Function: mapPositions
//
// Ratio-based mapping from cleaned-text offsets back into the original raw
// text, with both endpoints snapped outward to the nearest whitespace so
// the resulting span always aligns to whole words. Because we snap to
// space characters (0x20, single byte in UTF-8), the returned offsets are
// always safe positions for std::string::substr regardless of multi-byte
// content elsewhere.
void PlagiarismEngine::mapPositions(const std::string& original,
                                     const std::string& cleaned,
                                     int cleanStart, int cleanEnd,
                                     int& origStart, int& origEnd) {
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

PlagiarismEngine::PlagiarismEngine()
    : ngramSize_(4), boilerplateThreshold_(0.8),
      traceCallback_(nullptr), traceDataStructure_(false) {}
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

// Function: setBoilerplateThreshold
void PlagiarismEngine::setBoilerplateThreshold(double t) {
    if (t >= 0.1 && t <= 1.0) boilerplateThreshold_ = t;
}

// Function: detectBoilerplate
//
// Auto-detects phrases that appear in >= boilerplateThreshold_ fraction of
// indexed documents. These shared phrases (cover pages, question prompts,
// instructor disclaimers) are filtered out before similarity scoring so
// they don't inflate the plagiarism percentage.
//
// Algorithm: SEED + GREEDY EXTEND, in three phases.
//
//   PHASE 1 - SEED DETECTION.
//     Brute-force enumerate every contiguous word window of length
//     [SEED_MIN .. SEED_MAX] in every document, de-duplicating within each
//     document. Any phrase appearing in >= minDocs distinct documents is a
//     seed candidate. We also remember one (doc, position) anchor per
//     phrase for the extension phase.
//
//   PHASE 2 - GREEDY EXTENSION.
//     The previous revision only looked at windows of length 3-5, so any
//     common header longer than five words got reported as a handful of
//     overlapping five-word slices instead of one coherent phrase (the
//     bug visible in the screenshot of the demo dataset, whose shared
//     header normalises to ~22 words).
//
//     Now: starting from the anchor of each seed, we grow the phrase one
//     word at a time, alternately right then left, accepting each
//     extension as long as the extended sequence still occurs in >=
//     minDocs documents. The maximal common phrase is whatever survives
//     when both directions have been driven to failure. This works for
//     headers of arbitrary length and degrades to "no extension" when the
//     seed is already maximal.
//
//   PHASE 3 - SUBSUMPTION DEDUP (word-boundary aware).
//     After extension, many seeds collapse to identical maximal phrases.
//     The std::set in phase 2 takes care of exact duplicates. What's left
//     are distinct maximal phrases that may still share a head or tail
//     with each other; we drop any phrase whose entire word-sequence
//     appears inside a longer kept phrase. The check is word-boundary
//     aware (not raw char-level find), so "the cat" is correctly NOT
//     considered subsumed by "the cats sat" even though strstr would
//     match.
//
// Performance: extension calls countDocsWithSequence() repeatedly. Each
// call is sped up with a per-doc word->positions index, so the seek loop
// only visits positions where the first word of the sequence actually
// matches, instead of every position in the document. For typical
// academic datasets (10-50 docs of 1-3K words each) the whole pass
// finishes well under a second.
//
// Note: still uses tokenizeNoSynonym (no synonym substitution) so the
// phrases stored in boilerplate_ match the pre-synonym surface forms
// that applyBoilerplate() -> removeTermsStrict() later searches for.
int PlagiarismEngine::detectBoilerplate() {
    boilerplate_.clear();

    if (indexedFiles_.count() < 2) {
        emitTrace("detectBoilerplate skipped reason=less-than-2-files");
        return 0;
    }

    // Read, normalise, and tokenise each file once up-front. We hold both
    // the per-doc word vector (used everywhere downstream) and a per-doc
    // word->positions index (used to make extension cheap).
    std::vector<std::vector<std::string>> docWords;
    std::vector<std::unordered_map<std::string, std::vector<int>>> wordPositions;
    docWords.reserve(indexedFiles_.count());
    wordPositions.reserve(indexedFiles_.count());

    for (int i = 0; i < indexedFiles_.count(); ++i) {
        std::string raw = readFile(indexedFiles_[i].fullPath);
        std::string norm = normalizeText(raw);
        std::vector<std::string> words = tokenizeNoSynonym(norm);

        std::unordered_map<std::string, std::vector<int>> idx;
        for (int p = 0; p < static_cast<int>(words.size()); ++p) {
            idx[words[p]].push_back(p);
        }

        docWords.push_back(std::move(words));
        wordPositions.push_back(std::move(idx));
    }

    int numDocs = static_cast<int>(docWords.size());
    int minDocs = static_cast<int>(numDocs * boilerplateThreshold_);
    if (minDocs < 2) minDocs = 2;

    // ----------------------------------------------------------------
    // Helper: does `seq` appear as a contiguous word sequence in at
    // least `needed` documents? Returns the actual hit count so callers
    // can both verify and emit diagnostics.
    //
    // Uses the per-doc word position index to jump straight to positions
    // where the first word matches, instead of scanning every offset.
    // ----------------------------------------------------------------
    auto countDocsWithSequence =
        [&](const std::vector<std::string>& seq) -> int {
        if (seq.empty()) return 0;
        int plen = static_cast<int>(seq.size());
        int hits = 0;
        for (int d = 0; d < numDocs; ++d) {
            const auto& w = docWords[d];
            int wc = static_cast<int>(w.size());
            auto it = wordPositions[d].find(seq[0]);
            if (it == wordPositions[d].end()) continue;
            bool found = false;
            for (int pos : it->second) {
                if (pos + plen > wc) continue;
                bool match = true;
                for (int j = 1; j < plen; ++j) {
                    if (w[pos + j] != seq[j]) { match = false; break; }
                }
                if (match) { found = true; break; }
            }
            if (found) ++hits;
        }
        return hits;
    };

    // ----------------------------------------------------------------
    // Phase 1: seed detection.
    // ----------------------------------------------------------------
    // SEED_MIN of 3 keeps trivial bigrams ("of the", "in a") out of the
    // boilerplate list. SEED_MAX of 25 is large enough to catch most
    // realistic cover pages in one pass; anything longer is handled by
    // phase 2.
    const int SEED_MIN = 3;
    const int SEED_MAX = 25;

    struct SeedInfo {
        int count;      // # of distinct docs containing this seed
        int anchorDoc;  // document index of one occurrence
        int anchorPos;  // word offset of that occurrence
        int wordCount;  // number of words in the seed
    };
    std::unordered_map<std::string, SeedInfo> seeds;

    for (int d = 0; d < numDocs; ++d) {
        const auto& w = docWords[d];
        int wc = static_cast<int>(w.size());
        std::unordered_set<std::string> seenInDoc;
        int maxWs = std::min(SEED_MAX, wc);

        for (int ws = SEED_MIN; ws <= maxWs; ++ws) {
            for (int i = 0; i + ws <= wc; ++i) {
                std::string phrase;
                phrase.reserve(static_cast<size_t>(ws) * 8);
                phrase += w[i];
                for (int j = 1; j < ws; ++j) {
                    phrase += ' ';
                    phrase += w[i + j];
                }
                if (!seenInDoc.insert(phrase).second) continue;
                auto it = seeds.find(phrase);
                if (it == seeds.end()) {
                    seeds.emplace(phrase, SeedInfo{1, d, i, ws});
                } else {
                    ++it->second.count;
                }
            }
        }
    }

    // ----------------------------------------------------------------
    // Phase 2: greedy extension.
    // ----------------------------------------------------------------
    // For each seed meeting the threshold, walk outward from its anchor
    // until further extension drops below minDocs. Multiple seeds anchor
    // at different positions in the same maximal phrase but converge to
    // the same final string; the std::set absorbs the duplicates.
    std::set<std::string> maximalPhrases;

    for (const auto& kv : seeds) {
        if (kv.second.count < minDocs) continue;

        int d   = kv.second.anchorDoc;
        int pos = kv.second.anchorPos;
        int sw  = kv.second.wordCount;
        const auto& w = docWords[d];
        int wc = static_cast<int>(w.size());

        int leftEnd  = pos;        // inclusive
        int rightEnd = pos + sw;   // exclusive

        // Right extension.
        while (rightEnd < wc) {
            std::vector<std::string> ext(w.begin() + leftEnd,
                                          w.begin() + rightEnd + 1);
            if (countDocsWithSequence(ext) >= minDocs) {
                ++rightEnd;
            } else {
                break;
            }
        }

        // Left extension.
        while (leftEnd > 0) {
            std::vector<std::string> ext(w.begin() + leftEnd - 1,
                                          w.begin() + rightEnd);
            if (countDocsWithSequence(ext) >= minDocs) {
                --leftEnd;
            } else {
                break;
            }
        }

        std::string maximal;
        for (int i = leftEnd; i < rightEnd; ++i) {
            if (i > leftEnd) maximal += ' ';
            maximal += w[i];
        }
        maximalPhrases.insert(maximal);
    }

    // ----------------------------------------------------------------
    // Phase 3: subsumption dedup (word-boundary aware).
    // ----------------------------------------------------------------
    // Char-level find() would treat "the cat" as subsumed by "the cats
    // sat", which is wrong; we explicitly require space-or-end on both
    // sides of every candidate match.
    auto isWordSubstring =
        [](const std::string& haystack, const std::string& needle) -> bool {
        if (needle.empty()) return true;
        if (needle.size() > haystack.size()) return false;
        size_t p = 0;
        while ((p = haystack.find(needle, p)) != std::string::npos) {
            bool leftOk  = (p == 0)
                || (haystack[p - 1] == ' ');
            size_t ep    = p + needle.size();
            bool rightOk = (ep == haystack.size())
                || (haystack[ep] == ' ');
            if (leftOk && rightOk) return true;
            ++p;
        }
        return false;
    };

    std::vector<std::string> phraseList(maximalPhrases.begin(),
                                         maximalPhrases.end());
    // Sort longest-first so the first kept phrase is the most general,
    // and every later candidate is checked only against shorter-or-equal
    // already-kept entries (the natural domination order).
    std::sort(phraseList.begin(), phraseList.end(),
              [](const std::string& a, const std::string& b) {
                  if (a.size() != b.size()) return a.size() > b.size();
                  return a < b;  // tie-break lexicographically for stable output
              });

    std::vector<std::string> filtered;
    for (const auto& cand : phraseList) {
        bool subsumed = false;
        for (const auto& existing : filtered) {
            if (isWordSubstring(existing, cand)) {
                subsumed = true;
                break;
            }
        }
        if (!subsumed) {
            filtered.push_back(cand);
        }
    }

    for (const auto& phrase : filtered) {
        boilerplate_.append(phrase);
    }

    emitTrace("detectBoilerplate done phrases=" + std::to_string(boilerplate_.count())
            + " threshold=" + std::to_string(boilerplateThreshold_)
            + " minDocs=" + std::to_string(minDocs)
            + " totalDocs=" + std::to_string(numDocs));

    return boilerplate_.count();
}

// Function: getSynonymDictionary
// Inverts the synonymMap_ (synonym->root) into a grouped view (root->[synonyms])
// for display purposes.
std::vector<SynonymDictEntry> PlagiarismEngine::getSynonymDictionary() const {
    // Build root -> synonyms grouping.
    std::map<std::string, std::vector<std::string>> grouped;
    for (auto& kv : synonymMap_) {
        grouped[kv.second].push_back(kv.first);
    }
    std::vector<SynonymDictEntry> result;
    for (auto& kv : grouped) {
        SynonymDictEntry entry;
        entry.rootToken = kv.first;
        // Sort synonyms alphabetically for stable display.
        std::sort(kv.second.begin(), kv.second.end());
        entry.synonyms = kv.second;
        result.push_back(entry);
    }
    return result;
}

// Function: getSynonymMappingReport
// Tokenizes the input text and reports every word that has a synonym mapping,
// showing original_word -> root_token.
std::vector<SynonymMapping> PlagiarismEngine::getSynonymMappingReport(
    const std::string& text) const
{
    std::vector<SynonymMapping> report;
    if (synonymMap_.empty()) return report;

    std::string normalized = normalizeText(text);
    // Split into words manually (same logic as tokenize but we keep originals).
    std::string word;
    for (size_t i = 0; i <= normalized.size(); ++i) {
        if (i == normalized.size() || normalized[i] == ' ') {
            if (!word.empty()) {
                // Check if this word has a synonym mapping.
                std::string key;
                key.reserve(word.size());
                for (char c : word) {
                    key += static_cast<char>(
                        std::tolower(static_cast<unsigned char>(c)));
                }
                auto it = synonymMap_.find(key);
                if (it != synonymMap_.end()) {
                    report.push_back(SynonymMapping(word, it->second));
                }
                word.clear();
            }
        } else {
            word += normalized[i];
        }
    }
    return report;
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
//
// FIXES (versus the previous revision):
//   * The decision to install a tree-level trace callback is captured once at
//     entry into `wasTracingTree` and used at exit too, so the callback is
//     always cleared even if traceDataStructure_ flips mid-scan. Without
//     this, the callback would persist on tree_ holding dangling stack
//     references (treeTraceBudget / treeTraceTruncated) for any subsequent
//     query that runs while the flag is off.
//   * `wordStarts` and `matched` are now std::vector<int> / std::vector<bool>
//     instead of raw new[] arrays. If any append() inside the per-ngram loop
//     throws, the vectors clean themselves up; the old `new[]` blocks leaked.
//   * `wordStarts` is computed from PRE-synonym word boundaries via
//     findWordBoundaries(). The old code summed post-synonym word lengths
//     which produced wrong cleanStart/cleanEnd values whenever a synonym
//     substitution changed a word's length, and that error then propagated
//     through mapPositions() into highlight offsets.
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

    // Snapshot the tracing flag at function entry. Tree callback installation
    // and teardown both key off this local so a concurrent setter cannot
    // leave a dangling lambda holding references to the local budget vars.
    bool wasTracingTree = traceDataStructure_;
    int treeTraceBudget = 180;
    bool treeTraceTruncated = false;
    if (wasTracingTree) {
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

    // PRE-synonym word boundaries in `cleaned`. These align to the same
    // word count as `words` because tokenize() and findWordBoundaries()
    // share the same split rule.
    std::vector<int> wordStarts, wordEnds;
    findWordBoundaries(cleaned, wordStarts, wordEnds);
    if (static_cast<int>(wordStarts.size()) < words.count()) {
        // Defensive bail-out; clear the tree callback first so we don't leave
        // a dangling capture behind.
        if (wasTracingTree) tree_.clearTraceCallback();
        return report;
    }

    int textLen = static_cast<int>(queryText.size());
    std::vector<bool> matched(textLen, false);

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
            int cleanEnd = wordEnds[lastWordIdx];
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

    // Un-mark characters in the original query text that belong to
    // whitelist or boilerplate terms. Without this, the ratio-based
    // position mapping inflates matched ranges when filter words are
    // removed (cleaned text is shorter -> ratio is larger -> mapped
    // ranges cover more characters -> percentage goes UP instead of DOWN).
    {
        // Build a lowercase copy of the original for case-insensitive matching.
        std::string lowerQuery;
        lowerQuery.reserve(queryText.size());
        for (size_t ci = 0; ci < queryText.size(); ++ci) {
            lowerQuery += static_cast<char>(
                std::tolower(static_cast<unsigned char>(queryText[ci])));
        }

        // Lambda: find each term in the original text and un-mark those chars.
        auto unmarkTerms = [&](const RawBuffer<std::string>& terms) {
            for (int t = 0; t < terms.count(); ++t) {
                std::string term = normalizeText(terms[t]);
                if (term.empty()) continue;
                size_t p = 0;
                while ((p = lowerQuery.find(term, p)) != std::string::npos) {
                    // Word boundary checks.
                    bool leftOk = (p == 0) ||
                        !std::isalnum(static_cast<unsigned char>(lowerQuery[p - 1]));
                    size_t ep = p + term.size();
                    bool rightOk = (ep >= lowerQuery.size()) ||
                        !std::isalnum(static_cast<unsigned char>(lowerQuery[ep]));
                    if (leftOk && rightOk) {
                        for (size_t c = p; c < ep && c < static_cast<size_t>(textLen); ++c)
                            matched[c] = false;
                    }
                    ++p;
                }
            }
        };

        unmarkTerms(whitelist_);
        unmarkTerms(boilerplate_);
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

    // Use the SAME local snapshot we used at entry so the teardown branch
    // is paired with the setup branch regardless of any mid-scan flag flip.
    if (wasTracingTree) {
        tree_.clearTraceCallback();
    }
    return report;
}

// Function: rankSources
//
// Same offset/leak fixes as scan(): PRE-synonym word boundaries from
// findWordBoundaries(), std::vector for the boundary table, no raw new[].
RawBuffer<SourceScore> PlagiarismEngine::rankSources(
    const std::string& queryText,
    const std::string& queryFilename,
    double radius,
    int topK) const
{
    RawBuffer<SourceScore> scores;
    emitTrace("rankSources start file=\"" + queryFilename + "\" textLen="
            + std::to_string(queryText.size())
            + " radius=" + std::to_string(radius)
            + " topK=" + std::to_string(topK));
    if (tree_.empty() || queryText.empty()) {
        emitTrace("rankSources aborted reason=empty-tree-or-query");
        return scores;
    }
    std::string cleaned = preprocessText(queryText);
    RawBuffer<std::string> words = tokenize(cleaned);
    if (words.count() < ngramSize_) {
        emitTrace("rankSources aborted reason=query-shorter-than-ngram-window");
        return scores;
    }
    int totalQueryNgrams = words.count() - ngramSize_ + 1;

    std::vector<int> wordStarts, wordEnds;
    findWordBoundaries(cleaned, wordStarts, wordEnds);
    if (static_cast<int>(wordStarts.size()) < words.count()) {
        return scores;
    }

    RawBuffer<double> simSums;
    for (int i = 0; i <= words.count() - ngramSize_; ++i) {
        std::string ngramText;
        for (int j = 0; j < ngramSize_; ++j) {
            if (j > 0) ngramText += ' ';
            ngramText += words[i + j];
        }
        NGram queryNgram(ngramText, queryFilename, wordStarts[i], 0, i);
        double normalizedRadius = radius;
        if (normalizedRadius < 0.0) normalizedRadius = 0.0;
        double effectiveRadius = normalizedRadius
                     * static_cast<double>(queryNgram.text.size());
        RawBuffer<SearchResult> results = tree_.range_query(
            queryNgram, effectiveRadius);
        if (results.count() == 0) continue;
        int bestIdx = 0;
        for (int r = 1; r < results.count(); ++r) {
            if (results[r].distance < results[bestIdx].distance)
                bestIdx = r;
        }
        const std::string& src = results[bestIdx].ngram.sourceFile;
        int queryLen = static_cast<int>(queryNgram.text.size());
        int matchLen = static_cast<int>(results[bestIdx].ngram.text.size());
        double maxLen = static_cast<double>(queryLen > matchLen
                                            ? queryLen : matchLen);
        double normDist = (maxLen > 0.0)
            ? (results[bestIdx].distance / maxLen)
            : 0.0;
        if (normDist > 1.0) normDist = 1.0;
        double similarity = 1.0 - normDist;
        int sourceIdx = -1;
        for (int s = 0; s < scores.count(); ++s) {
            if (scores[s].sourceFile == src) {
                sourceIdx = s;
                break;
            }
        }
        if (sourceIdx < 0) {
            scores.append(SourceScore(src));
            simSums.append(0.0);
            sourceIdx = scores.count() - 1;
        }
        scores[sourceIdx].matchedNgrams += 1;
        simSums[sourceIdx] += similarity;
    }
    for (int i = 0; i < scores.count(); ++i) {
        scores[i].coveragePercent = (totalQueryNgrams > 0)
            ? (100.0 * static_cast<double>(scores[i].matchedNgrams)
               / static_cast<double>(totalQueryNgrams))
            : 0.0;
        scores[i].avgSimilarity = (scores[i].matchedNgrams > 0)
            ? (simSums[i] / static_cast<double>(scores[i].matchedNgrams))
            : 0.0;
    }
    for (int i = 0; i < scores.count() - 1; ++i) {
        int best = i;
        for (int j = i + 1; j < scores.count(); ++j) {
            if (scores[j].matchedNgrams > scores[best].matchedNgrams ||
                (scores[j].matchedNgrams == scores[best].matchedNgrams &&
                 scores[j].avgSimilarity > scores[best].avgSimilarity)) {
                best = j;
            }
        }
        if (best != i) {
            scores.swapElements(i, best);
        }
    }
    if (topK > 0 && scores.count() > topK) {
        while (scores.count() > topK)
            scores.removeAt(scores.count() - 1);
    }
    emitTrace("rankSources done sources=" + std::to_string(scores.count())
            + " totalQueryNgrams=" + std::to_string(totalQueryNgrams));
    return scores;
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
