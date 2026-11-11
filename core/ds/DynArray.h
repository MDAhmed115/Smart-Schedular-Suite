#pragma once
#include <cstddef>
#include <stdexcept>
#include <utility>

template<class T>
class DynArray {
    T* data_;
    std::size_t size_;
    std::size_t capacity_;

    void grow(std::size_t minimum) {
        if (capacity_ >= minimum) return;
        std::size_t next = capacity_ ? capacity_ * 2 : 4;
        if (next < minimum) next = minimum;
        T* fresh = new T[next];
        for (std::size_t i = 0; i < size_; ++i) fresh[i] = std::move(data_[i]);
        delete[] data_;
        data_ = fresh;
        capacity_ = next;
    }

public:
    DynArray() : data_(nullptr), size_(0), capacity_(0) {}

    DynArray(const DynArray& other)
        : data_(other.capacity_ ? new T[other.capacity_] : nullptr),
          size_(other.size_), capacity_(other.capacity_) {
        for (std::size_t i = 0; i < size_; ++i) data_[i] = other.data_[i];
    }

    DynArray& operator=(const DynArray& other) {
        if (this == &other) return *this;
        T* fresh = other.capacity_ ? new T[other.capacity_] : nullptr;
        try {
            for (std::size_t i = 0; i < other.size_; ++i) fresh[i] = other.data_[i];
        } catch (...) {
            delete[] fresh;
            throw;
        }
        delete[] data_;
        data_ = fresh;
        size_ = other.size_;
        capacity_ = other.capacity_;
        return *this;
    }

    ~DynArray() { delete[] data_; }

    std::size_t size() const { return size_; }
    bool empty() const { return size_ == 0; }
    std::size_t capacity() const { return capacity_; }

    T& at(std::size_t i) {
        if (i >= size_) throw std::out_of_range("DynArray index out of range");
        return data_[i];
    }
    const T& at(std::size_t i) const {
        if (i >= size_) throw std::out_of_range("DynArray index out of range");
        return data_[i];
    }

    T& operator[](std::size_t i) { return data_[i]; }
    const T& operator[](std::size_t i) const { return data_[i]; }

    void push_back(const T& value) {
        T copy = value;
        grow(size_ + 1);
        data_[size_++] = copy;
    }

    void pop_back() {
        if (empty()) throw std::out_of_range("pop_back on empty DynArray");
        --size_;
    }

    void insert(std::size_t i, const T& value) {
        if (i > size_) throw std::out_of_range("DynArray insert index out of range");
        T copy = value;
        grow(size_ + 1);
        for (std::size_t j = size_; j > i; --j) data_[j] = std::move(data_[j - 1]);
        data_[i] = copy;
        ++size_;
    }

    void erase(std::size_t i) {
        if (i >= size_) throw std::out_of_range("DynArray erase index out of range");
        for (std::size_t j = i; j + 1 < size_; ++j) data_[j] = std::move(data_[j + 1]);
        --size_;
    }

    void clear() { size_ = 0; }
    void reserve(std::size_t n) { grow(n); }

    T* begin() { return data_; }
    T* end() { return data_ + size_; }
    const T* begin() const { return data_; }
    const T* end() const { return data_ + size_; }
};
