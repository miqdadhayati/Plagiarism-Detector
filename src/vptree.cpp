#include "vptree.h"
#include <cmath>
#include <cstdlib>
#include <algorithm>
#include <climits>

// ============================================================================
// DISTANCE METRIC — Normalized Levenshtein Edit Distance
// ============================================================================
double VPTree::distance_metric(const NGram& a, const NGram& b) {
    const std::string& s = a.text;
    const std::string& t = b.text;
    int m = static_cast<int>(s.size());
    int n = static_cast<int>(t.size());

    if (m == 0 && n == 0) return 0.0;
    if (m == 0) return 1.0;
    if (n == 0) return 1.0;

    // Two-row Levenshtein using raw pointers
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
        // Swap rows
        int* tmp = prev;
        prev = curr;
        curr = tmp;
    }

    double editDist = static_cast<double>(prev[n]);
    double maxLen   = static_cast<double>(m > n ? m : n);

    delete[] prev;
    delete[] curr;

    return editDist / maxLen;   // Normalized to [0, 1]
}

// ============================================================================
// QUICKSELECT — find k-th smallest in raw double array (in-place)
// ============================================================================
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

// ============================================================================
// SELECT VANTAGE POINT
// Strategy: sample up to 5 candidates, pick the one whose distances to
// a random subset have the highest variance (spread).  High spread gives
// better partitioning.
// ============================================================================
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

// ============================================================================
// CALCULATE MEDIAN DISTANCE
// ============================================================================
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

// ============================================================================
// BUILD TREE — Recursive construction
// ============================================================================
VPTreeNode* VPTree::buildRecursive(NGram* items, int count) {
    if (count == 0) return nullptr;

    if (count == 1) {
        VPTreeNode* leaf = new VPTreeNode(items[0]);
        ++nodeCount_;
        return leaf;
    }

    // 1. Select vantage point
    NGram vp = select_vantage_point(items, count);

    // Find and swap vp to front
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

    // 3. Partition rest into inside (<= median) and outside (> median)
    //    using raw pointer arrays
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

    // 4. Recurse
    node->left  = buildRecursive(inside,  inCount);
    node->right = buildRecursive(outside, outCount);

    delete[] inside;
    delete[] outside;

    return node;
}

void VPTree::build_tree(NGram* items, int count) {
    clear();
    if (count > 0)
        root_ = buildRecursive(items, count);
}

// ============================================================================
// RANGE QUERY — find all items within `radius` of query
// ============================================================================
void VPTree::rangeQueryRecursive(VPTreeNode* node,
                                  const NGram& query,
                                  double radius,
                                  RawBuffer<SearchResult>& results) const {
    if (!node) return;

    double dist = distance_metric(query, node->vantagePoint);

    // If vantage point is within radius, add it
    if (dist <= radius) {
        // Don't return self-matches (same file, same position)
        if (!(query.sourceFile == node->vantagePoint.sourceFile &&
              query.startPos == node->vantagePoint.startPos)) {
            results.append(SearchResult(node->vantagePoint, dist));
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

RawBuffer<SearchResult> VPTree::range_query(const NGram& query,
                                             double radius) const {
    RawBuffer<SearchResult> results;
    rangeQueryRecursive(root_, query, radius, results);
    return results;
}

// ============================================================================
// KNN SEARCH — find k nearest neighbors
// ============================================================================
void VPTree::knnInsert(RawBuffer<SearchResult>& results,
                        const SearchResult& sr,
                        int k,
                        double& tau) const {
    // Find insertion point — keep buffer sorted by descending distance
    // so the worst (farthest) is at end for easy removal
    results.append(sr);

    if (results.count() > k) {
        // Find and remove the farthest
        int worstIdx = 0;
        for (int i = 1; i < results.count(); ++i) {
            if (results[i].distance > results[worstIdx].distance)
                worstIdx = i;
        }
        results.removeAt(worstIdx);
    }

    // Update tau to the farthest remaining neighbor
    if (results.count() == k) {
        tau = 0.0;
        for (int i = 0; i < results.count(); ++i) {
            if (results[i].distance > tau)
                tau = results[i].distance;
        }
    }
}

void VPTree::knnRecursive(VPTreeNode* node,
                           const NGram& query,
                           int k,
                           RawBuffer<SearchResult>& results,
                           double& tau) const {
    if (!node) return;

    double dist = distance_metric(query, node->vantagePoint);

    // Consider this node
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

RawBuffer<SearchResult> VPTree::search_knn(const NGram& query, int k) const {
    RawBuffer<SearchResult> results;
    double tau = 1e18;
    knnRecursive(root_, query, k, results, tau);
    return results;
}

// ============================================================================
// INSERT — Add a single item to the existing tree
// ============================================================================
void VPTree::insert(const NGram& item) {
    if (!root_) {
        root_ = new VPTreeNode(item);
        ++nodeCount_;
        return;
    }

    // Traverse tree to find insertion point
    VPTreeNode* curr = root_;
    while (true) {
        double dist = distance_metric(item, curr->vantagePoint);

        if (dist <= curr->medianDistance) {
            if (!curr->left) {
                curr->left = new VPTreeNode(item);
                ++nodeCount_;
                return;
            }
            curr = curr->left;
        } else {
            if (!curr->right) {
                curr->right = new VPTreeNode(item);
                ++nodeCount_;
                return;
            }
            curr = curr->right;
        }
    }
}

// ============================================================================
// UTILITY METHODS
// ============================================================================
bool VPTree::is_leaf(const VPTreeNode* node) {
    if (!node) return false;
    return node->left == nullptr && node->right == nullptr;
}

int VPTree::get_height(const VPTreeNode* node) {
    if (!node) return 0;
    int lh = get_height(node->left);
    int rh = get_height(node->right);
    return 1 + (lh > rh ? lh : rh);
}

void VPTree::collectAll(VPTreeNode* node, RawBuffer<NGram>& out) const {
    if (!node) return;
    out.append(node->vantagePoint);
    collectAll(node->left, out);
    collectAll(node->right, out);
}

void VPTree::destroyRecursive(VPTreeNode* node) {
    if (!node) return;
    destroyRecursive(node->left);
    destroyRecursive(node->right);
    delete node;
}

void VPTree::clear() {
    destroyRecursive(root_);
    root_      = nullptr;
    nodeCount_ = 0;
}

void VPTree::rebuild() {
    RawBuffer<NGram> all;
    collectAll(root_, all);
    clear();
    if (all.count() > 0)
        root_ = buildRecursive(all.rawData(), all.count());
}

// ============================================================================
// CONSTRUCTOR / DESTRUCTOR
// ============================================================================
VPTree::VPTree()
    : root_(nullptr), nodeCount_(0) {
    std::srand(42);   // deterministic for reproducibility
}

VPTree::~VPTree() {
    clear();
}
