#include <iostream>
#include <cassert>
#include <cmath>
#include "rawbuffer.h"
#include "ngram.h"
#include "vptree.h"
#include "engine.h"

void test_rawbuffer() {
    std::cout << "[TEST] RawBuffer... ";
    RawBuffer<int> buf;
    assert(buf.count() == 0);
    assert(buf.isEmpty());

    for (int i = 0; i < 100; ++i) buf.append(i);
    assert(buf.count() == 100);
    assert(buf[50] == 50);

    buf.removeAt(0);
    assert(buf.count() == 99);
    assert(buf[0] == 1);

    RawBuffer<int> copy = buf;
    assert(copy.count() == 99);
    assert(copy[0] == 1);

    buf.clear();
    assert(buf.isEmpty());
    assert(copy.count() == 99); // copy unaffected

    std::cout << "PASS\n";
}

void test_distance_metric() {
    std::cout << "[TEST] Distance metric... ";

    NGram a("hello world", "", 0, 0, 0);
    NGram b("hello world", "", 0, 0, 0);
    assert(VPTree::distance_metric(a, b) == 0.0);

    NGram c("hello", "", 0, 0, 0);
    NGram d("hallo", "", 0, 0, 0);
    double dist = VPTree::distance_metric(c, d);
    assert(dist > 0.0 && dist < 1.0);
    // Levenshtein("hello","hallo") = 1, normalized = 1/5 = 0.2
    assert(std::abs(dist - 0.2) < 0.001);

    NGram e("abc", "", 0, 0, 0);
    NGram f("xyz", "", 0, 0, 0);
    double dist2 = VPTree::distance_metric(e, f);
    assert(dist2 == 1.0); // all 3 chars different

    std::cout << "PASS\n";
}

void test_vptree_build_and_query() {
    std::cout << "[TEST] VP-Tree build + range_query... ";

    VPTree tree;
    assert(tree.empty());

    // Create some n-grams
    const int N = 10;
    NGram items[N] = {
        NGram("the quick brown fox", "doc1.txt", 0, 20, 0),
        NGram("the quick brown dog", "doc1.txt", 5, 25, 1),
        NGram("a lazy brown fox", "doc2.txt", 0, 18, 0),
        NGram("the slow brown fox", "doc2.txt", 5, 22, 1),
        NGram("completely different text", "doc3.txt", 0, 25, 0),
        NGram("nothing similar here", "doc3.txt", 5, 24, 1),
        NGram("random words placed here", "doc3.txt", 10, 30, 2),
        NGram("the quick red fox", "doc4.txt", 0, 18, 0),
        NGram("some other content now", "doc4.txt", 5, 25, 1),
        NGram("the quick brown fox", "doc4.txt", 10, 30, 2),
    };

    tree.build_tree(items, N);
    assert(!tree.empty());
    assert(tree.size() == N);
    assert(VPTree::get_height(tree.root()) > 0);

    // Range query: search for "the quick brown fox"
    NGram query("the quick brown fox", "query.txt", 0, 20, 0);
    auto results = tree.range_query(query, 0.25);

    // Should find several close matches
    assert(results.count() > 0);

    // Exact match from doc4 should be found (distance = 0)
    bool foundExact = false;
    for (int i = 0; i < results.count(); ++i) {
        if (results[i].distance < 0.001 &&
            results[i].ngram.sourceFile == "doc4.txt") {
            foundExact = true;
        }
    }
    assert(foundExact);

    std::cout << "PASS (found " << results.count() << " matches)\n";
}

void test_vptree_knn() {
    std::cout << "[TEST] VP-Tree KNN search... ";

    VPTree tree;
    NGram items[5] = {
        NGram("alpha beta gamma", "a.txt", 0, 10, 0),
        NGram("alpha beta delta", "b.txt", 0, 10, 0),
        NGram("omega psi chi", "c.txt", 0, 10, 0),
        NGram("alpha gamma delta", "d.txt", 0, 10, 0),
        NGram("one two three", "e.txt", 0, 10, 0),
    };
    tree.build_tree(items, 5);

    NGram query("alpha beta gamma", "q.txt", 0, 10, 0);
    auto results = tree.search_knn(query, 2);

    assert(results.count() == 2);
    // Closest should be "alpha beta gamma" from a.txt (dist=0)
    // or "alpha beta delta" from b.txt (small dist)
    std::cout << "PASS (k=2, got " << results.count() << ")\n";
}

void test_vptree_insert() {
    std::cout << "[TEST] VP-Tree insert... ";

    VPTree tree;
    NGram first("hello world today", "x.txt", 0, 10, 0);
    tree.insert(first);
    assert(tree.size() == 1);
    assert(VPTree::is_leaf(tree.root()));

    tree.insert(NGram("hello world tomorrow", "y.txt", 0, 10, 0));
    tree.insert(NGram("goodbye world today", "z.txt", 0, 10, 0));
    assert(tree.size() == 3);
    assert(VPTree::get_height(tree.root()) >= 1);

    std::cout << "PASS (size=" << tree.size()
              << ", height=" << VPTree::get_height(tree.root()) << ")\n";
}

void test_engine_full_scan() {
    std::cout << "[TEST] Full engine scan... ";

    PlagiarismEngine engine;
    engine.setNgramSize(4);

    // Add database files
    bool ok1 = engine.addFile("../sample_data/database_doc1.txt");
    bool ok2 = engine.addFile("../sample_data/database_doc2.txt");

    if (!ok1 || !ok2) {
        std::cout << "SKIP (sample files not found at expected path)\n";
        return;
    }

    assert(engine.treeSize() > 0);
    std::cout << "\n  Tree: " << engine.treeSize() << " nodes, height "
              << engine.treeHeight() << "\n";

    // Scan the suspicious document
    std::string queryText = PlagiarismEngine::readFile(
        "../sample_data/query_suspicious.txt");
    assert(!queryText.empty());

    ScanReport report = engine.scan(queryText, "query_suspicious.txt", 0.30);

    std::cout << "  Match: " << report.matchPercentage << "%\n";
    std::cout << "  Segments: " << report.segments.count() << "\n";
    std::cout << "  Sources: ";
    for (int i = 0; i < report.matchedFiles.count(); ++i)
        std::cout << report.matchedFiles[i] << " ";
    std::cout << "\n";

    // Should detect plagiarism
    assert(report.matchPercentage > 5.0);
    assert(report.matchedFiles.count() > 0);

    std::cout << "  PASS\n";
}

void test_whitelist() {
    std::cout << "[TEST] Whitelist filtering... ";

    PlagiarismEngine engine;
    engine.addWhitelistWord("university");
    engine.addWhitelistWord("department");
    assert(engine.whitelist().count() == 2);

    engine.removeWhitelistWord(0);
    assert(engine.whitelist().count() == 1);

    engine.clearWhitelist();
    assert(engine.whitelist().count() == 0);

    std::cout << "PASS\n";
}

int main() {
    std::cout << "=== VP-Tree Plagiarism Detector — Unit Tests ===\n\n";

    test_rawbuffer();
    test_distance_metric();
    test_vptree_build_and_query();
    test_vptree_knn();
    test_vptree_insert();
    test_whitelist();
    test_engine_full_scan();

    std::cout << "\n=== ALL TESTS PASSED ===\n";
    return 0;
}
