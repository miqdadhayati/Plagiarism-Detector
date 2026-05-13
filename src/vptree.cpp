re#include "vptree.h"
#include <cmath>
#include <cstdlib>
#include <algorithm>
#include <climits>
#include <string>

// Function: emitTrace
void VPTree::emitTrace(const std::string& message) const {
    if (traceCallback_) {
        traceCallback_(message);
    }
}

// Function: distance_metric
// Calculates Levenshtein distance using 2-row DP.
// Time Complexity: O(m * n), Space Complexity: O(n).
double VPTree::distance_metric(const NGram& a, const NGram& b) {
    const std::string& s = a.text;
    const std::string& t = b.text;
    int m = static_cast<int>(s.size());
    int n = static_cast<int>(t.size());
    if (m == 0 && n == 0) return 0.0;
    if (m == 0) return static_cast<double>(n);
    if (n == 0) return static_cast<double>(m);
    int* prev = new int[n + 1];
    int* curr = new int[n + 1];
    for (int j = 0; j <= n; ++j) prev[j] = j;
    for (int i = 1; i <= m; ++i) {
        curr[0] = i;
        for (int j = 1; j <= n; ++j) {
            int cost = (s[i - 1] == t[j - 1]) ? 0 : 1;
            int ins  = curr[j - 1] + 1;
            int del  = prev[j] + 1;
            int sub  = prev[j - 1] + cost;
            curr[j] = ins;
            if (del < curr[j]) curr[j] = del;
            if (sub < curr[j]) curr[j] = sub;
        }
        int* tmp = prev;
        prev = curr;
        curr = tmp;
    }
    double editDist = static_cast<double>(prev[n]);
    delete[] prev;
    delete[] curr;
    return editDist;
}

// Function: quickSelect
// Returns k-th smallest value using Hoare partition.
// Time Complexity: O(n) average, O(n^2) worst-case.
double VPTree::quickSelect(double* arr, int n, int k) {
    if (n <= 0) return 0.0;
    if (n == 1) return arr[0];
    if (k < 0)   k = 0;
    if (k >= n)  k = n - 1;
    int left = 0, right = n - 1;
    while (left < right) {
        double pivot = arr[left + (right - left) / 2];
        int i = left, j = right;
        while (i <= j) {
            while (arr[i] < pivot) ++i;
            while (arr[j] > pivot) --j;
            if (i <= j) {
                double tmp = arr[i];
                arr[i] = arr[j];
                arr[j] = tmp;
                ++i; --j;
            }
        }
        if (k <= j)      right = j;
        else if (k >= i) left  = i;
        else             break;
    }
    return arr[k];
}

// Function: select_vantage_point
// Selects a vantage point by finding the one with maximum variance among samples.
// Time Complexity: O(1) (bounded sample size).
NGram VPTree::select_vantage_point(NGram* items, int count) {
    if (count <= 0) return NGram();
    if (count == 1) return items[0];
    int numCandidates = (count < 5) ? count : 5;
    int sampleSize    = (count < 20) ? count : 20;
    int bestIdx = 0;
    double bestVariance = -1.0;
    for (int c = 0; c < numCandidates; ++c) {
        int candidateIdx = std::rand() % count;
        const NGram& candidate = items[candidateIdx];
        double sum  = 0.0;
        double sum2 = 0.0;
        int    used = 0;
        for (int s = 0; s < sampleSize; ++s) {
            int si = std::rand() % count;
            if (si == candidateIdx) continue;
            double d = distance_metric(candidate, items[si]);
            sum  += d;
            sum2 += d * d;
            ++used;
        }
        if (used > 0) {
            double mean = sum / used;
            double var  = (sum2 / used) - (mean * mean);
            if (var > bestVariance) {
                bestVariance = var;
                bestIdx      = candidateIdx;
            }
        }
    }
    return items[bestIdx];
}

// Function: calculate_median_distance
// Calculates median distance from the vantage point to all items.
// Time Complexity: O(n) average.
double VPTree::calculate_median_distance(const NGram& vp,
                                          NGram* items, int count) {
    if (count <= 0) return 0.0;
    double* dists = new double[count];
    for (int i = 0; i < count; ++i)
        dists[i] = distance_metric(vp, items[i]);
    int medianIdx = count / 2;
    double median = quickSelect(dists, count, medianIdx);
    delete[] dists;
    return median;
}

// Function: buildRecursive
// Builds VP subtree recursively with tie-balancing for identical distances.
// Time Complexity: O(n log n) average, O(n^2) worst-case.
VPTreeNode* VPTree::buildRecursive(NGram* items, int count) {
    emitTrace("buildRecursive(count=" + std::to_string(count) + ")");
    if (count == 0) return nullptr;
    if (count == 1) {
        VPTreeNode* leaf = new VPTreeNode(items[0]);
        ++nodeCount_;
        emitTrace("  leaf node: \"" + items[0].text + "\"");
        return leaf;
    }
    // 1. Select vantage point
    NGram vp = select_vantage_point(items, count);
    emitTrace("  selected VP: \"" + vp.text + "\"");
    for (int i = 0; i < count; ++i) {
        if (items[i].text == vp.text &&
            items[i].sourceFile == vp.sourceFile &&
            items[i].startPos == vp.startPos) {
            NGram tmp  = items[0];
            items[0]   = items[i];
            items[i]   = tmp;
            break;
        }
    }
    VPTreeNode* node = new VPTreeNode(items[0]);
    ++nodeCount_;
    NGram* rest     = items + 1;
    int    restSize = count - 1;
    if (restSize == 0) return node;
    // 2. Calculate median distance from vp to the rest
    double median = calculate_median_distance(node->vantagePoint, rest, restSize);
    node->medianDistance = median;
    emitTrace("  median distance: " + std::to_string(median));
    // 3. Partition rest. Items with d < median go strictly inside; items
    //    with d > median go strictly outside; items with d == median are
    //    alternated between the two halves so that long runs of duplicates
    //    don't collapse the tree into a single chain.
    NGram* inside  = new NGram[restSize];
    NGram* outside = new NGram[restSize];
    int    inCount = 0, outCount = 0;
    int    tieToggle = 0;
    for (int i = 0; i < restSize; ++i) {
        double d = distance_metric(node->vantagePoint, rest[i]);
        if (d < median) {
            inside[inCount++] = rest[i];
        } else if (d > median) {
            outside[outCount++] = rest[i];
        } else {
            // d == median: alternate to prevent linked-list degeneration.
            if ((tieToggle++ & 1) == 0)
                inside[inCount++] = rest[i];
            else
                outside[outCount++] = rest[i];
        }
    }
    emitTrace("  partition inside=" + std::to_string(inCount)
            + " outside=" + std::to_string(outCount));
    // 4. Recurse
    node->left  = buildRecursive(inside,  inCount);
    node->right = buildRecursive(outside, outCount);
    delete[] inside;
    delete[] outside;
    return node;
}

// Function: build_tree
// Clears and builds a new VP tree from items.
// Time Complexity: O(n log n) average.
void VPTree::build_tree(NGram* items, int count) {
    emitTrace("build_tree(start count=" + std::to_string(count) + ")");
    clear();
    if (count > 0)
        root_ = buildRecursive(items, count);
    emitTrace("build_tree(done nodes=" + std::to_string(nodeCount_) + ")");
}

// Function: rangeQueryRecursive
// Recursively searches for items within radius using triangle inequality pruning.
// Time Complexity: O(log n) average, O(n) worst-case.
void VPTree::rangeQueryRecursive(VPTreeNode* node,
                                  const NGram& query,
                                  double radius,
                                  RawBuffer<SearchResult>& results) const {
    if (!node) return;
    double dist = distance_metric(query, node->vantagePoint);
    emitTrace("range visit vp=\"" + node->vantagePoint.text + "\" dist="
            + std::to_string(dist)
            + " radius=" + std::to_string(radius)
            + " mu=" + std::to_string(node->medianDistance));
    // If vantage point is within radius, add it
    if (dist <= radius) {
        // Don't return self-matches (same file, same position)
        if (!(query.sourceFile == node->vantagePoint.sourceFile &&
              query.startPos == node->vantagePoint.startPos)) {
            results.append(SearchResult(node->vantagePoint, dist));
            emitTrace("  -> match accepted");
        }
    }
    double mu = node->medianDistance;
    // Left: items with d(vp, item) <= mu.
    if (dist - radius <= mu)
        rangeQueryRecursive(node->left, query, radius, results);
    // Right: items with d(vp, item) >= mu (tie items may live here).
    if (dist + radius >= mu)
        rangeQueryRecursive(node->right, query, radius, results);
}

// Function: range_query
// Public entry point for range query search.
// Time Complexity: O(log n) average, O(n) worst-case.
RawBuffer<SearchResult> VPTree::range_query(const NGram& query,
                                             double radius) const {
    emitTrace("range_query(start query=\"" + query.text + "\" radius="
            + std::to_string(radius) + ")");
    RawBuffer<SearchResult> results;
    rangeQueryRecursive(root_, query, radius, results);
    emitTrace("range_query(done matches=" + std::to_string(results.count()) + ")");
    return results;
}

// Function: knnInsert
// Inserts search result into a bounded priority queue.
// Time Complexity: O(k) worst-case.
void VPTree::knnInsert(RawBuffer<SearchResult>& results,
                        const SearchResult& sr,
                        int k,
                        double& tau) const {
    emitTrace("knn insert candidate dist=" + std::to_string(sr.distance));
    if (k <= 0) return;
    // Find insertion point — keep buffer sorted by descending distance
    results.append(sr);
    if (results.count() > k) {
        int worstIdx = 0;
        for (int i = 1; i < results.count(); ++i) {
            if (results[i].distance > results[worstIdx].distance)
                worstIdx = i;
        }
        results.removeAt(worstIdx);
        emitTrace("  removed farthest neighbor at idx=" + std::to_string(worstIdx));
    }
    if (results.count() == k) {
        tau = 0.0;
        for (int i = 0; i < results.count(); ++i) {
            if (results[i].distance > tau)
                tau = results[i].distance;
        }
        emitTrace("  tau updated=" + std::to_string(tau));
    }
}

// Function: knnRecursive
// Recursively searches for k-nearest neighbors using triangle inequality pruning.
// Time Complexity: O(log n) average, O(n) worst-case.
void VPTree::knnRecursive(VPTreeNode* node,
                           const NGram& query,
                           int k,
                           RawBuffer<SearchResult>& results,
                           double& tau) const {
    if (!node) return;
    double dist = distance_metric(query, node->vantagePoint);
    emitTrace("knn visit vp=\"" + node->vantagePoint.text + "\" dist="
            + std::to_string(dist)
            + " tau=" + std::to_string(tau)
            + " mu=" + std::to_string(node->medianDistance));
    if (!(query.sourceFile == node->vantagePoint.sourceFile &&
          query.startPos == node->vantagePoint.startPos)) {
        if (results.count() < k || dist < tau) {
            knnInsert(results, SearchResult(node->vantagePoint, dist), k, tau);
        }
    }
    double mu = node->medianDistance;
    // Decide traversal order: visit closer child first
    if (dist <= mu) {
        // Query is inside; search left first
        if (dist - tau <= mu)
            knnRecursive(node->left, query, k, results, tau);
        if (dist + tau >= mu)
            knnRecursive(node->right, query, k, results, tau);
    } else {
        // Query is outside; search right first
        if (dist + tau >= mu)
            knnRecursive(node->right, query, k, results, tau);
        if (dist - tau <= mu)
            knnRecursive(node->left, query, k, results, tau);
    }
}

// Function: search_knn
// Public entry point for k-nearest neighbor search.
// Time Complexity: O(log n) average, O(n) worst-case.
RawBuffer<SearchResult> VPTree::search_knn(const NGram& query, int k) const {
    emitTrace("search_knn(start query=\"" + query.text + "\" k="
            + std::to_string(k) + ")");
    RawBuffer<SearchResult> results;
    if (k <= 0) return results;
    double tau = 1e18;
    knnRecursive(root_, query, k, results, tau);
    emitTrace("search_knn(done neighbors=" + std::to_string(results.count()) + ")");
    return results;
}

// Function: insert
// Inserts a single item into the tree by collecting all items and rebuilding.
// Time Complexity: O(n log n) average.
void VPTree::insert(const NGram& item) {
    emitTrace("insert(start item=\"" + item.text + "\")");
    RawBuffer<NGram> all;
    collectAll(root_, all);
    all.append(item);
    clear();
    root_ = buildRecursive(all.rawData(), all.count());
    emitTrace("insert(done size=" + std::to_string(nodeCount_) + ")");
}

// Function: contains
// Checks if exact item exists in the tree.
// Time Complexity: O(n).
bool VPTree::contains(const NGram& item) const {
    emitTrace("contains(check item=\"" + item.text + "\")");
    RawBuffer<NGram> all;
    collectAll(root_, all);
    for (int i = 0; i < all.count(); ++i) {
        if (all[i] == item) {
            emitTrace("contains(result=true)");
            return true;
        }
    }
    emitTrace("contains(result=false)");
    return false;
}

// Function: remove
// Removes one exact item from the tree by rebuilding it.
// Time Complexity: O(n log n) average.
bool VPTree::remove(const NGram& item) {
    emitTrace("remove(start item=\"" + item.text + "\")");
    if (!root_) return false;
    RawBuffer<NGram> all;
    collectAll(root_, all);
    int removeIdx = -1;
    for (int i = 0; i < all.count(); ++i) {
        if (all[i] == item) {
            removeIdx = i;
            break;
        }
    }
    if (removeIdx < 0) {
        emitTrace("remove(result=false item-not-found)");
        return false;
    }
    all.removeAt(removeIdx);
    clear();
    if (all.count() > 0)
        root_ = buildRecursive(all.rawData(), all.count());
    emitTrace("remove(result=true size=" + std::to_string(nodeCount_) + ")");
    return true;
}

// Function: update
// Replaces one item with another and rebuilds the tree.
// Time Complexity: O(n log n) average.
bool VPTree::update(const NGram& oldItem, const NGram& newItem) {
    emitTrace("update(start old=\"" + oldItem.text + "\" new=\""
            + newItem.text + "\")");
    RawBuffer<NGram> all;
    collectAll(root_, all);
    int updateIdx = -1;
    for (int i = 0; i < all.count(); ++i) {
        if (all[i] == oldItem) {
            updateIdx = i;
            break;
        }
    }
    if (updateIdx < 0) {
        emitTrace("update(result=false old-item-not-found)");
        return false;
    }
    all[updateIdx] = newItem;
    clear();
    if (all.count() > 0)
        root_ = buildRecursive(all.rawData(), all.count());
    emitTrace("update(result=true size=" + std::to_string(nodeCount_) + ")");
    return true;
}

// Function: is_leaf
// Checks if the given node is a leaf node.
// Time Complexity: O(1).
bool VPTree::is_leaf(const VPTreeNode* node) {
    if (!node) return false;
    return node->left == nullptr && node->right == nullptr;
}

// Function: get_height
// Computes the maximum depth of the tree.
// Time Complexity: O(n).
int VPTree::get_height(const VPTreeNode* node) {
    if (!node) return 0;
    int lh = get_height(node->left);
    int rh = get_height(node->right);
    return 1 + (lh > rh ? lh : rh);
}

// Function: collectAll
// Collects all items in the subtree rooted at the given node.
// Time Complexity: O(n).
void VPTree::collectAll(VPTreeNode* node, RawBuffer<NGram>& out) const {
    if (!node) return;
    out.append(node->vantagePoint);
    collectAll(node->left, out);
    collectAll(node->right, out);
}

// Function: destroyRecursive
// Recursively deletes nodes to free memory.
// Time Complexity: O(n).
void VPTree::destroyRecursive(VPTreeNode* node) {
    if (!node) return;
    destroyRecursive(node->left);
    destroyRecursive(node->right);
    delete node;
}

// Function: clear
// Clears all nodes from the tree.
// Time Complexity: O(n).
void VPTree::clear() {
    emitTrace("clear(start)");
    destroyRecursive(root_);
    root_      = nullptr;
    nodeCount_ = 0;
    emitTrace("clear(done)");
}

// Function: rebuild
// Rebuilds the tree from current data.
// Time Complexity: O(n log n) average.
void VPTree::rebuild() {
    emitTrace("rebuild(start)");
    RawBuffer<NGram> all;
    collectAll(root_, all);
    clear();
    if (all.count() > 0)
        root_ = buildRecursive(all.rawData(), all.count());
    emitTrace("rebuild(done size=" + std::to_string(nodeCount_) + ")");
}

// Function: setTraceCallback
void VPTree::setTraceCallback(const std::function<void(const std::string&)>& cb) const {
    traceCallback_ = cb;
}

// Function: clearTraceCallback
void VPTree::clearTraceCallback() const {
    traceCallback_ = nullptr;
}

// Function: VPTree
VPTree::VPTree()
    : root_(nullptr), nodeCount_(0), traceCallback_(nullptr) {
    std::srand(42);   // deterministic for reproducibility
}

// Function: ~VPTree
VPTree::~VPTree() {
    clear();
}
