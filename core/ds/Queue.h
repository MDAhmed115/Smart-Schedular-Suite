#pragma once
#include <cstddef>
#include <stdexcept>
#include <utility>

template<class T>
class Queue {
    T* data_;
    std::size_t capacity_;
    std::size_t size_;
    std::size_t front_;

    void grow() {
        std::size_t next = capacity_ ? capacity_ * 2 : 4;
        T* fresh = new T[next];
        for (std::size_t i = 0; i < size_; ++i)
            fresh[i] = std::move(data_[(front_ + i) % capacity_]);
        delete[] data_;
        data_ = fresh;
        capacity_ = next;
        front_ = 0;
    }

public:
    Queue() : data_(nullptr), capacity_(0), size_(0), front_(0) {}
    Queue(const Queue& other) : Queue() {
        for (std::size_t i = 0; i < other.size_; ++i)
            enqueue(other.data_[(other.front_ + i) % other.capacity_]);
    }
    Queue& operator=(const Queue& other) {
        if (this == &other) return *this;
        Queue temp(other);
        swap(temp);
        return *this;
    }
    ~Queue() { delete[] data_; }

    void swap(Queue& other) noexcept {
        std::swap(data_, other.data_);
        std::swap(capacity_, other.capacity_);
        std::swap(size_, other.size_);
        std::swap(front_, other.front_);
    }

    void enqueue(const T& value) {
        if (size_ == capacity_) grow();
        data_[(front_ + size_) % capacity_] = value;
        ++size_;
    }

    T dequeue() {
        if (empty()) throw std::out_of_range("dequeue on empty Queue");
        T value = std::move(data_[front_]);
        front_ = (front_ + 1) % capacity_;
        --size_;
        if (size_ == 0) front_ = 0;
        return value;
    }

    T& front() {
        if (empty()) throw std::out_of_range("front on empty Queue");
        return data_[front_];
    }
    const T& front() const {
        if (empty()) throw std::out_of_range("front on empty Queue");
        return data_[front_];
    }
    bool empty() const { return size_ == 0; }
    std::size_t size() const { return size_; }
    void clear() { size_ = 0; front_ = 0; }
};
