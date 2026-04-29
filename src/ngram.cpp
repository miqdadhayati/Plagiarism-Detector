#include "ngram.hpp"

NGram::NGram()
    : startPos(0), endPos(0), ngramIndex(0) {}

NGram::NGram(const std::string& text,
             const std::string& sourceFile,
             int startPos, int endPos,
             int ngramIndex)
    : text(text)
    , sourceFile(sourceFile)
    , startPos(startPos)
    , endPos(endPos)
    , ngramIndex(ngramIndex) {}

bool NGram::operator==(const NGram& o) const {
    return text == o.text
        && sourceFile == o.sourceFile
        && startPos == o.startPos;
}

bool NGram::operator!=(const NGram& o) const {
    return !(*this == o);
}
