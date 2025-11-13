#ifndef FILE_TREE_H
#define FILE_TREE_H

#include <string>
#include <vector>
#include <cstring>
#include <ctime>
#include "../include/odf_types.hpp"

using namespace std;

struct TreeNode {
    string name;
    bool isFile;
    uint32_t entryIndex;
    uint32_t startBlockIndex;
    uint64_t size;
    uint32_t permissions;
    string owner;
    uint64_t created_time;
    uint64_t modified_time;
    
    TreeNode* parent; 
    vector<TreeNode*> children;
    
    TreeNode(const string& n, bool file = false) 
        : name(n), isFile(file), entryIndex(0), startBlockIndex(0), 
          size(0), permissions(0644), owner(""), created_time(0), modified_time(0), parent(nullptr) {}
    
    ~TreeNode() {
        for (size_t i = 0; i < children.size(); i++) {
            delete children[i];
        }
    }
    
    TreeNode* findChild(const string& childName) {
        for (size_t i = 0; i < children.size(); i++) {
            if (children[i]->name == childName) {
                return children[i];
            }
        }
        return nullptr;
    }
    
    void addChild(TreeNode* child) {
        child->parent = this;
        children.push_back(child);
    }
    
    bool removeChild(const string& childName) {
        for (size_t i = 0; i < children.size(); i++) {
            if (children[i]->name == childName) {
                delete children[i];
                children.erase(children.begin() + i);
                return true;
            }
        }
        return false;
    }
    
    bool removeChild(TreeNode* child) {
        for (size_t i = 0; i < children.size(); i++) {
            if (children[i] == child) {
                children.erase(children.begin() + i);
                return true;
            }
        }
        return false;
    }
    
    string getFullPath() {
        if (parent == nullptr) return "/";
        
        vector<string> pathParts;
        TreeNode* current = this;
        
        while (current->parent != nullptr) {
            pathParts.push_back(current->name);
            current = current->parent;
        }
        
        string path = "";
        for (int i = pathParts.size() - 1; i >= 0; i--) {
            path += "/" + pathParts[i];
        }
        
        return path.empty() ? "/" : path;
    }
};


class FileTree {
private:
    TreeNode* root;
    
    vector<string> splitPath(const string& path) {
        vector<string> parts;
        
        if (path.empty() || path == "/") {
            return parts;
        }
        
        size_t start = 0;
        size_t end = path.find('/');
        
        if (end == 0) {
            start = 1;
            end = path.find('/', start);
        }
        
        while (end != string::npos) {
            if (end != start) {
                parts.push_back(path.substr(start, end - start));
            }
            start = end + 1;
            end = path.find('/', start);
        }
        
        if (start < path.length()) {
            parts.push_back(path.substr(start));
        }
        
        return parts;
    }
    
public:
    FileTree() {
        root = new TreeNode("/", false);
        root->entryIndex = 1;
        root->owner = "admin";
        root->permissions = 0755;
        root->created_time = time(nullptr);
        root->modified_time = root->created_time;
    }
    
    ~FileTree() {
        delete root;
    }
    
    TreeNode* getRoot() {
        return root;
    }
    
    TreeNode* findNode(const string& path) {
        if (path == "/" || path.empty()) {
            return root;
        }
        
        vector<string> parts = splitPath(path);
        TreeNode* current = root;
        
        for (size_t i = 0; i < parts.size(); i++) {
            if (parts[i].empty()) continue;
            current = current->findChild(parts[i]);
            if (current == nullptr) return nullptr;
        }
        
        return current;
    }
    
    TreeNode* createNode(const string& path, bool isFile, const string& owner) {
        if (path == "/") return nullptr; 
        
        size_t lastSlash = path.find_last_of('/');
        string parentPath;
        string name;
        
        if (lastSlash == 0) {
            parentPath = "/";
            name = path.substr(1);
        } else {
            parentPath = path.substr(0, lastSlash);
            name = path.substr(lastSlash + 1);
        }
        
        if (name.empty() || name.find('/') != string::npos) {
            return nullptr;
        }
        
        TreeNode* parent = findNode(parentPath);
        if (parent == nullptr) return nullptr;
        if (parent->isFile) return nullptr;
        
        if (parent->findChild(name)) return nullptr;
        
        TreeNode* newNode = new TreeNode(name, isFile);
        newNode->owner = owner;
        newNode->created_time = time(nullptr);
        newNode->modified_time = newNode->created_time;
        
        if (isFile) {
            newNode->permissions = 0644;
        } else {
            newNode->permissions = 0755;
        }
        
        parent->addChild(newNode);
        parent->modified_time = time(nullptr);
        
        return newNode;
    }
    
    bool deleteNode(const string& path) {
        if (path == "/") return false; 
        
        TreeNode* node = findNode(path);
        if (node == nullptr) return false;
        
        if (!node->isFile && !node->children.empty()) {
            return false;
        }
        
        if (node->parent) {
            bool removed = node->parent->removeChild(node);
            if (removed) {
                delete node;
                return true;
            }
        }
        
        return false;
    }
    
    vector<FileEntry> listDirectory(const string& path) {
        vector<FileEntry> entries;
        
        TreeNode* dir = findNode(path);
        if (dir == nullptr || dir->isFile) return entries;
        
        for (size_t i = 0; i < dir->children.size(); i++) {
            TreeNode* child = dir->children[i];
            FileEntry entry;
            
            memset(&entry, 0, sizeof(FileEntry));
            
            strncpy(entry.name, child->name.c_str(), sizeof(entry.name) - 1);
            entry.name[sizeof(entry.name) - 1] = '\0';
            
            if (child->isFile) {
                entry.setType(EntryType::FILE);
            } else {
                entry.setType(EntryType::DIRECTORY);
            }
            
            entry.size = child->size;
            entry.permissions = child->permissions;
            entry.inode = child->entryIndex;
            entry.created_time = child->created_time;
            entry.modified_time = child->modified_time;
            
            strncpy(entry.owner, child->owner.c_str(), sizeof(entry.owner) - 1);
            entry.owner[sizeof(entry.owner) - 1] = '\0';
            
            entries.push_back(entry);
        }
        
        return entries;
    }
    
    bool exists(const string& path) {
        return findNode(path) != nullptr;
    }
    
    bool isFile(const string& path) {
        TreeNode* node = findNode(path);
        return node != nullptr && node->isFile;
    }
    
    bool isDirectory(const string& path) {
        TreeNode* node = findNode(path);
        return node != nullptr && !node->isFile;
    }
    
    bool rename(const string& oldPath, const string& newPath) {
        TreeNode* oldNode = findNode(oldPath);
        if (oldNode == nullptr || oldPath == "/") return false;
        
        size_t lastSlash = newPath.find_last_of('/');
        string newParentPath;
        string newName;
        
        if (lastSlash == 0) {
            newParentPath = "/";
            newName = newPath.substr(1);
        } else {
            newParentPath = newPath.substr(0, lastSlash);
            newName = newPath.substr(lastSlash + 1);
        }
        
        if (newName.empty() || newName.find('/') != string::npos) {
            return false;
        }
        
        TreeNode* newParent = findNode(newParentPath);
        if (newParent == nullptr || newParent->isFile) return false;
        
        if (newParent->findChild(newName)) return false;
        
        TreeNode* oldParent = oldNode->parent;
        if (oldParent) {
            oldParent->removeChild(oldNode);
            oldParent->modified_time = time(nullptr);
        }
        
        oldNode->name = newName;
        newParent->addChild(oldNode);
        newParent->modified_time = time(nullptr);
        oldNode->modified_time = time(nullptr);
        
        return true;
    }
    
    void getStats(uint32_t& fileCount, uint32_t& dirCount) {
        fileCount = 0;
        dirCount = 0;
        countNodes(root, fileCount, dirCount);
    }
    
private:
    void countNodes(TreeNode* node, uint32_t& fileCount, uint32_t& dirCount) {
        if (node == nullptr) return;
        
        if (node->isFile) {
            fileCount++;
        } else {
            dirCount++;
            for (size_t i = 0; i < node->children.size(); i++) {
                countNodes(node->children[i], fileCount, dirCount);
            }
        }
    }
};

#endif