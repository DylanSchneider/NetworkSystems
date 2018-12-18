#include <iostream>
#include <string>
#include <thread>
#include <sstream>
#include <fstream>
#include <vector>
#include <set>
#include <map>
#include <utility>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <dirent.h>

#define USAGE "./dfs <path/to/dir> <port>&"
#define MAXBUFSIZE 1024


// shared resources
std::string server_dir;
std::vector<std::pair<std::string, std::string>> auth_list;

std::mutex auth_list_lock;

bool file_exists (const std::string& name) {
    struct stat buffer;
    return (stat (name.c_str(), &buffer) == 0);
}


bool userIsValid(std::string username, std::string password) {
    bool isValid = false;
    
    for(const auto& user_pair : auth_list) {
        if (user_pair.first == username && user_pair.second == password) {
            isValid = true;
        }
    }
    return isValid;
}

//thread function
void dfs_worker(int conn_fd) {
   
    // wait for auth
    char auth_buf[MAXBUFSIZE];
    bzero(auth_buf, sizeof(auth_buf));
    read(conn_fd, auth_buf, MAXBUFSIZE);
    std::string auth(auth_buf);
    std::stringstream auth_ss(auth);
    std::string client_username, client_password;
    auth_ss >> client_username;
    auth_ss >> client_password;
    
    if (userIsValid(client_username, client_password)) {
        std::string validAuth = "VALID";
        write(conn_fd, validAuth.c_str(), validAuth.length()*sizeof(char));
    }
    else {
        std::string invalidAuth = "INVALID";
        write(conn_fd, invalidAuth.c_str(), invalidAuth.length()*sizeof(char));
        return;
    }
    
    // get user home directory
    std::string user_dir = server_dir + client_username + "/";
    
    // create user dir if doesnt exist
    struct stat sb;
    if (stat(user_dir.c_str(), &sb) != 0)
    {
        mkdir(user_dir.c_str(), S_IRWXU);
    }
    
    // wait for request
    char request_buf[MAXBUFSIZE];
    bzero(request_buf, sizeof(request_buf));
    read(conn_fd, request_buf, MAXBUFSIZE);
    std::string request(request_buf);
    std::cout << "req: " << request << std::endl;
    std::string command, filename, subdir;
    std::stringstream request_ss(request);
    request_ss >> command;
    request_ss >> filename;
    request_ss >> subdir;
    
    
    if (command == "LIST" || command == "list" || command == "ls") {
        std::string working_dir;
        if (filename.empty()) {
            working_dir = user_dir;
        }
        else {
            working_dir = user_dir + filename;
        }
        
        std::map<std::string, std::set<std::string>> file_parts;
        
        DIR* dirp = opendir(working_dir.c_str());
        struct dirent * dp;
        while ((dp = readdir(dirp)) != NULL) {
            std::string file(dp->d_name);
            
            // skip current and back dir
            if (file == "." || file == "..") {
                continue;
            }
            
            // prepend the path
            std::string full_file_path = working_dir + file;
            
            // check for directory
            struct stat stats;
            stat(full_file_path.c_str(), &stats);
            if (S_ISDIR(stats.st_mode)) {
                file += "/";
                file_parts[file].insert("0");
                continue;
            }
            
            // dfs files
            std::string id = file.substr(file.size()-1, file.size()-1);
            std::string filename_to_store = file.substr(1, file.size()-3);
            file_parts[filename_to_store].insert(id);
            
        }
        closedir(dirp);
        
        for (const auto& f : file_parts) {
            std::string str_to_send = f.first + "-";
            for (const auto id : f.second) {
                str_to_send += id + ":";
            }
            str_to_send.pop_back();
            str_to_send += " ";
            write(conn_fd, str_to_send.c_str(), str_to_send.length());
        }
        
    }
    
    if (command == "GET" || command == "get") {
        std::string working_dir;
        if (subdir.empty()) {
            working_dir = user_dir;
        }
        else {
            working_dir = user_dir + subdir;
        }

        std::string id_arr[] = {"1", "2", "3", "4"};
        std::string valid_ids = "";
        
        std::vector<std::string> found_filenames;
        for (int i=0; i<4; i++) {
            std::string file_to_test = working_dir + "." + filename + "." + id_arr[i];
            if (file_exists(file_to_test)) {
                valid_ids += (id_arr[i] + " ");
                found_filenames.push_back(file_to_test);
            }
        }
        write(conn_fd, valid_ids.c_str(), valid_ids.length());
        
        char read_sendfile_buf[MAXBUFSIZE];
        
        while(1) {
            bzero(read_sendfile_buf, sizeof(read_sendfile_buf));
            read(conn_fd, read_sendfile_buf, sizeof(read_sendfile_buf));
            std::string sendfile (read_sendfile_buf);
            if (sendfile == "END_GET_CMD====") {
                break;
            }
            std::string filepath = working_dir + sendfile;
            std::ifstream f(filepath);
            std::string contents;
            while(getline(f, contents)){
                write(conn_fd, contents.c_str(), contents.length());
            }
            std::string eof = "END_FILE====";
            write(conn_fd, eof.c_str(), eof.length());
        }
        
    }
    else if (command == "PUT" || command == "put") {
        std::string working_dir;
        if (subdir.empty()) {
            working_dir = user_dir;
        }
        else {
            working_dir = user_dir + subdir;
        }
        
        // read chunk ids
        char put_parts_buf[MAXBUFSIZE];
        read(conn_fd, put_parts_buf, 3);
        std::string parts (put_parts_buf);
        std::stringstream file_parts (parts);
        std::string first_part_id, second_part_id;
        file_parts >> first_part_id;
        file_parts >> second_part_id;
        
        // read first file
        std::string chunk1_filename = "." + filename + "." + first_part_id;
        std::string filepath1 = working_dir + chunk1_filename;
        std::ofstream outfile1 (filepath1);
        char put_read_buf[MAXBUFSIZE];
        read(conn_fd, put_read_buf, MAXBUFSIZE);
        std::string read_result1 (put_read_buf);
        outfile1 << read_result1;
        outfile1.close();
        bzero(put_read_buf, sizeof(put_read_buf));
        std::cout << "wrote to " + filepath1 << std::endl;
        
        std::string write_success = "SUCCESS";
        write(conn_fd, write_success.c_str(), write_success.length());
        
        // read second file
        std::string chunk2_filename = "." + filename + "." + second_part_id;
        std::string filepath2 = working_dir + chunk2_filename;
        std::ofstream outfile2 (filepath2);
        read(conn_fd, put_read_buf, MAXBUFSIZE);
        std::string read_result2 (put_read_buf);
        outfile2 << read_result2;
        outfile2.close();
        bzero(put_read_buf, sizeof(put_read_buf));
        std::cout << "wrote to " + filepath2 << std::endl;
    }
    
    else if (command == "MKDIR" || command == "mkdir") {
        std::string full_dir_path = user_dir + filename;
        mkdir(full_dir_path.c_str(), S_IRWXU);
    }
    close(conn_fd);
}

int main(int argc, const char* argv[]) {
    if (argc != 3) {
        std::cout << USAGE << std::endl;
        exit(1);
    }
    
    server_dir = argv[1];
    
    int server_port;
    server_port = std::stoi(argv[2]);
    
    std::cout << "server connected to " << server_dir << " on port " << server_port << std::endl;
    
    // server conf
    std::string server_conf_filename = "dfs.conf";
    std::ifstream server_conf_file(server_conf_filename);
    std::string line;
    while(std::getline(server_conf_file, line)) {
        std::string temp_username, temp_password;
        std::stringstream line_ss(line);
        line_ss >> temp_username;
        line_ss >> temp_password;
        
        auth_list.push_back(std::make_pair(temp_username, temp_password));
    }
    
    
    int server_socket_fd, server_conn_fd;
    bool error = false;
    struct sockaddr_in servaddr;
    
    server_socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    
    bzero(&servaddr, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htons(INADDR_ANY);
    servaddr.sin_port = htons(server_port);
    
    bind(server_socket_fd, (struct sockaddr *) &servaddr, sizeof(servaddr));
    
    listen(server_socket_fd, 10);
    
    while(1) {
        server_conn_fd = accept(server_socket_fd,(struct sockaddr*) NULL, NULL);
        std::thread server_worker(dfs_worker, server_conn_fd);
        server_worker.join();
    }

}
