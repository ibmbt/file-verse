#ifndef CONFIG_PARSER_H
#define CONFIG_PARSER_H

#include <string>
#include <fstream>
#include <sstream>
#include <iostream>
#include <algorithm>

using namespace std;

struct FileSystemConfig {
    uint64_t total_size;
    uint64_t header_size;
    uint64_t block_size;
    uint32_t max_files;
    uint32_t max_filename_length;
    
    uint32_t max_users;
    string admin_username;
    string admin_password;
    bool require_auth;
    
    uint32_t port;
    uint32_t max_connections;
    uint32_t queue_timeout;
    
    FileSystemConfig() 
        : total_size(104857600),
          header_size(512),
          block_size(4096),
          max_files(1000),
          max_filename_length(255),
          max_users(50),
          admin_username("admin"),
          admin_password("admin123"),
          require_auth(true),
          port(8080),
          max_connections(20),
          queue_timeout(30) {}
};

class ConfigParser {
private:
    static string trim(const string& str) {
        size_t first = str.find_first_not_of(" \t\r\n");
        if (first == string::npos) return "";
        size_t last = str.find_last_not_of(" \t\r\n");
        return str.substr(first, (last - first + 1));
    }
    
    static string removeQuotes(const string& str) {
        string trimmed = trim(str);
        if (trimmed.length() >= 2) {
            if ((trimmed.front() == '"' && trimmed.back() == '"') ||
                (trimmed.front() == '\'' && trimmed.back() == '\'')) {
                return trimmed.substr(1, trimmed.length() - 2);
            }
        }
        return trimmed;
    }
    
    static bool parseBool(const string& str) {
        string lower = str;
        transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
        return (lower == "true" || lower == "1" || lower == "yes");
    }
    
public:
    static FileSystemConfig parse(const char* config_path) {
        FileSystemConfig config;
        
        if (!config_path || config_path[0] == '\0') {
            return config;
        }
        
        ifstream file(config_path);
        if (!file.is_open()) {
            return config;
        }
        
        string line;
        string current_section = "";
        
        while (getline(file, line)) {
            line = trim(line);
            
            if (line.empty() || line[0] == '#' || line[0] == ';') {
                continue;
            }
            
            if (line[0] == '[' && line.back() == ']') {
                current_section = line.substr(1, line.length() - 2);
                current_section = trim(current_section);
                continue;
            }
            
            size_t equals_pos = line.find('=');
            if (equals_pos == string::npos) {
                continue;
            }
            
            string key = trim(line.substr(0, equals_pos));
            string value = trim(line.substr(equals_pos + 1));
            
            size_t comment_pos = value.find('#');
            if (comment_pos != string::npos) {
                value = trim(value.substr(0, comment_pos));
            }
            
            if (current_section == "filesystem") {
                if (key == "total_size") config.total_size = stoull(value);
                else if (key == "header_size") config.header_size = stoull(value);
                else if (key == "block_size") config.block_size = stoull(value);
                else if (key == "max_files") config.max_files = stoul(value);
                else if (key == "max_filename_length") config.max_filename_length = stoul(value);
            }
            else if (current_section == "security") {
                if (key == "max_users") config.max_users = stoul(value);
                else if (key == "admin_username") config.admin_username = removeQuotes(value);
                else if (key == "admin_password") config.admin_password = removeQuotes(value);
                else if (key == "require_auth") config.require_auth = parseBool(value);
            }
            else if (current_section == "server") {
                if (key == "port") config.port = stoul(value);
                else if (key == "max_connections") config.max_connections = stoul(value);
                else if (key == "queue_timeout") config.queue_timeout = stoul(value);
            }
        }
        
        file.close();
        return config;
    }
    
    static void printConfig(const FileSystemConfig& config) {
        cout << "[filesystem]" << endl;
        cout << "  total_size: " << config.total_size << endl;
        cout << "  header_size: " << config.header_size << endl;
        cout << "  block_size: " << config.block_size << endl;
        cout << "  max_files: " << config.max_files << endl;
        cout << "  max_filename_length: " << config.max_filename_length << endl;
        
        cout << "[security]" << endl;
        cout << "  max_users: " << config.max_users << endl;
        cout << "  admin_username: " << config.admin_username << endl;
        cout << "  admin_password: " << config.admin_password << endl;
        cout << "  require_auth: " << config.require_auth << endl;
        
        cout << "[server]" << endl;
        cout << "  port: " << config.port << endl;
        cout << "  max_connections: " << config.max_connections << endl;
        cout << "  queue_timeout: " << config.queue_timeout << endl;
    }
};

#endif