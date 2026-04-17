#ifndef VPTREE_H
#define VPTREE_H

#include "ngram.h"
#include "rawbuffer.h"
#include <functional>

// Node in the VP-tree.
struct VPTreeNode {
    NGram        vantagePoint;
    double       medianDistance;   // mu: partition threshold
    VPTreeNode*  left;            // items with dist <= mu
    VPTreeNode*  right;           // items with dist >  mu

    VPTreeNode()
        : medianDistance(0.0), left(nullptr), right(nullptr) {}

    explicit VPTreeNode(const NGram& vp)
        : vantagePoint(vp), medianDistance(0.0),
          left(nullptr), right(nullptr) {}
};

// VP-tree using raw-pointer storage.
class VPTree {
private:
    VPTreeNode* root_;
    int         nodeCount_;
    mutable std::function<void(const std::string&)> traceCallback_;

    // Select k-th smallest value in-place.
    static double quickSelect(double* arr, int n, int k);

    // Emit backend trace message when callback is active.
    void emitTrace(const std::string& message) const;

    // Build subtree recursively from item range.
    VPTreeNode* buildRecursive(NGram* items, int count);

    // Delete subtree recursively.
    void destroyRecursive(VPTreeNode* node);

    // Range query helper.
    void rangeQueryRecursive(VPTreeNode* node,
                             const NGram& query,
                             double radius,
                             RawBuffer<SearchResult>& results) const;

    // KNN query helper.
    void knnRecursive(VPTreeNode* node,
                      const NGram& query,
                      int k,
                      RawBuffer<SearchResult>& results,
                      double& tau) const;

    // Insert candidate into KNN buffer.
    void knnInsert(RawBuffer<SearchResult>& results,
                   const SearchResult& sr,
                   int k,
                   double& tau) const;

    // Collect all n-grams from a subtree.
    void collectAll(VPTreeNode* node, RawBuffer<NGram>& out) const;

public:
    VPTree();
    ~VPTree();

    // Choose vantage point for partitioning.
    static NGram select_vantage_point(NGram* items, int count);

    // Compute median distance from vantage point.
    static double calculate_median_distance(const NGram& vp,
                                            NGram* items, int count);

    // Build tree from item array.
    void build_tree(NGram* items, int count);

    // Levenshtein edit distance metric.
    static double distance_metric(const NGram& a, const NGram& b);

    // Find up to k nearest neighbors.
    RawBuffer<SearchResult> search_knn(const NGram& query, int k) const;

    // Find all items within radius.
    RawBuffer<SearchResult> range_query(const NGram& query, double radius) const;

    // Insert one item.
    void insert(const NGram& item);

    // Check if exact item exists.
    bool contains(const NGram& item) const;

    // Remove one exact item.
    bool remove(const NGram& item);

    // Replace one item with another.
    bool update(const NGram& oldItem, const NGram& newItem);

    // Check leaf node.
    static bool is_leaf(const VPTreeNode* node);

    // Compute tree height.
    static int get_height(const VPTreeNode* node);

    // Set trace callback.
    void setTraceCallback(const std::function<void(const std::string&)>& cb) const;

    // Clear trace callback.
    void clearTraceCallback() const;

    // Accessors
    int         size()   const { return nodeCount_; }
    bool        empty()  const { return root_ == nullptr; }
    VPTreeNode* root()   const { return root_; }

    // Clear all nodes.
    void clear();

    // Rebuild from current data.
    void rebuild();
};

#endif // VPTREE_H
