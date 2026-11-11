#pragma once
#include "BinaryTree.h"
#include <functional>

template<class T, class Cmp = std::less<T>>
class BST {
    TreeNode<T>* root_ = nullptr;
    Cmp cmp_;

    TreeNode<T>* insertAt(TreeNode<T>* node, TreeNode<T>* parent, const T& value) {
        if (!node) return new TreeNode<T>(value, parent);
        if (cmp_(value, node->val)) node->left = insertAt(node->left, node, value);
        else if (cmp_(node->val, value)) node->right = insertAt(node->right, node, value);
        return node;
    }

    TreeNode<T>* findNode(const T& value) const {
        TreeNode<T>* p = root_;
        while (p) {
            if (cmp_(value, p->val)) p = p->left;
            else if (cmp_(p->val, value)) p = p->right;
            else return p;
        }
        return nullptr;
    }

public:
    BST() = default;
    BST(const BST&) = delete;
    BST& operator=(const BST&) = delete;
    ~BST() { destroyTree(root_); }

    void insert(const T& value) { root_ = insertAt(root_, nullptr, value); }
    bool contains(const T& value) const { return findNode(value) != nullptr; }
    TreeNode<T>* root() const { return root_; }

    bool erase(const T& value) {
        TreeNode<T>* node = findNode(value);
        if (!node) return false;
        if (node->left && node->right) {
            TreeNode<T>* successor = node->right;
            while (successor->left) successor = successor->left;
            node->val = successor->val;
            node = successor;
        }
        TreeNode<T>* child = node->left ? node->left : node->right;
        if (child) child->parent = node->parent;
        if (!node->parent) root_ = child;
        else if (node->parent->left == node) node->parent->left = child;
        else node->parent->right = child;
        delete node;
        return true;
    }
};
