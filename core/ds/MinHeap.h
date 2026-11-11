#pragma once
#include "DynArray.h"
#include <functional>
#include <stdexcept>
#include <utility>

template<class T, class Cmp = std::less<T>>
class MinHeap {
    DynArray<T> data_;
    Cmp cmp_;

    void siftUp(std::size_t i) {
        while (i > 0) {
            std::size_t p = (i - 1) / 2;
            if (!cmp_(data_[i], data_[p])) break;
            std::swap(data_[i], data_[p]);
            i = p;
        }
    }

    void siftDown(std::size_t i) {
        while (true) {
            std::size_t left = i * 2 + 1, right = left + 1, best = i;
            if (left < size() && cmp_(data_[left], data_[best])) best = left;
            if (right < size() && cmp_(data_[right], data_[best])) best = right;
            if (best == i) break;
            std::swap(data_[i], data_[best]);
            i = best;
        }
    }

public:
    void push(const T& value) { data_.push_back(value); siftUp(size() - 1); }
    void pop() {
        if (empty()) throw std::out_of_range("pop on empty MinHeap");
        if (size() == 1) { data_.pop_back(); return; }
        data_[0] = data_[size() - 1];
        data_.pop_back();
        siftDown(0);
    }
    const T& top() const {
        if (empty()) throw std::out_of_range("top on empty MinHeap");
        return data_[0];
    }
    std::size_t size() const { return data_.size(); }
    bool empty() const { return data_.empty(); }

    void heapify(const DynArray<T>& values) {
        data_ = values;
        for (std::size_t i = size() / 2; i > 0; --i) siftDown(i - 1);
    }

    DynArray<T> drain() {
        DynArray<T> result;
        result.reserve(size());
        while (!empty()) {
            result.push_back(top());
            pop();
        }
        return result;
    }
};
