#ifndef VPTREE_H
#define VPTREE_H

#include "ngram.hpp"
#include "rawbuffer.hpp"

// ============================================================================
// VPTreeNode — A node in the Vantage-Point Tree.
// Stores the vantage point NGram, the partitioning median distance,
// and raw pointers to left (inside) and right (outside) children.
// ============================================================================
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

// ============================================================================
// VPTree — Vantage-Point Tree for metric-space nearest-neighbor search.
//
// All internal storage uses raw pointers and manual memory management.
// No std::vector, std::list, std::map, or any forbidden container is used.
//
// Required methods:
//   select_vantage_point()      — Chooses a pivot from a set of items
//   calculate_median_distance() — Computes median dist from vantage point
//   build_tree()                — Recursive tree construction
//   distance_metric()           — Levenshtein-based normalized distance
//   search_knn()                — K-nearest-neighbor search
//   range_query()               — Fixed-radius search
//   insert()                    — Add a single item to existing tree
//   is_leaf()                   — Check if node is a leaf
//   get_height()                — Tree height
// ============================================================================
class VPTree {
private:
    VPTreeNode* root_;
    int         nodeCount_;

    // ---- Internal helpers (all use raw pointers) ----

    // Quickselect-style partitioning to find the k-th smallest element
    // in a raw array of doubles. Modifies the array in place.
    static double quickSelect(double* arr, int n, int k);

    // Partition items around the median distance from vp, returns built node.
    VPTreeNode* buildRecursive(NGram* items, int count);

    // Recursively free nodes
    void destroyRecursive(VPTreeNode* node);

    // Internal recursive range query
    void rangeQueryRecursive(VPTreeNode* node,
                             const NGram& query,
                             double radius,
                             RawBuffer<SearchResult>& results) const;

    // Internal recursive KNN query
    void knnRecursive(VPTreeNode* node,
                      const NGram& query,
                      int k,
                      RawBuffer<SearchResult>& results,
                      double& tau) const;

    // Insert a result into KNN buffer, maintaining size <= k
    void knnInsert(RawBuffer<SearchResult>& results,
                   const SearchResult& sr,
                   int k,
                   double& tau) const;

    // Collect all items under a subtree (for rebuild)
    void collectAll(VPTreeNode* node, RawBuffer<NGram>& out) const;

public:
    VPTree();
    ~VPTree();

    // ---- Required public interface ----

    /// Select a vantage point from an array of NGrams.
    /// Strategy: pick the item that maximizes spread (variance of distances).
    /// For efficiency, samples a subset and picks the best candidate.
    static NGram select_vantage_point(NGram* items, int count);

    /// Calculate the median distance from the vantage point to all items.
    static double calculate_median_distance(const NGram& vp,
                                            NGram* items, int count);

    /// Build the tree from an array of NGrams.
    /// The array is consumed (reordered) during construction.
    void build_tree(NGram* items, int count);

    /// Metric space distance: normalized Levenshtein edit distance.
    /// Returns a value in [0.0, 1.0] where 0.0 = identical.
    static double distance_metric(const NGram& a, const NGram& b);

    /// K-nearest-neighbor search. Returns up to k closest items.
    RawBuffer<SearchResult> search_knn(const NGram& query, int k) const;

    /// Range query: returns all items within `radius` distance of query.
    RawBuffer<SearchResult> range_query(const NGram& query, double radius) const;

    /// Insert a single NGram into the existing tree.
    void insert(const NGram& item);

    /// Check if a node is a leaf (no children).
    static bool is_leaf(const VPTreeNode* node);

    /// Get the height of the tree (or subtree).
    static int get_height(const VPTreeNode* node);

    /// Accessors
    int         size()   const { return nodeCount_; }
    bool        empty()  const { return root_ == nullptr; }
    VPTreeNode* root()   const { return root_; }

    /// Clear all nodes
    void clear();

    /// Rebuild tree from existing data (useful after many inserts)
    void rebuild();
};

#endif // VPTREE_H
