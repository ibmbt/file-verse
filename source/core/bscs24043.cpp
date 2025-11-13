#include "../include/odf_types.hpp"
#include <iostream>
#include <string>
#include <cstring>

using namespace std;

extern "C" {
    int fs_init(void** instance, const char* omni_path, const char* config_path);
    int fs_shutdown(void* instance);
    int fs_format(const char* omni_path, const char* config_path);
    
    int user_login(void** session, const char* username, const char* password);
    int user_logout(void* session);
    int user_create(void* admin_session, const char* username, const char* password, UserRole role);
    int user_delete(void* admin_session, const char* username);
    int user_list(void* admin_session, UserInfo** users, int* count);
    int get_session_info(void* session, SessionInfo* info);
    
    int file_create(void* session, const char* path, const char* data, size_t size);
    int file_read(void* session, const char* path, char** buffer, size_t* size);
    int file_delete(void* session, const char* path);
    int file_exists(void* session, const char* path);
    int file_rename(void* session, const char* old_path, const char* new_path);
    int file_edit(void* session, const char* path, const char* data, size_t size, uint32_t index);
    int file_truncate(void* session, const char* path);
    
    int dir_create(void* session, const char* path);
    int dir_list(void* session, const char* path, FileEntry** entries, int* count);
    int dir_delete(void* session, const char* path);
    int dir_exists(void* session, const char* path);
    
    int get_metadata(void* session, const char* path, FileMetadata* meta);
    int set_permissions(void* session, const char* path, uint32_t permissions);
    int get_stats(void* session, FSStats* stats);
    
    void free_buffer(void* buffer);
    const char* get_error_message(int error_code);
}

// Global state
void* fs_instance = nullptr;
void* current_session = nullptr;
string current_username = "";
string omni_file_path = "";

// Utility functions
void clearInputBuffer() {
    cin.clear();
    // Using a large fixed value instead of numeric_limits<streamsize>::max()
    cin.ignore(10000, '\n');
}

void pressEnterToContinue() {
    cout << "\nPress Enter to continue...";
    cin.ignore(10000, '\n');
}

void printError(int error_code) {
    cout << "ERROR: " << get_error_message(error_code) << " (code: " << error_code << ")" << endl;
}

bool isValidPath(const string& path) {
    if (path.empty() || path[0] != '/') {
        cout << "ERROR: Path must start with /" << endl;
        return false;
    }
    return true;
}

// Menu functions
void initializeFileSystem() {
    cout << "\n--- Initialize File System ---" << endl;
    
    if (fs_instance != nullptr) {
        cout << "File system already initialized!" << endl;
        return;
    }
    
    cout << "Enter .omni file path: ";
    getline(cin, omni_file_path);
    
    // Validate extension
    if (omni_file_path.length() < 5 || omni_file_path.substr(omni_file_path.length() - 5) != ".omni") {
        cout << "ERROR: Invalid file extension. Must be .omni" << endl;
        omni_file_path = "";
        return;
    }
    
    int result = fs_init(&fs_instance, omni_file_path.c_str(), "./compiled/default.uconf");
    if (result != static_cast<int>(OFSErrorCodes::SUCCESS)) {
        printError(result);
        omni_file_path = "";
    }
}

void formatFileSystem() {
    cout << "\n--- Format File System ---" << endl;
    
    if (fs_instance != nullptr) {
        cout << "ERROR: File system is currently initialized. Shutdown first." << endl;
        return;
    }
    
    string path;
    cout << "Enter .omni file path to format: ";
    getline(cin, path);
    
    // Validate extension
    if (path.length() < 5 || path.substr(path.length() - 5) != ".omni") {
        cout << "ERROR: Invalid file extension. Must be .omni" << endl;
        return;
    }
    
    int result = fs_format(path.c_str(), nullptr);
    if (result != static_cast<int>(OFSErrorCodes::SUCCESS)) {
        printError(result);
    }
}

void shutdownFileSystem() {
    cout << "\n--- Shutdown File System ---" << endl;
    
    if (fs_instance == nullptr) {
        cout << "No file system initialized" << endl;
        return;
    }
    
    if (current_session != nullptr) {
        cout << "Logging out current user..." << endl;
        user_logout(current_session);
        current_session = nullptr;
        current_username = "";
    }
    
    int result = fs_shutdown(fs_instance);
    if (result == static_cast<int>(OFSErrorCodes::SUCCESS)) {
        fs_instance = nullptr;
        omni_file_path = "";
    } else {
        printError(result);
    }
}

void loginUser() {
    cout << "\n--- User Login ---" << endl;
    
    if (fs_instance == nullptr) {
        cout << "ERROR: File system not initialized" << endl;
        return;
    }
    
    if (current_session != nullptr) {
        cout << "Already logged in as: " << current_username << endl;
        return;
    }
    
    string username, password;
    cout << "Username: ";
    getline(cin, username);
    cout << "Password: ";
    getline(cin, password);
    
    int result = user_login(&current_session, username.c_str(), password.c_str());
    if (result == static_cast<int>(OFSErrorCodes::SUCCESS)) {
        current_username = username;
        cout << "Login successful! Welcome, " << username << endl;
    } else {
        printError(result);
    }
}

void logoutUser() {
    cout << "\n--- User Logout ---" << endl;
    
    if (current_session == nullptr) {
        cout << "Not logged in" << endl;
        return;
    }
    
    int result = user_logout(current_session);
    if (result == static_cast<int>(OFSErrorCodes::SUCCESS)) {
        cout << "Logged out successfully. Goodbye, " << current_username << "!" << endl;
        current_session = nullptr;
        current_username = "";
    } else {
        printError(result);
    }
}

void createUser() {
    cout << "\n--- Create User ---" << endl;
    
    if (current_session == nullptr) {
        cout << "ERROR: Must be logged in" << endl;
        return;
    }
    
    string username, password;
    int role_choice;
    
    cout << "New username: ";
    getline(cin, username);
    cout << "Password: ";
    getline(cin, password);
    cout << "Role (0=Normal, 1=Admin): ";
    cin >> role_choice;
    clearInputBuffer();
    
    UserRole role = (role_choice == 1) ? UserRole::ADMIN : UserRole::NORMAL;
    
    int result = user_create(current_session, username.c_str(), password.c_str(), role);
    if (result == static_cast<int>(OFSErrorCodes::SUCCESS)) {
        cout << "User created successfully" << endl;
    } else {
        printError(result);
    }
}

void deleteUser() {
    cout << "\n--- Delete User ---" << endl;
    
    if (current_session == nullptr) {
        cout << "ERROR: Must be logged in" << endl;
        return;
    }
    
    string username;
    cout << "Username to delete: ";
    getline(cin, username);
    
    int result = user_delete(current_session, username.c_str());
    if (result == static_cast<int>(OFSErrorCodes::SUCCESS)) {
        cout << "User deleted successfully" << endl;
    } else {
        printError(result);
    }
}

void listUsers() {
    cout << "\n--- List Users ---" << endl;
    
    if (current_session == nullptr) {
        cout << "ERROR: Must be logged in" << endl;
        return;
    }
    
    UserInfo* users;
    int count;
    
    int result = user_list(current_session, &users, &count);
    if (result == static_cast<int>(OFSErrorCodes::SUCCESS)) {
        cout << "\nTotal users: " << count << endl;
        for (int i = 0; i < count; i++) {
            cout << "  " << users[i].username 
                 << " (" << (users[i].role == UserRole::ADMIN ? "Admin" : "Normal") << ")" << endl;
        }
        delete[] users;
    } else {
        printError(result);
    }
}

void showSessionInfo() {
    cout << "\n--- Session Information ---" << endl;
    
    if (current_session == nullptr) {
        cout << "Not logged in" << endl;
        return;
    }
    
    SessionInfo info;
    int result = get_session_info(current_session, &info);
    if (result == static_cast<int>(OFSErrorCodes::SUCCESS)) {
        cout << "Session ID: " << info.session_id << endl;
        cout << "Username: " << info.user.username << endl;
        cout << "Role: " << (info.user.role == UserRole::ADMIN ? "Admin" : "Normal") << endl;
        cout << "Login time: " << info.login_time << endl;
        cout << "Last activity: " << info.last_activity << endl;
        cout << "Operations count: " << info.operations_count << endl;
    } else {
        printError(result);
    }
}

void createFile() {
    cout << "\n--- Create File ---" << endl;
    
    if (current_session == nullptr) {
        cout << "ERROR: Must be logged in" << endl;
        return;
    }
    
    string path, content;
    cout << "File path: ";
    getline(cin, path);
    
    if (!isValidPath(path)) return;
    
    cout << "Content (or press Enter for empty file): ";
    getline(cin, content);
    
    int result = file_create(current_session, path.c_str(), content.c_str(), content.length());
    if (result == static_cast<int>(OFSErrorCodes::SUCCESS)) {
        cout << "File created successfully" << endl;
    } else {
        printError(result);
    }
}

void readFile() {
    cout << "\n--- Read File ---" << endl;
    
    if (current_session == nullptr) {
        cout << "ERROR: Must be logged in" << endl;
        return;
    }
    
    string path;
    cout << "File path: ";
    getline(cin, path);
    
    if (!isValidPath(path)) return;
    
    char* buffer;
    size_t size;
    
    int result = file_read(current_session, path.c_str(), &buffer, &size);
    if (result == static_cast<int>(OFSErrorCodes::SUCCESS)) {
        cout << "\nFile content (" << size << " bytes):" << endl;
        cout << string(buffer, size) << endl;
        free_buffer(buffer);
    } else {
        printError(result);
    }
}

void deleteFile() {
    cout << "\n--- Delete File ---" << endl;
    
    if (current_session == nullptr) {
        cout << "ERROR: Must be logged in" << endl;
        return;
    }
    
    string path;
    cout << "File path: ";
    getline(cin, path);
    
    if (!isValidPath(path)) return;
    
    int result = file_delete(current_session, path.c_str());
    if (result == static_cast<int>(OFSErrorCodes::SUCCESS)) {
        cout << "File deleted successfully" << endl;
    } else {
        printError(result);
    }
}

void renameFile() {
    cout << "\n--- Rename File ---" << endl;
    
    if (current_session == nullptr) {
        cout << "ERROR: Must be logged in" << endl;
        return;
    }
    
    string old_path, new_path;
    cout << "Old path: ";
    getline(cin, old_path);
    cout << "New path: ";
    getline(cin, new_path);
    
    if (!isValidPath(old_path) || !isValidPath(new_path)) return;
    
    int result = file_rename(current_session, old_path.c_str(), new_path.c_str());
    if (result == static_cast<int>(OFSErrorCodes::SUCCESS)) {
        cout << "File renamed successfully" << endl;
    } else {
        printError(result);
    }
}

void editFile() {
    cout << "\n--- Edit File ---" << endl;
    
    if (current_session == nullptr) {
        cout << "ERROR: Must be logged in" << endl;
        return;
    }
    
    string path, new_content;
    uint32_t index;
    
    cout << "File path: ";
    getline(cin, path);
    
    if (!isValidPath(path)) return;
    
    cout << "Start index (byte position): ";
    cin >> index;
    clearInputBuffer();
    
    cout << "New content: ";
    getline(cin, new_content);
    
    int result = file_edit(current_session, path.c_str(), new_content.c_str(), new_content.length(), index);
    if (result == static_cast<int>(OFSErrorCodes::SUCCESS)) {
        cout << "File edited successfully" << endl;
    } else {
        printError(result);
    }
}

void truncateFile() {
    cout << "\n--- Truncate File ---" << endl;
    
    if (current_session == nullptr) {
        cout << "ERROR: Must be logged in" << endl;
        return;
    }
    
    string path;
    cout << "File path: ";
    getline(cin, path);
    
    if (!isValidPath(path)) return;
    
    cout << "WARNING: This will overwrite the file with 'siruamr' pattern" << endl;
    cout << "Continue? (y/n): ";
    char confirm;
    cin >> confirm;
    clearInputBuffer();
    
    if (confirm != 'y' && confirm != 'Y') {
        cout << "Cancelled" << endl;
        return;
    }
    
    int result = file_truncate(current_session, path.c_str());
    if (result == static_cast<int>(OFSErrorCodes::SUCCESS)) {
        cout << "File truncated successfully" << endl;
    } else {
        printError(result);
    }
}

void createDirectory() {
    cout << "\n--- Create Directory ---" << endl;
    
    if (current_session == nullptr) {
        cout << "ERROR: Must be logged in" << endl;
        return;
    }
    
    string path;
    cout << "Directory path: ";
    getline(cin, path);
    
    if (!isValidPath(path)) return;
    
    int result = dir_create(current_session, path.c_str());
    if (result == static_cast<int>(OFSErrorCodes::SUCCESS)) {
        cout << "Directory created successfully" << endl;
    } else {
        printError(result);
    }
}

void listDirectory() {
    cout << "\n--- List Directory ---" << endl;
    
    if (current_session == nullptr) {
        cout << "ERROR: Must be logged in" << endl;
        return;
    }
    
    string path;
    cout << "Directory path (/ for root): ";
    getline(cin, path);
    
    if (path.empty()) path = "/";
    if (!isValidPath(path)) return;
    
    FileEntry* entries;
    int count;
    
    int result = dir_list(current_session, path.c_str(), &entries, &count);
    if (result == static_cast<int>(OFSErrorCodes::SUCCESS)) {
        cout << "\nContents of " << path << " (" << count << " items):" << endl;
        if (count == 0) {
            cout << "  (empty)" << endl;
        } else {
            for (int i = 0; i < count; i++) {
                if (entries[i].getType() == EntryType::DIRECTORY) {
                    cout << "  -> " << entries[i].name << endl;
                } else {
                    cout << "  - " << entries[i].name << endl;
                }
            }
        }
        delete[] entries;
    } else {
        printError(result);
    }
}

void deleteDirectory() {
    cout << "\n--- Delete Directory ---" << endl;
    
    if (current_session == nullptr) {
        cout << "ERROR: Must be logged in" << endl;
        return;
    }
    
    string path;
    cout << "Directory path: ";
    getline(cin, path);
    
    if (!isValidPath(path)) return;
    
    int result = dir_delete(current_session, path.c_str());
    if (result == static_cast<int>(OFSErrorCodes::SUCCESS)) {
        cout << "Directory deleted successfully" << endl;
    } else {
        printError(result);
    }
}

void getMetadata() {
    cout << "\n--- Get Metadata ---" << endl;
    
    if (current_session == nullptr) {
        cout << "ERROR: Must be logged in" << endl;
        return;
    }
    
    string path;
    cout << "Path: ";
    getline(cin, path);
    
    if (!isValidPath(path)) return;
    
    FileMetadata meta;
    int result = get_metadata(current_session, path.c_str(), &meta);
    if (result == static_cast<int>(OFSErrorCodes::SUCCESS)) {
        cout << "\nMetadata for: " << path << endl;
        cout << "  Name: " << meta.entry.name << endl;
        cout << "  Type: " << (meta.entry.getType() == EntryType::FILE ? "File" : "Directory") << endl;
        cout << "  Size: " << meta.entry.size << " bytes" << endl;
        cout << "  Permissions: 0" << oct << meta.entry.permissions << dec << endl;
        cout << "  Owner: " << meta.entry.owner << endl;
        cout << "  Created: " << meta.entry.created_time << endl;
        cout << "  Modified: " << meta.entry.modified_time << endl;
        cout << "  Blocks used: " << meta.blocks_used << endl;
        cout << "  Actual size: " << meta.actual_size << " bytes" << endl;
    } else {
        printError(result);
    }
}

void setPermissions() {
    cout << "\n--- Set Permissions ---" << endl;
    
    if (current_session == nullptr) {
        cout << "ERROR: Must be logged in" << endl;
        return;
    }
    
    string path;
    uint32_t perms;
    
    cout << "Path: ";
    getline(cin, path);
    
    if (!isValidPath(path)) return;
    
    cout << "Permissions (octal, e.g., 644): ";
    cin >> oct >> perms >> dec;
    clearInputBuffer();
    
    int result = set_permissions(current_session, path.c_str(), perms);
    if (result == static_cast<int>(OFSErrorCodes::SUCCESS)) {
        cout << "Permissions set successfully" << endl;
    } else {
        printError(result);
    }
}

void showStats() {
    cout << "\n--- File System Statistics ---" << endl;
    
    if (current_session == nullptr) {
        cout << "ERROR: Must be logged in" << endl;
        return;
    }
    
    FSStats stats;
    int result = get_stats(current_session, &stats);
    if (result == static_cast<int>(OFSErrorCodes::SUCCESS)) {
        cout << "Total size: " << stats.total_size << " bytes (" << (stats.total_size / 1024 / 1024) << " MB)" << endl;
        cout << "Used space: " << stats.used_space << " bytes (" << (stats.used_space / 1024 / 1024) << " MB)" << endl;
        cout << "Free space: " << stats.free_space << " bytes (" << (stats.free_space / 1024 / 1024) << " MB)" << endl;
        cout << "Total files: " << stats.total_files << endl;
        cout << "Total directories: " << stats.total_directories << endl;
        cout << "Total users: " << stats.total_users << endl;
        cout << "Active sessions: " << stats.active_sessions << endl;
        cout << "Fragmentation: " << stats.fragmentation << "%" << endl;
    } else {
        printError(result);
    }
}

void showMainMenu() {
    cout << "\n";
    cout << "OFS File System Manager" << endl;
    cout << "File: " << (omni_file_path.empty() ? "(none)" : omni_file_path) << endl;
    cout << "User: " << (current_username.empty() ? "(not logged in)" : current_username) << endl;
    cout << "\nMain Menu:" << endl;
    cout << "1. System Operations" << endl;
    cout << "2. User Operations" << endl;
    cout << "3. File Operations" << endl;
    cout << "4. Directory Operations" << endl;
    cout << "5. Info Operations" << endl;
    cout << "0. Exit" << endl;
    cout << "\nChoice: ";
}

void systemOperationsMenu() {
    while (true) {
        cout << "\n";
        cout << "System Operations Menu:" << endl;
        cout << "1. Initialize File System" << endl;
        cout << "2. Format File System" << endl;
        cout << "3. Shutdown File System" << endl;
        cout << "0. Back to Main Menu" << endl;
        cout << "\nChoice: ";
        
        int choice;
        cin >> choice;
        clearInputBuffer();
        
        switch (choice) {
            case 1: initializeFileSystem(); pressEnterToContinue(); break;
            case 2: formatFileSystem(); pressEnterToContinue(); break;
            case 3: shutdownFileSystem(); pressEnterToContinue(); break;
            case 0: return;
            default: cout << "Invalid choice" << endl;
        }
    }
}

void userOperationsMenu() {
    while (true) {
        cout << "\n";
        cout << "User Operations Menu:" << endl;
        cout << "1. Login" << endl;
        cout << "2. Logout" << endl;
        cout << "3. Create User" << endl;
        cout << "4. Delete User" << endl;
        cout << "5. List Users" << endl;
        cout << "6. Show Session Info" << endl;
        cout << "0. Back to Main Menu" << endl;
        cout << "\nChoice: ";
        
        int choice;
        cin >> choice;
        clearInputBuffer();
        
        switch (choice) {
            case 1: loginUser(); pressEnterToContinue(); break;
            case 2: logoutUser(); pressEnterToContinue(); break;
            case 3: createUser(); pressEnterToContinue(); break;
            case 4: deleteUser(); pressEnterToContinue(); break;
            case 5: listUsers(); pressEnterToContinue(); break;
            case 6: showSessionInfo(); pressEnterToContinue(); break;
            case 0: return;
            default: cout << "Invalid choice" << endl;
        }
    }
}

void fileOperationsMenu() {
    while (true) {
        cout << "\n";
        cout << "File Operations Menu:" << endl;
        cout << "1. Create File" << endl;
        cout << "2. Read File" << endl;
        cout << "3. Delete File" << endl;
        cout << "4. Rename File" << endl;
        cout << "5. Edit File" << endl;
        cout << "6. Truncate File" << endl;
        cout << "0. Back to Main Menu" << endl;
        cout << "\nChoice: ";
        
        int choice;
        cin >> choice;
        clearInputBuffer();
        
        switch (choice) {
            case 1: createFile(); pressEnterToContinue(); break;
            case 2: readFile(); pressEnterToContinue(); break;
            case 3: deleteFile(); pressEnterToContinue(); break;
            case 4: renameFile(); pressEnterToContinue(); break;
            case 5: editFile(); pressEnterToContinue(); break;
            case 6: truncateFile(); pressEnterToContinue(); break;
            case 0: return;
            default: cout << "Invalid choice" << endl;
        }
    }
}

void directoryOperationsMenu() {
    while (true) {
        cout << "\n";
        cout << "Directory Operations Menu:" << endl;
        cout << "1. Create Directory" << endl;
        cout << "2. List Directory" << endl;
        cout << "3. Delete Directory" << endl;
        cout << "0. Back to Main Menu" << endl;
        cout << "\nChoice: ";
        
        int choice;
        cin >> choice;
        clearInputBuffer();
        
        switch (choice) {
            case 1: createDirectory(); pressEnterToContinue(); break;
            case 2: listDirectory(); pressEnterToContinue(); break;
            case 3: deleteDirectory(); pressEnterToContinue(); break;
            case 0: return;
            default: cout << "Invalid choice" << endl;
        }
    }
}

void infoOperationsMenu() {
    while (true) {
        cout << "\n";
        cout << "Info Operations Menu:" << endl;
        cout << "1. Get Metadata" << endl;
        cout << "2. Set Permissions" << endl;
        cout << "3. Show File System Stats" << endl;
        cout << "0. Back to Main Menu" << endl;
        cout << "\nChoice: ";
        
        int choice;
        cin >> choice;
        clearInputBuffer();
        
        switch (choice) {
            case 1: getMetadata(); pressEnterToContinue(); break;
            case 2: setPermissions(); pressEnterToContinue(); break;
            case 3: showStats(); pressEnterToContinue(); break;
            case 0: return;
            default: cout << "Invalid choice" << endl;
        }
    }
}

int main() {
    while (true) {
        showMainMenu();
        
        int choice;
        cin >> choice;
        clearInputBuffer();
        
        switch (choice) {
            case 1: systemOperationsMenu(); break;
            case 2: userOperationsMenu(); break;
            case 3: fileOperationsMenu(); break;
            case 4: directoryOperationsMenu(); break;
            case 5: infoOperationsMenu(); break;
            case 0:
                cout << "\nExiting..." << endl;
                if (fs_instance != nullptr) {
                    cout << "Shutting down file system..." << endl;
                    if (current_session != nullptr) {
                        user_logout(current_session);
                    }
                    fs_shutdown(fs_instance);
                }
                return 0;
            default:
                cout << "Invalid choice" << endl;
        }
    }
    
    return 0;
}

