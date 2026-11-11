#pragma once
#include "DynArray.h"
#include "LinkedList.h"
#include <cstddef>
#include <functional>
#include <string>

inline std::size_t hashString(const std::string& value) {
    std::size_t hash = 5381;
    for (unsigned char c : value) hash = hash * 33 + c;
    return hash;
}

struct StringHash {
    std::size_t operator()(const std::string& value) const { return hashString(value); }
};

struct IntHash {
    std::size_t operator()(int value) const {
        return static_cast<std::size_t>(value) * 2654435761u;
    }
};

template<class K, class V, class Hash = std::hash<K>>
class HashTable {
    struct Entry { K key; V value; };
    DynArray<LinkedList<Entry>> buckets_;
    std::size_t size_;
    Hash hash_;

    void rehash(std::size_t count) {
        DynArray<LinkedList<Entry>> fresh;
        for (std::size_t i = 0; i < count; ++i) fresh.push_back(LinkedList<Entry>());
        for (std::size_t i = 0; i < buckets_.size(); ++i) {
            for (ListNode<Entry>* node = buckets_[i].head(); node; node = node->next) {
                std::size_t index = hash_(node->val.key) % count;
                fresh[index].push_back(node->val);
            }
        }
        buckets_ = fresh;
    }

public:
    explicit HashTable(std::size_t bucketCount = 8) : size_(0) {
        if (bucketCount == 0) bucketCount = 8;
        for (std::size_t i = 0; i < bucketCount; ++i)
            buckets_.push_back(LinkedList<Entry>());
    }

    bool insert(const K& key, const V& value) {
        std::size_t index = hash_(key) % buckets_.size();
        for (ListNode<Entry>* node = buckets_[index].head(); node; node = node->next) {
            if (node->val.key == key) {
                node->val.value = value;
                return false;
            }
        }
        buckets_[index].push_back(Entry{key, value});
        ++size_;
        if (loadFactor() > 0.75) rehash(buckets_.size() * 2);
        return true;
    }

    V* find(const K& key) {
        std::size_t index = hash_(key) % buckets_.size();
        for (ListNode<Entry>* node = buckets_[index].head(); node; node = node->next)
            if (node->val.key == key) return &node->val.value;
        return nullptr;
    }

    const V* find(const K& key) const {
        std::size_t index = hash_(key) % buckets_.size();
        for (ListNode<Entry>* node = buckets_[index].head(); node; node = node->next)
            if (node->val.key == key) return &node->val.value;
        return nullptr;
    }

    bool contains(const K& key) const { return find(key) != nullptr; }

    bool erase(const K& key) {
        std::size_t index = hash_(key) % buckets_.size();
        for (ListNode<Entry>* node = buckets_[index].head(); node; node = node->next) {
            if (node->val.key == key) {
                buckets_[index].remove(node);
                --size_;
                return true;
            }
        }
        return false;
    }

    std::size_t size() const { return size_; }
    double loadFactor() const {
        return static_cast<double>(size_) / static_cast<double>(buckets_.size());
    }
};
