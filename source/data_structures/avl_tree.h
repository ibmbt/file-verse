#ifndef AVL_TREE_H
#define AVL_TREE_H

#include <string>
#include <vector>
#include "../include/odf_types.hpp"

using namespace std;

template<typename T>
struct AVLNode {
    string value;
    T data;
    AVLNode* left;
    AVLNode* right;
    int height;
    
    AVLNode(const string& v, const T& d) 
        : value(v), data(d), left(nullptr), right(nullptr), height(1) {}
};

template<typename T>
class AVLTree {
private:
    AVLNode<T>* root;
    
    int height(AVLNode<T>* node) {
        if (node == nullptr) return 0;
        return node->height;
    }
    
    int getBalance(AVLNode<T>* node) {
        if (node == nullptr) return 0;
        return height(node->left) - height(node->right);
    }
    
    void updateHeight(AVLNode<T>* node) {
        if (node != nullptr) {
            int leftHeight = height(node->left);
            int rightHeight = height(node->right);
            
            if (leftHeight > rightHeight) {
                node->height = 1 + leftHeight;
            } 
            else {
                node->height = 1 + rightHeight;
            }
        }
    }

    
    AVLNode<T>* rotateRight(AVLNode<T>* y) {
        AVLNode<T>* x = y->left;
        AVLNode<T>* T2 = x->right;
        
        x->right = y;
        y->left = T2;
        
        updateHeight(y);
        updateHeight(x);
        
        return x;
    }
    
    AVLNode<T>* rotateLeft(AVLNode<T>* x) {
        AVLNode<T>* y = x->right;
        AVLNode<T>* T2 = y->left;
        
        y->left = x;
        x->right = T2;
        
        updateHeight(x);
        updateHeight(y);
        
        return y;
    }
    
    AVLNode<T>* insertHelper(AVLNode<T>* node, const string& value, const T& data) {
        if (node == nullptr) {
            return new AVLNode<T>(value, data);
        }
        
        if (value < node->value) {
            node->left = insertHelper(node->left, value, data);
        } else if (value > node->value) {
            node->right = insertHelper(node->right, value, data);
        } else {
            node->data = data;
            return node;
        }
        
        updateHeight(node);
        
        int balance = getBalance(node);
        
        if (balance > 1 && value < node->left->value) {
            return rotateRight(node);
        }
        
        if (balance < -1 && value > node->right->value) {
            return rotateLeft(node);
        }
        
        if (balance > 1 && value > node->left->value) {
            node->left = rotateLeft(node->left);
            return rotateRight(node);
        }
        
        if (balance < -1 && value < node->right->value) {
            node->right = rotateRight(node->right);
            return rotateLeft(node);
        }
        
        return node;
    }
    
    AVLNode<T>* findMin(AVLNode<T>* node) {
        while (node->left != nullptr) {
            node = node->left;
        }
        return node;
    }
    
    AVLNode<T>* deleteHelper(AVLNode<T>* node, const string& value) {
    if (node == nullptr) return nullptr;

    if (value < node->value) {
        node->left = deleteHelper(node->left, value);
    } else if (value > node->value) {
        node->right = deleteHelper(node->right, value);
    } else {
        if (node->left == nullptr || node->right == nullptr) {
            AVLNode<T>* temp = nullptr;
            if (node->left != nullptr) {
                temp = node->left;
            } else if (node->right != nullptr) {
                temp = node->right;
            }

            if (temp == nullptr) {
                temp = node;
                node = nullptr;
            } else {
                *node = *temp;
            }
            delete temp;
        } else {
            AVLNode<T>* temp = findMin(node->right);
            node->value = temp->value;
            node->data = temp->data;
            node->right = deleteHelper(node->right, temp->value);
        }
    }

    if (node == nullptr) return nullptr;

    updateHeight(node);

    int balance = getBalance(node);

    if (balance > 1 && getBalance(node->left) >= 0) {
        return rotateRight(node);
    }

    if (balance > 1 && getBalance(node->left) < 0) {
        node->left = rotateLeft(node->left);
        return rotateRight(node);
    }

    if (balance < -1 && getBalance(node->right) <= 0) {
        return rotateLeft(node);
    }

    if (balance < -1 && getBalance(node->right) > 0) {
        node->right = rotateRight(node->right);
        return rotateLeft(node);
    }

    return node;
}

    
    AVLNode<T>* searchHelper(AVLNode<T>* node, const string& value) {
        if (node == nullptr || node->value == value) {
            return node;
        }
        
        if (value < node->value) {
            return searchHelper(node->left, value);
        }
        return searchHelper(node->right, value);
    }
    
    void inOrderHelper(AVLNode<T>* node, vector<T>& result) {
        if (node != nullptr) {
            inOrderHelper(node->left, result);
            result.push_back(node->data);
            inOrderHelper(node->right, result);
        }
    }
    
    void cleanup(AVLNode<T>* node) {
        if (node != nullptr) {
            cleanup(node->left);
            cleanup(node->right);
            delete node;
        }
    }
    
public:
    AVLTree() : root(nullptr) {}
    
    ~AVLTree() {
        cleanup(root);
    }
    
    void insert(const string& value, const T& data) {
        root = insertHelper(root, value, data);
    }
    
    bool remove(const string& value) {
        if (search(value) == nullptr) return false;
        root = deleteHelper(root, value);
        return true;
    }
    
    T* search(const string& value) {
        AVLNode<T>* node = searchHelper(root, value);
        if (node == nullptr) return nullptr;
        return &(node->data);
    }
    
    vector<T> getAllSorted() {
        vector<T> result;
        inOrderHelper(root, result);
        return result;
    }
    
    bool isEmpty() {
        return root == nullptr;
    }

    int size() {
        vector<T> all = getAllSorted();
        return all.size();
    }
};

#endif 
