#include "ngram.h"

// Function: NGram
NGram::NGram()
    : startPos(0), endPos(0), ngramIndex(0) {}

// Function: NGram
NGram::NGram(const std::string& text,
             const std::string& sourceFile,
             int startPos, int endPos,
             int ngramIndex)
    : text(text)
    , sourceFile(sourceFile)
    , startPos(startPos)
    , endPos(endPos)
    , ngramIndex(ngramIndex) {}

// Function: operator==
bool NGram::operator==(const NGram& o) const {
    return text == o.text
        && sourceFile == o.sourceFile
        && startPos == o.startPos;
}

// Function: operator!=
bool NGram::operator!=(const NGram& o) const {
    return !(*this == o);
}
