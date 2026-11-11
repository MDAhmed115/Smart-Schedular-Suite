#pragma once
#include <cstddef>
#include <utility>

template<class T>
struct ListNode {
    T val;
    ListNode* prev;
    ListNode* next;
    explicit ListNode(const T& value) : val(value), prev(nullptr), next(nullptr) {}
};

template<class T>
class LinkedList {
    ListNode<T>* head_;
    ListNode<T>* tail_;
    std::size_t size_;

    void copyFrom(const LinkedList& other) {
        for (ListNode<T>* p = other.head_; p; p = p->next) push_back(p->val);
    }

public:
    LinkedList() : head_(nullptr), tail_(nullptr), size_(0) {}
    LinkedList(const LinkedList& other) : LinkedList() { copyFrom(other); }

    LinkedList& operator=(const LinkedList& other) {
        if (this == &other) return *this;
        LinkedList temp(other);
        clear();
        head_ = temp.head_;
        tail_ = temp.tail_;
        size_ = temp.size_;
        temp.head_ = temp.tail_ = nullptr;
        temp.size_ = 0;
        return *this;
    }

    ~LinkedList() { clear(); }

    void push_front(const T& value) {
        ListNode<T>* n = new ListNode<T>(value);
        n->next = head_;
        if (head_) head_->prev = n;
        else tail_ = n;
        head_ = n;
        ++size_;
    }

    void push_back(const T& value) {
        ListNode<T>* n = new ListNode<T>(value);
        n->prev = tail_;
        if (tail_) tail_->next = n;
        else head_ = n;
        tail_ = n;
        ++size_;
    }

    void pop_front() {
        if (!head_) return;
        ListNode<T>* old = head_;
        head_ = head_->next;
        if (head_) head_->prev = nullptr;
        else tail_ = nullptr;
        delete old;
        --size_;
    }

    void pop_back() {
        if (!tail_) return;
        ListNode<T>* old = tail_;
        tail_ = tail_->prev;
        if (tail_) tail_->next = nullptr;
        else head_ = nullptr;
        delete old;
        --size_;
    }

    void remove(ListNode<T>* node) {
        if (!node) return;
        if (node->prev) node->prev->next = node->next;
        else if (head_ == node) head_ = node->next;
        else return;
        if (node->next) node->next->prev = node->prev;
        else tail_ = node->prev;
        delete node;
        --size_;
    }

    ListNode<T>* head() const { return head_; }
    ListNode<T>* tail() const { return tail_; }
    std::size_t size() const { return size_; }
    bool empty() const { return size_ == 0; }

    void clear() {
        while (head_) {
            ListNode<T>* next = head_->next;
            delete head_;
            head_ = next;
        }
        tail_ = nullptr;
        size_ = 0;
    }
};
