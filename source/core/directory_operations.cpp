#include "../include/odf_types.hpp"
#include "../include/ofs_instance.h"
#include "../include/session_manager.h"
#include "../include/helper_functions.h"
#include <iostream>
#include <cstring>
#include <ctime>

using namespace std;

// create a new directory
extern "C" int dir_create(void* session, const char* path) {
    string* session_str = (string*)session;
    ManagedSession* ms = SessionManager::getSession(*session_str);
    
    if (!ms || !ms->instance) {
        return static_cast<int>(OFSErrorCodes::ERROR_INVALID_SESSION);
    }
    
    OFSInstance* fs = ms->instance;

    // check for valid path
    if (!path || path[0] != '/') {
        return static_cast<int>(OFSErrorCodes::ERROR_INVALID_PATH);
    }
    
    // check if already exist
    if (fs->file_tree->exists(path)) {
        return static_cast<int>(OFSErrorCodes::ERROR_FILE_EXISTS);
    }
    
    // get parent index for this directory
    uint32_t parent_idx = getParentIndexFromPath(fs, string(path));
    if (parent_idx == 0 && string(path) != "/") {
        // parent doesn't exist
        return static_cast<int>(OFSErrorCodes::ERROR_INVALID_PATH);
    }
    
    TreeNode* node = fs->file_tree->createNode(path, false, ms->info.user.username);
    if (!node) {
        return static_cast<int>(OFSErrorCodes::ERROR_INVALID_PATH);
    }
    
    // find next free index
    uint32_t free_index = findFreeEntryIndex(fs);
    if (free_index == 0) {
        fs->file_tree->deleteNode(path);
        return static_cast<int>(OFSErrorCodes::ERROR_NO_SPACE);
    }
    
    node->entryIndex = free_index;
    node->permissions = 0755;
    node->created_time = time(nullptr);
    node->modified_time = node->created_time;
    
    fs->total_directories++;
    
    // write fileEntry to disk with parent_index
    string filename = extractFilename(string(path));
    FileEntry dir_entry(filename, EntryType::DIRECTORY, 0, node->permissions, 
                       node->owner, node->entryIndex, parent_idx);
    
    dir_entry.created_time = node->created_time;
    dir_entry.modified_time = node->modified_time;
    dir_entry.markValid();
    
    uint64_t file_entry_offset = fs->header.user_table_offset + 
                                (fs->header.max_users * sizeof(UserInfo)) +
                                (node->entryIndex * sizeof(FileEntry));
    
    fseek(fs->omni_file, file_entry_offset, SEEK_SET);
    fwrite(&dir_entry, sizeof(FileEntry), 1, fs->omni_file);
    fflush(fs->omni_file);
    
    return static_cast<int>(OFSErrorCodes::SUCCESS);
}

// list all directories at path
extern "C" int dir_list(void* session, const char* path, FileEntry** entries, int* count) {
    string* session_str = (string*)session;
    ManagedSession* ms = SessionManager::getSession(*session_str);
    
    if (!ms || !ms->instance) {
        return static_cast<int>(OFSErrorCodes::ERROR_INVALID_SESSION);
    }
    
    OFSInstance* fs = ms->instance;
    
    // is valid directory
    if (!fs->file_tree->isDirectory(path)) {
        return static_cast<int>(OFSErrorCodes::ERROR_NOT_FOUND);
    }
    
    vector<FileEntry> dir_entries = fs->file_tree->listDirectory(path);
    *count = dir_entries.size();
    
    if (*count > 0) {
        *entries = new FileEntry[*count];
        for (int i = 0; i < *count; i++) {
            (*entries)[i] = dir_entries[i];
        }
    } else {
        *entries = nullptr;
    }
    
    return static_cast<int>(OFSErrorCodes::SUCCESS);
}

// delete directory 
extern "C" int dir_delete(void* session, const char* path) {
    string* session_str = (string*)session;
    ManagedSession* ms = SessionManager::getSession(*session_str);
    
    if (!ms || !ms->instance) {
        return static_cast<int>(OFSErrorCodes::ERROR_INVALID_SESSION);
    }
    
    OFSInstance* fs = ms->instance;
    
    // cannot delete root 
    if (strcmp(path, "/") == 0) {
        return static_cast<int>(OFSErrorCodes::ERROR_INVALID_OPERATION);
    }
    
    // find node
    TreeNode* node = fs->file_tree->findNode(path);
    if (!node) {
        return static_cast<int>(OFSErrorCodes::ERROR_NOT_FOUND);
    }
    
    // invalid operation to delete file (can only delete directories)
    if (node->isFile) {
        return static_cast<int>(OFSErrorCodes::ERROR_INVALID_OPERATION);
    }
    
    // can not delete non empty directories
    if (!node->children.empty()) {
        return static_cast<int>(OFSErrorCodes::ERROR_DIRECTORY_NOT_EMPTY);
    }
    
    // owner/admin to delete directory
    if (strcmp(node->owner.c_str(), ms->info.user.username) != 0 && 
        ms->info.user.role != UserRole::ADMIN) {
        return static_cast<int>(OFSErrorCodes::ERROR_PERMISSION_DENIED);
    }
    
    uint64_t file_entry_offset = fs->header.user_table_offset + 
                                (fs->header.max_users * sizeof(UserInfo)) +
                                (node->entryIndex * sizeof(FileEntry));
    
    FileEntry entry;
    fseek(fs->omni_file, file_entry_offset, SEEK_SET);
    fread(&entry, sizeof(FileEntry), 1, fs->omni_file);
    
    // mark validity to invalid
    entry.markInvalid();
    
    fseek(fs->omni_file, file_entry_offset, SEEK_SET);
    fwrite(&entry, sizeof(FileEntry), 1, fs->omni_file);
    fflush(fs->omni_file);
    
    if (fs->file_tree->deleteNode(path)) {
        fs->total_directories--;
        return static_cast<int>(OFSErrorCodes::SUCCESS);
    }
    
    return static_cast<int>(OFSErrorCodes::ERROR_IO_ERROR);
}

// check if directory exist
extern "C" int dir_exists(void* session, const char* path) {
    string* session_str = (string*)session;
    ManagedSession* ms = SessionManager::getSession(*session_str);
    
    if (!ms || !ms->instance) {
        return static_cast<int>(OFSErrorCodes::ERROR_INVALID_SESSION);
    }
    
    OFSInstance* fs = ms->instance;
    
    if (fs->file_tree->isDirectory(path)) {
        return static_cast<int>(OFSErrorCodes::SUCCESS);
    }
    
    return static_cast<int>(OFSErrorCodes::ERROR_NOT_FOUND);
}