#pragma once
#include "DynArray.h"
#include <stdexcept>

template<class T>
class Stack {
    DynArray<T> data_;
public:
    void push(const T& value) { data_.push_back(value); }
    void pop() {
        if (empty()) throw std::out_of_range("pop on empty Stack");
        data_.pop_back();
    }
    T& top() {
        if (empty()) throw std::out_of_range("top on empty Stack");
        return data_[data_.size() - 1];
    }
    const T& top() const {
        if (empty()) throw std::out_of_range("top on empty Stack");
        return data_[data_.size() - 1];
    }
    bool empty() const { return data_.empty(); }
    std::size_t size() const { return data_.size(); }
    void clear() { data_.clear(); }
    const DynArray<T>& items() const { return data_; }
};
