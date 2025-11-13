#include "../include/odf_types.hpp"
#include "../include/ofs_instance.h"
#include "../include/session_manager.h"
#include "../include/helper_functions.h"
#include <iostream>
#include <ctime>
#include <cstring>

using namespace std;


// checks in
extern "C" int user_login(void** session, const char* username, const char* password) {
    OFSInstance* fs = SessionManager::getInstance();
    if (!fs) {
        return static_cast<int>(OFSErrorCodes::ERROR_INVALID_OPERATION);
    }
    
    // seaarch for user in instance of file system
    UserInfo* user = fs->users.search(username);
    if (!user) {
        return static_cast<int>(OFSErrorCodes::ERROR_NOT_FOUND);
    }
    
    // checks the password
    string hashed_input = simple_hash(password);
    if (hashed_input != user->password_hash) {
        return static_cast<int>(OFSErrorCodes::ERROR_PERMISSION_DENIED);
    }
    
    // last login is changed to now
    user->last_login = time(nullptr);
    
    // creates seperate session for each user
    string session_id = SessionManager::createSession(*user, fs);
    *session = (void*)new string(session_id);
    
    return static_cast<int>(OFSErrorCodes::SUCCESS);
}

// logs out the user from system, still in users table but session ended
extern "C" int user_logout(void* session) {
    string* session_str = (string*)session;
    
    if (SessionManager::removeSession(*session_str)) {
        delete session_str;
        return static_cast<int>(OFSErrorCodes::SUCCESS);
    }
    
    return static_cast<int>(OFSErrorCodes::ERROR_INVALID_SESSION);
}

// gets session info 
extern "C" int get_session_info(void* session, SessionInfo* info) {
    string* session_str = (string*)session;
    ManagedSession* ms = SessionManager::getSession(*session_str);
    
    if (!ms) {
        return static_cast<int>(OFSErrorCodes::ERROR_INVALID_SESSION);
    }
    
    *info = ms->info;
    return static_cast<int>(OFSErrorCodes::SUCCESS);
}

// creates a new user (only by admin)
extern "C" int user_create(void* admin_session, const char* username, const char* password, UserRole role) {
    string* session_str = (string*)admin_session;
    ManagedSession* ms = SessionManager::getSession(*session_str);
    
    if (!ms || !ms->instance) {
        return static_cast<int>(OFSErrorCodes::ERROR_INVALID_SESSION);
    }
    
    // checks is admin
    if (ms->info.user.role != UserRole::ADMIN) {
        return static_cast<int>(OFSErrorCodes::ERROR_PERMISSION_DENIED);
    }
    

    OFSInstance* fs = ms->instance;
    
    // checks for duplicate username
    if (fs->users.search(username) != nullptr) {
        return static_cast<int>(OFSErrorCodes::ERROR_FILE_EXISTS);
    }
    
    // creates new user
    UserInfo new_user(username, simple_hash(password), role, time(nullptr));
    fs->users.insert(username, new_user);
    
    fseek(fs->omni_file, fs->header.user_table_offset, SEEK_SET);
    
    for (uint32_t i = 0; i < fs->header.max_users; i++) {
        UserInfo existing;
        uint64_t slot_offset = fs->header.user_table_offset + (i * sizeof(UserInfo));
        
        fseek(fs->omni_file, slot_offset, SEEK_SET);
        fread(&existing, sizeof(UserInfo), 1, fs->omni_file);
        
        if (!existing.is_active || existing.username[0] == '\0') {
            fseek(fs->omni_file, slot_offset, SEEK_SET);
            fwrite(&new_user, sizeof(UserInfo), 1, fs->omni_file);
            fflush(fs->omni_file);
            
            cout << "User created: " << username << endl;
            return static_cast<int>(OFSErrorCodes::SUCCESS);
        }
    }
    
    fs->users.remove(username);
    return static_cast<int>(OFSErrorCodes::ERROR_NO_SPACE);
}

// deletes the user
extern "C" int user_delete(void* admin_session, const char* username) {
    string* session_str = (string*)admin_session;
    ManagedSession* ms = SessionManager::getSession(*session_str);
    
    if (!ms || !ms->instance) {
        return static_cast<int>(OFSErrorCodes::ERROR_INVALID_SESSION);
    }
    // checks fr admin permissions
    if (ms->info.user.role != UserRole::ADMIN) {
        return static_cast<int>(OFSErrorCodes::ERROR_PERMISSION_DENIED);
    }
    
    OFSInstance* fs = ms->instance;
    
    // cannot delete itself
    if (strcmp(username, ms->info.user.username) == 0) {
        return static_cast<int>(OFSErrorCodes::ERROR_INVALID_OPERATION);
    }
    
    if (!fs->users.remove(username)) {
        return static_cast<int>(OFSErrorCodes::ERROR_NOT_FOUND);
    }
    
    fseek(fs->omni_file, fs->header.user_table_offset, SEEK_SET);
    
    for (uint32_t i = 0; i < fs->header.max_users; i++) {
        UserInfo existing;
        uint64_t slot_offset = fs->header.user_table_offset + (i * sizeof(UserInfo));
        
        fseek(fs->omni_file, slot_offset, SEEK_SET);
        fread(&existing, sizeof(UserInfo), 1, fs->omni_file);
        
        if (strcmp(existing.username, username) == 0) {
            existing.is_active = 0;
            fseek(fs->omni_file, slot_offset, SEEK_SET);
            fwrite(&existing, sizeof(UserInfo), 1, fs->omni_file);
            fflush(fs->omni_file);
            
            cout << "User deleted: " << username << endl;
            return static_cast<int>(OFSErrorCodes::SUCCESS);
        }
    }
    
    return static_cast<int>(OFSErrorCodes::SUCCESS);
}

// list all the valid users
extern "C" int user_list(void* admin_session, UserInfo** users, int* count) {
    string* session_str = (string*)admin_session;
    ManagedSession* ms = SessionManager::getSession(*session_str);
    
    if (!ms || !ms->instance) {
        return static_cast<int>(OFSErrorCodes::ERROR_INVALID_SESSION);
    }
    
    if (ms->info.user.role != UserRole::ADMIN) {
        return static_cast<int>(OFSErrorCodes::ERROR_PERMISSION_DENIED);
    }
    
    OFSInstance* fs = ms->instance;
    vector<UserInfo> all_users = fs->users.getAllSorted();
    *count = all_users.size();
    
    *users = new UserInfo[all_users.size()];
    for (size_t i = 0; i < all_users.size(); i++) {
        (*users)[i] = all_users[i];
    }
    
    return static_cast<int>(OFSErrorCodes::SUCCESS);
}