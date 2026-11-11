#pragma once
#include "Queue.h"
#include <cstddef>
#include <functional>

template<class T>
struct TreeNode {
    T val;
    TreeNode* left;
    TreeNode* right;
    TreeNode* parent;
    explicit TreeNode(const T& value, TreeNode* p = nullptr)
        : val(value), left(nullptr), right(nullptr), parent(p) {}
};

template<class T, class Visit>
void inorder(TreeNode<T>* root, Visit visit) {
    if (!root) return;
    inorder(root->left, visit);
    visit(root->val);
    inorder(root->right, visit);
}

template<class T, class Visit>
void levelOrder(TreeNode<T>* root, Visit visit) {
    if (!root) return;
    Queue<TreeNode<T>*> q;
    q.enqueue(root);
    while (!q.empty()) {
        TreeNode<T>* node = q.dequeue();
        visit(node->val);
        if (node->left) q.enqueue(node->left);
        if (node->right) q.enqueue(node->right);
    }
}

template<class T>
int height(TreeNode<T>* root) {
    if (!root) return 0;
    int left = height(root->left), right = height(root->right);
    return 1 + (left > right ? left : right);
}

template<class T>
void destroyTree(TreeNode<T>* root) {
    if (!root) return;
    destroyTree(root->left);
    destroyTree(root->right);
    delete root;
}
