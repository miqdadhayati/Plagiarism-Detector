#ifndef RAWBUFFER_H
#define RAWBUFFER_H

#include <cstring>
#include <utility>

// ============================================================================
// RawBuffer<T> — A manual-memory dynamic buffer using new[]/delete[].
// This is NOT a standard container. It satisfies the constraint of using
// "raw pointers and manual memory management" for all internal storage.
// ============================================================================
template <typename T>
class RawBuffer {
private:
    T*  data_;
    int size_;
    int capacity_;

    void grow() {
        int newCap = (capacity_ == 0) ? 4 : capacity_ * 2;
        T* newData = new T[newCap];
        for (int i = 0; i < size_; ++i)
            newData[i] = std::move(data_[i]);
        delete[] data_;
        data_     = newData;
        capacity_ = newCap;
    }

public:
    RawBuffer() : data_(nullptr), size_(0), capacity_(0) {}

    RawBuffer(int initialCap)
        : data_(new T[initialCap]), size_(0), capacity_(initialCap) {}

    ~RawBuffer() { delete[] data_; }

    // Copy
    RawBuffer(const RawBuffer& o)
        : data_(nullptr), size_(0), capacity_(0) {
        if (o.size_ > 0) {
            data_     = new T[o.size_];
            capacity_ = o.size_;
            size_     = o.size_;
            for (int i = 0; i < size_; ++i)
                data_[i] = o.data_[i];
        }
    }

    RawBuffer& operator=(const RawBuffer& o) {
        if (this != &o) {
            delete[] data_;
            data_ = nullptr;
            size_ = capacity_ = 0;
            if (o.size_ > 0) {
                data_     = new T[o.size_];
                capacity_ = o.size_;
                size_     = o.size_;
                for (int i = 0; i < size_; ++i)
                    data_[i] = o.data_[i];
            }
        }
        return *this;
    }

    // Move
    RawBuffer(RawBuffer&& o) noexcept
        : data_(o.data_), size_(o.size_), capacity_(o.capacity_) {
        o.data_ = nullptr;
        o.size_ = o.capacity_ = 0;
    }

    RawBuffer& operator=(RawBuffer&& o) noexcept {
        if (this != &o) {
            delete[] data_;
            data_     = o.data_;
            size_     = o.size_;
            capacity_ = o.capacity_;
            o.data_   = nullptr;
            o.size_ = o.capacity_ = 0;
        }
        return *this;
    }

    void append(const T& val) {
        if (size_ >= capacity_) grow();
        data_[size_++] = val;
    }

    void append(T&& val) {
        if (size_ >= capacity_) grow();
        data_[size_++] = std::move(val);
    }

    void removeAt(int idx) {
        if (idx < 0 || idx >= size_) return;
        for (int i = idx; i < size_ - 1; ++i)
            data_[i] = std::move(data_[i + 1]);
        --size_;
    }

    void clear() { size_ = 0; }

    void swapElements(int i, int j) {
        T tmp       = std::move(data_[i]);
        data_[i]    = std::move(data_[j]);
        data_[j]    = std::move(tmp);
    }

    T&       operator[](int i)       { return data_[i]; }
    const T& operator[](int i) const { return data_[i]; }

    int  count()    const { return size_; }
    bool isEmpty()  const { return size_ == 0; }
    T*   rawData()        { return data_; }
    const T* rawData() const { return data_; }

    // Simple linear search
    bool contains(const T& val) const {
        for (int i = 0; i < size_; ++i)
            if (data_[i] == val) return true;
        return false;
    }
};

#endif // RAWBUFFER_H
