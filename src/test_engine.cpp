#include <iostream>
#include <cassert>
#include <cmath>
#include <string>
#include <fstream>
#include <cstdio>
#include "rawbuffer.h"
#include "ngram.h"
#include "vptree.h"
#include "engine.h"

// Function: print_visual_op
void print_visual_op(const std::string& name, bool ok) {
    std::cout << "  " << name << " "
              << (ok ? "[##########] PASS" : "[----------] FAIL")
              << "\n";
}

// Function: test_rawbuffer
void test_rawbuffer() {
    std::cout << "[TEST] RawBuffer... ";
    RawBuffer<int> buf;
    assert(buf.count() == 0);
    assert(buf.isEmpty());
    for (int i = 0; i < 100; ++i) buf.append(i);
    assert(buf.count() == 100);
    assert(buf[50] == 50);
    assert(buf.contains(50));
    buf.removeAt(0);
    assert(buf.count() == 99);
    assert(buf[0] == 1);
    buf.swapElements(0, 10);
    assert(buf[0] == 11);
    assert(buf[10] == 1);
    RawBuffer<int> copy = buf;
    assert(copy.count() == 99);
    assert(copy[0] == 11);
    buf.clear();
    assert(buf.isEmpty());
    assert(copy.count() == 99); // copy unaffected
    std::cout << "PASS\n";
}

// Function: test_distance_metric
void test_distance_metric() {
    std::cout << "[TEST] Distance metric... ";
    NGram a("hello world", "", 0, 0, 0);
    NGram b("hello world", "", 0, 0, 0);
    assert(VPTree::distance_metric(a, b) == 0.0);
    NGram c("hello", "", 0, 0, 0);
    NGram d("hallo", "", 0, 0, 0);
    double dist = VPTree::distance_metric(c, d);
    assert(dist > 0.0);
    // Levenshtein("hello","hallo") = 1
    assert(std::abs(dist - 1.0) < 0.001);
    NGram e("abc", "", 0, 0, 0);
    NGram f("xyz", "", 0, 0, 0);
    double dist2 = VPTree::distance_metric(e, f);
    assert(dist2 == 3.0); // all 3 chars different
    NGram g("ab", "", 0, 0, 0);
    NGram h("aba", "", 0, 0, 0);
    NGram i("ba", "", 0, 0, 0);
    double dgi = VPTree::distance_metric(g, i);
    double dgh = VPTree::distance_metric(g, h);
    double dhi = VPTree::distance_metric(h, i);
    assert(dgi <= dgh + dhi + 1e-9);
    std::cout << "PASS\n";
}

// Function: test_vptree_build_and_query
void test_vptree_build_and_query() {
    std::cout << "[TEST] VP-Tree build + range_query... ";
    VPTree tree;
    assert(tree.empty());
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

// Function: test_vptree_knn
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
    bool foundClose = false;
    for (int i = 0; i < results.count(); ++i) {
        if (results[i].ngram.sourceFile == "a.txt" ||
            results[i].ngram.sourceFile == "b.txt") {
            foundClose = true;
        }
    }
    assert(foundClose);
    std::cout << "PASS (k=2, got " << results.count() << ")\n";
}

// Function: test_vptree_insert
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

// Function: test_vptree_crud
void test_vptree_crud() {
    std::cout << "[TEST] VP-Tree CRUD operations... ";
    VPTree tree;
    NGram a("alpha beta gamma", "crud_a.txt", 0, 16, 0);
    NGram b("delta epsilon zeta", "crud_b.txt", 0, 18, 0);
    NGram c("theta iota kappa", "crud_c.txt", 0, 16, 0);
    NGram items[3] = {a, b, c};
    // CREATE
    tree.build_tree(items, 3);
    assert(tree.size() == 3);
    // READ
    assert(tree.contains(a));
    assert(tree.contains(b));
    // UPDATE
    NGram updatedB("delta epsilon lambda", "crud_b.txt", 0, 20, 0);
    bool updated = tree.update(b, updatedB);
    assert(updated);
    assert(!tree.contains(b));
    assert(tree.contains(updatedB));
    // DELETE
    bool removed = tree.remove(a);
    assert(removed);
    assert(!tree.contains(a));
    assert(tree.size() == 2);
    bool removedMissing = tree.remove(a);
    assert(!removedMissing);
    std::cout << "PASS\n";
}

// Function: test_vptree_utilities
void test_vptree_utilities() {
    std::cout << "[TEST] VP-Tree utility methods... ";
    VPTree tree;
    NGram items[4] = {
        NGram("one two three", "u1.txt", 0, 12, 0),
        NGram("one two four", "u2.txt", 0, 11, 0),
        NGram("five six seven", "u3.txt", 0, 14, 0),
        NGram("eight nine ten", "u4.txt", 0, 14, 0),
    };
    tree.build_tree(items, 4);
    assert(!tree.empty());
    assert(VPTree::get_height(tree.root()) >= 1);
    assert(!VPTree::is_leaf(tree.root()));
    int before = tree.size();
    tree.rebuild();
    assert(tree.size() == before);
    assert(!tree.empty());
    tree.clear();
    assert(tree.empty());
    assert(tree.size() == 0);
    std::cout << "PASS\n";
}

// Function: test_vptree_operations_visualization
void test_vptree_operations_visualization() {
    std::cout << "[TEST] VP-Tree operations visualization...\n";
    VPTree tree;
    NGram a("graph search tree", "viz_a.txt", 0, 17, 0);
    NGram b("graph shortest path", "viz_b.txt", 0, 19, 0);
    NGram c("neural network model", "viz_c.txt", 0, 20, 0);
    NGram d("machine learning model", "viz_d.txt", 0, 22, 0);
    NGram e("dynamic programming", "viz_e.txt", 0, 19, 0);
    NGram items[5] = {a, b, c, d, e};
    tree.build_tree(items, 5);
    bool createOk = !tree.empty() && tree.size() == 5;
    bool readOk = tree.contains(c);
    NGram query("graph search tree", "viz_query.txt", 0, 17, 0);
    auto rangeResults = tree.range_query(query, 0.25);
    bool rangeOk = rangeResults.count() > 0;
    auto knnResults = tree.search_knn(query, 2);
    bool knnOk = knnResults.count() == 2;
    NGram updatedD("machine learning systems", "viz_d.txt", 0, 24, 0);
    bool updateOk = tree.update(d, updatedD) && tree.contains(updatedD) && !tree.contains(d);
    bool deleteOk = tree.remove(e) && !tree.contains(e) && tree.size() == 4;
    int beforeRebuild = tree.size();
    tree.rebuild();
    bool rebuildOk = !tree.empty() && tree.size() == beforeRebuild;
    bool heightOk = VPTree::get_height(tree.root()) >= 1;
    print_visual_op("CREATE", createOk);
    print_visual_op("READ", readOk);
    print_visual_op("RANGE QUERY", rangeOk);
    print_visual_op("KNN QUERY", knnOk);
    print_visual_op("UPDATE", updateOk);
    print_visual_op("DELETE", deleteOk);
    print_visual_op("REBUILD", rebuildOk);
    print_visual_op("HEIGHT CHECK", heightOk);
    bool allOk = createOk && readOk && rangeOk && knnOk
              && updateOk && deleteOk && rebuildOk && heightOk;
    assert(allOk);
    std::cout << "  VISUAL SUMMARY [##########] PASS\n";
}

// Function: test_engine_full_scan
void test_engine_full_scan() {
    std::cout << "[TEST] Full engine scan... ";
    PlagiarismEngine engine;
    engine.setNgramSize(4);
    bool ok1 = engine.addFile("../sample_data/database_doc1.txt");
    bool ok2 = engine.addFile("../sample_data/database_doc2.txt");
    if (!ok1 || !ok2) {
        std::cout << "SKIP (sample files not found at expected path)\n";
        return;
    }
    assert(engine.treeSize() > 0);
    std::cout << "\n  Tree: " << engine.treeSize() << " nodes, height "
              << engine.treeHeight() << "\n";
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
    assert(report.matchPercentage > 5.0);
    assert(report.matchedFiles.count() > 0);
    std::cout << "  PASS\n";
}

// Function: test_filter_list_management
void test_filter_list_management() {
    std::cout << "[TEST] Filter list management... ";
    PlagiarismEngine engine;
    engine.addWhitelistWord("university");
    engine.addWhitelistWord("department");
    assert(engine.whitelist().count() == 2);
    engine.removeWhitelistWord(0);
    assert(engine.whitelist().count() == 1);
    engine.clearWhitelist();
    assert(engine.whitelist().count() == 0);

    engine.addBoilerplatePhrase("introduction");
    engine.addBoilerplatePhrase("author declaration");
    assert(engine.boilerplate().count() == 2);
    engine.removeBoilerplatePhrase(0);
    assert(engine.boilerplate().count() == 1);
    engine.clearBoilerplate();
    assert(engine.boilerplate().count() == 0);

    std::cout << "PASS\n";
}

// Function: test_strict_filter_preprocessing
void test_strict_filter_preprocessing() {
    std::cout << "[TEST] Strict filter preprocessing... ";
    PlagiarismEngine engine;
    engine.addWhitelistWord("data");
    engine.addBoilerplatePhrase("official department header");

    std::string cleaned = engine.preprocessText(
        "Official Department Header. Database data driven methods.");
    assert(cleaned == "database driven methods");

    // "data" must not remove the substring inside "database".
    std::string strictBoundary = engine.preprocessText(
        "Database systems and data pipelines");
    assert(strictBoundary == "database systems and pipelines");

    std::cout << "PASS\n";
}

// Function: test_boilerplate_reindex_effect
void test_boilerplate_reindex_effect() {
    std::cout << "[TEST] Boilerplate reindex effect... ";
    const char* dbPath = "tmp_filter_db.txt";

    {
        std::ofstream ofs(dbPath);
        assert(ofs.good());
        ofs << "official boilerplate header alpha beta gamma delta epsilon";
    }

    std::string query =
        "Official boilerplate header alpha beta gamma delta epsilon";

    PlagiarismEngine engine;
    engine.setNgramSize(3);
    assert(engine.addFile(dbPath));
    assert(engine.indexedFiles().count() == 1);
    int beforeNgrams = engine.indexedFiles()[0].ngramCount;

    engine.addBoilerplatePhrase("official boilerplate header");
    assert(engine.rebuildIndexWithFilters());
    assert(engine.indexedFiles().count() == 1);
    int afterNgrams = engine.indexedFiles()[0].ngramCount;
    assert(afterNgrams < beforeNgrams);

    std::string cleaned = engine.preprocessText(query);
    assert(cleaned == "alpha beta gamma delta epsilon");

    ScanReport report = engine.scan(query, "tmp_query.txt", 0.30);
    assert(report.matchPercentage > 10.0);

    int rm = std::remove(dbPath);
    assert(rm == 0);

    std::cout << "PASS\n";
}

// Function: main
int main() {
    std::cout << "=== VP-Tree Plagiarism Detector — Unit Tests ===\n\n";
    test_rawbuffer();
    test_distance_metric();
    test_vptree_build_and_query();
    test_vptree_knn();
    test_vptree_insert();
    test_vptree_crud();
    test_vptree_utilities();
    test_vptree_operations_visualization();
    test_filter_list_management();
    test_strict_filter_preprocessing();
    test_boilerplate_reindex_effect();
    test_engine_full_scan();
    std::cout << "\n=== ALL TESTS PASSED ===\n";
    return 0;
}
