#include "vptree.h"
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
double VPTree::quickSelect(double* arr, int n, int k) {
    if (n <= 1) return arr[0];
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
        else if (k >= i)  left  = i;
        else              break;
    }
    return arr[k];
}

// Function: select_vantage_point
NGram VPTree::select_vantage_point(NGram* items, int count) {
    if (count <= 1) return items[0];
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
double VPTree::calculate_median_distance(const NGram& vp,
                                          NGram* items, int count) {
    if (count == 0) return 0.0;
    double* dists = new double[count];
    for (int i = 0; i < count; ++i)
        dists[i] = distance_metric(vp, items[i]);
    int medianIdx = count / 2;
    double median = quickSelect(dists, count, medianIdx);
    delete[] dists;
    return median;
}

// Function: buildRecursive
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
    // 3. Partition rest into inside (<= median) and outside (> median)
    NGram* inside  = new NGram[restSize];
    NGram* outside = new NGram[restSize];
    int    inCount = 0, outCount = 0;
    for (int i = 0; i < restSize; ++i) {
        double d = distance_metric(node->vantagePoint, rest[i]);
        if (d <= median)
            inside[inCount++]   = rest[i];
        else
            outside[outCount++] = rest[i];
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
void VPTree::build_tree(NGram* items, int count) {
    emitTrace("build_tree(start count=" + std::to_string(count) + ")");
    clear();
    if (count > 0)
        root_ = buildRecursive(items, count);
    emitTrace("build_tree(done nodes=" + std::to_string(nodeCount_) + ")");
}

// Function: rangeQueryRecursive
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
    // Pruning: only visit children that could contain results
    // Left child (inside): items with dist(vp, item) <= mu
    //   Can contain results if dist - radius <= mu
    if (dist - radius <= mu)
        rangeQueryRecursive(node->left, query, radius, results);
    // Right child (outside): items with dist(vp, item) > mu
    //   Can contain results if dist + radius > mu
    if (dist + radius > mu)
        rangeQueryRecursive(node->right, query, radius, results);
}

// Function: range_query
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
void VPTree::knnInsert(RawBuffer<SearchResult>& results,
                        const SearchResult& sr,
                        int k,
                        double& tau) const {
    emitTrace("knn insert candidate dist=" + std::to_string(sr.distance));
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
        if (dist + tau > mu)
            knnRecursive(node->right, query, k, results, tau);
    } else {
        // Query is outside; search right first
        if (dist + tau > mu)
            knnRecursive(node->right, query, k, results, tau);
        if (dist - tau <= mu)
            knnRecursive(node->left, query, k, results, tau);
    }
}

// Function: search_knn
RawBuffer<SearchResult> VPTree::search_knn(const NGram& query, int k) const {
    emitTrace("search_knn(start query=\"" + query.text + "\" k="
            + std::to_string(k) + ")");
    RawBuffer<SearchResult> results;
    double tau = 1e18;
    knnRecursive(root_, query, k, results, tau);
    emitTrace("search_knn(done neighbors=" + std::to_string(results.count()) + ")");
    return results;
}

// Function: insert
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
bool VPTree::is_leaf(const VPTreeNode* node) {
    if (!node) return false;
    return node->left == nullptr && node->right == nullptr;
}

// Function: get_height
int VPTree::get_height(const VPTreeNode* node) {
    if (!node) return 0;
    int lh = get_height(node->left);
    int rh = get_height(node->right);
    return 1 + (lh > rh ? lh : rh);
}

// Function: collectAll
void VPTree::collectAll(VPTreeNode* node, RawBuffer<NGram>& out) const {
    if (!node) return;
    out.append(node->vantagePoint);
    collectAll(node->left, out);
    collectAll(node->right, out);
}

// Function: destroyRecursive
void VPTree::destroyRecursive(VPTreeNode* node) {
    if (!node) return;
    destroyRecursive(node->left);
    destroyRecursive(node->right);
    delete node;
}

// Function: clear
void VPTree::clear() {
    emitTrace("clear(start)");
    destroyRecursive(root_);
    root_      = nullptr;
    nodeCount_ = 0;
    emitTrace("clear(done)");
}

// Function: rebuild
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
