#include <iostream>
#include <cstdio>
#include <string>
#include <sstream>
#include <fstream>
#include <vector>
#include <utility>
#include <memory>
#include <array>
#include <set>
#include <map>
#include <stdexcept>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>

#define USAGE "./dfc <path/to/conf>"
#define MAXBUFSIZE 1024

struct conf {
    // client conf
    std::string client_username;
    std::string client_password;
    // server <id, ip, port>
    std::vector<std::tuple<int, std::string, int>> servers;
};

conf read_conf(std::string filename) {
    std::ifstream client_conf_file(filename);
    std::string line;
    
    conf c;
    // read 4 server lines
    for(int i=0; i<4; i++) {
        std::getline(client_conf_file, line);
        std::string ip_port_pair;
        std::stringstream line_ss(line);
        std::getline(line_ss, ip_port_pair, ' ');
        std::getline(line_ss, ip_port_pair, ' ');
        std::getline(line_ss, ip_port_pair, ' ');
        std::stringstream ip_port_pair_ss (ip_port_pair);
        std::string ip, port;
        std::getline(ip_port_pair_ss, ip, ':');
        std::getline(ip_port_pair_ss, port);
        c.servers.push_back(std::make_tuple(i+1, ip, std::stoi(port)));
    }
    
    // read username/password
    getline(client_conf_file, line);
    std::stringstream line_ss1(line);
    getline(line_ss1, c.client_username, ' ');
    getline(line_ss1, c.client_username);
    
    getline(client_conf_file, line);
    std::stringstream line_ss2(line);
    getline(line_ss2, c.client_password, ' ');
    getline(line_ss2, c.client_password);
    
    client_conf_file.close();
    return c;
}

void closeServers(std::vector<std::pair<int, int>> conn_fds) {
    for(const auto& conn_fd : conn_fds) {
        close(conn_fd.second);
    }
}

std::ifstream::pos_type filesize(std::string filename)
{
    std::ifstream in(filename, std::ifstream::ate | std::ifstream::binary);
    return in.tellg();
}

std::string getCommandOutput(const char* cmd) {
    std::array<char, 128> buffer;
    std::string result;
    std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd, "r"), pclose);
    if (!pipe) {
        throw std::runtime_error("popen() failed!");
    }
    while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
        result += buffer.data();
    }
    return result;
}

bool userIsValid(std::string username, std::string password, std::vector<std::pair<int, int>> conn_fds) {
    bool isValid = false;
    for(const auto& conn_fd : conn_fds) {
        std::string auth_str = username + " " + password;
        write(conn_fd.second, auth_str.c_str(), auth_str.length());
        char auth_read_buf[MAXBUFSIZE];
        read(conn_fd.second, auth_read_buf, MAXBUFSIZE);
        std::string auth_response(auth_read_buf);
        if (auth_response == "INVALID") {
            isValid = false;
        }
        else if (auth_response == "VALID") {
            isValid = true;
        }
    }
    return isValid;
}

void list(std::vector<std::pair<int, int>> conn_fds, std::string subdir) {
    std::map<std::string, std::set<std::string>> list_map;
    
    for (const auto& conn_fd : conn_fds) {
        char read_list_buf[MAXBUFSIZE];
        bzero(read_list_buf, sizeof(read_list_buf));
        while(read(conn_fd.second, read_list_buf, MAXBUFSIZE) > 0) {
            std::string list(read_list_buf);
            std::stringstream list_ss(list);
            
            std::string file;
            while(std::getline(list_ss, file, ' ')) {
                std::stringstream file_ss (file);
                std::string fn;
                getline(file_ss, fn, '-');
                std::string id;
                while(getline(file_ss, id, ':')) {
                    list_map[fn].insert(id);
                }
            }
            bzero(read_list_buf, sizeof(read_list_buf));
        }
    }
    if (!subdir.empty()) {
        std::cout << subdir << std::endl;
    }
    std::cout << "-----------------------" << std::endl;
    for (const auto& lm : list_map) {
        std::string fname = lm.first;
        std::string complete;
        std::string first_elem = *(lm.second.begin());
        std::set<std::string> ideal_ids = {"1", "2", "3", "4"};
        if (first_elem == "0") {
            std::cout << fname << std::endl;
        }
        else if (lm.second == ideal_ids) {
            std::cout << fname << std::endl;
        }
        else {
            std::cout << fname + "[incomplete]" << std::endl;
        }
    }
    std::cout << "-----------------------" << std::endl;
    
}

void get(std::string filename, std::vector<std::pair<int, int>> conn_fds) {
    // get part locations from server in 1 read
    std::map<std::string, int> part_locations;
    for (const auto& conn_fd : conn_fds) {
        char read_part_buf[MAXBUFSIZE];
        bzero(read_part_buf, sizeof(read_part_buf));
        read(conn_fd.second, read_part_buf, sizeof(read_part_buf));
        std::string read_part(read_part_buf);
        std::stringstream read_part_ss(read_part);
        std::string file1, file2;
        read_part_ss >> file1;
        read_part_ss >> file2;
        
        //not in map, add to map
        if(part_locations.find(file1) == part_locations.end()) {
            part_locations[file1] = conn_fd.second;
        }
        if (part_locations.find(file2) == part_locations.end()) {
            part_locations[file2] = conn_fd.second;
        }
    }
    
    std::string found_file_parts = "";
    for(const auto& map_pair : part_locations) {
        found_file_parts += map_pair.first;
    }
    
    if (found_file_parts == "1234") {
        std::ofstream write_file(filename);
        // loop through map
        for(const auto& map_pair : part_locations) {
            std::string file_to_get = "." + filename + "." + map_pair.first;
            write(map_pair.second, file_to_get.c_str(), file_to_get.length());
            
            
            
            char file_read_buf[MAXBUFSIZE];
            while(1) {
                bzero(file_read_buf, sizeof(file_read_buf));
                read(map_pair.second, file_read_buf, sizeof(file_read_buf));
                std::string contents(file_read_buf);
                std::size_t found = contents.find("END_FILE====");
                if (found != std::string::npos) {
                    std::string contents2 = contents.substr(0, found);
                    write_file << contents2;
                    break;
                }
                write_file << contents;
                
            }
            
        }
        write_file.close();
    }
    
    for (const auto& conn_fd : conn_fds) {
        //send end get
        std::string end_get = "END_GET_CMD====";
        write(conn_fd.second, end_get.c_str(), end_get.length());
    }
    
}


void put(std::string filename, std::vector<std::pair<int, int>> conn_fds) {
    
    // send the file
    //split file into 4 parts
    int total_file_size = filesize(filename);
    int chunk_size_int = total_file_size / 4;
    int chunk_size_remainder = total_file_size % 4;
    
    // make chunk 4 a little bigger if the file isnt perfectly divisible
    int chunk_size, chunk4_size;
    if (chunk_size_remainder == 0){
        chunk_size = chunk4_size = chunk_size_int;
    }
    else {
        chunk_size = chunk_size_int;
        chunk4_size = chunk_size_int + chunk_size_remainder;
    }
    
    char chunk1 [chunk_size];
    char chunk2 [chunk_size];
    char chunk3 [chunk_size];
    char chunk4 [chunk4_size];
    bzero(chunk1, chunk_size);
    bzero(chunk2, chunk_size);
    bzero(chunk3, chunk_size);
    bzero(chunk4, chunk4_size);
    
    int file_placeholder = 0;
    std::ifstream file_to_chunk(filename, std::ifstream::binary);
    
    file_to_chunk.read(chunk1, chunk_size);
    file_placeholder += chunk_size;
    
    file_to_chunk.seekg(file_placeholder);
    file_to_chunk.read(chunk2, chunk_size);
    file_placeholder += chunk_size;
    
    file_to_chunk.seekg(file_placeholder);
    file_to_chunk.read(chunk3, chunk_size);
    file_placeholder += chunk_size;
    
    file_to_chunk.seekg(file_placeholder);
    file_to_chunk.read(chunk4, chunk4_size);
    
    file_to_chunk.close();
    
    // get file hash
    std::string md5_command = "md5 -q " + filename;
    std::string file_hash = getCommandOutput(md5_command.c_str());
    std::string file_hash_small = file_hash.substr(23,31);
    int x = std::stoul(file_hash_small, nullptr, 16) % 4;
    
    std::string chunk_ids;
    char read_success_buf[MAXBUFSIZE];
    std::cout << "hash value x = " << x << std::endl;
    switch(x) {
        case 0:
            for(const auto& conn_fd : conn_fds) {
                chunk_ids.clear();
                bzero(read_success_buf, sizeof(read_success_buf));
                switch(conn_fd.first) {
                    case 1:
                        chunk_ids = "1 2";
                        write(conn_fd.second, chunk_ids.c_str(), chunk_ids.length());
                        write(conn_fd.second, chunk1, strlen(chunk1));
                        read(conn_fd.second, read_success_buf, MAXBUFSIZE);
                        write(conn_fd.second, chunk2, strlen(chunk2));
                        break;
                    case 2:
                        chunk_ids = "2 3";
                        write(conn_fd.second, chunk_ids.c_str(), chunk_ids.length());
                        write(conn_fd.second, chunk2, strlen(chunk2));
                        read(conn_fd.second, read_success_buf, MAXBUFSIZE);
                        write(conn_fd.second, chunk3, strlen(chunk3));
                        break;
                    case 3:
                        chunk_ids = "3 4";
                        write(conn_fd.second, chunk_ids.c_str(), chunk_ids.length());
                        write(conn_fd.second, chunk3, strlen(chunk3));
                        read(conn_fd.second, read_success_buf, MAXBUFSIZE);
                        write(conn_fd.second, chunk4, strlen(chunk4));
                        break;
                    case 4:
                        chunk_ids = "4 1";
                        write(conn_fd.second, chunk_ids.c_str(), chunk_ids.length());
                        write(conn_fd.second, chunk4, strlen(chunk4));
                        read(conn_fd.second, read_success_buf, MAXBUFSIZE);
                        write(conn_fd.second, chunk1, strlen(chunk1));
                        break;
                }
            }
            break;
        
        case 1:
            for(const auto& conn_fd : conn_fds) {
                chunk_ids.clear();
                switch(conn_fd.first) {
                    case 1:
                        chunk_ids = "4 1";
                        write(conn_fd.second, chunk_ids.c_str(), chunk_ids.length());
                        write(conn_fd.second, chunk4, strlen(chunk4));
                        read(conn_fd.second, read_success_buf, MAXBUFSIZE);
                        write(conn_fd.second, chunk1, strlen(chunk1));
                        break;
                    case 2:
                        chunk_ids = "1 2";
                        write(conn_fd.second, chunk_ids.c_str(), chunk_ids.length());
                        write(conn_fd.second, chunk1, strlen(chunk1));
                        read(conn_fd.second, read_success_buf, MAXBUFSIZE);
                        write(conn_fd.second, chunk2, strlen(chunk2));
                        break;
                    case 3:
                        chunk_ids = "2 3";
                        write(conn_fd.second, chunk_ids.c_str(), chunk_ids.length());
                        write(conn_fd.second, chunk2, strlen(chunk2));
                        read(conn_fd.second, read_success_buf, MAXBUFSIZE);
                        write(conn_fd.second, chunk3, strlen(chunk3));
                        break;
                    case 4:
                        chunk_ids = "3 4";
                        write(conn_fd.second, chunk_ids.c_str(), chunk_ids.length());
                        write(conn_fd.second, chunk3, strlen(chunk3));
                        read(conn_fd.second, read_success_buf, MAXBUFSIZE);
                        write(conn_fd.second, chunk4, strlen(chunk4));
                        break;
                }
            }
            break;
            
        case 2:
            for(const auto& conn_fd : conn_fds) {
                chunk_ids.clear();
                bzero(read_success_buf, sizeof(read_success_buf));
                switch(conn_fd.first) {
                    case 1:
                        chunk_ids = "3 4";
                        write(conn_fd.second, chunk_ids.c_str(), chunk_ids.length());
                        write(conn_fd.second, chunk3, strlen(chunk3));
                        read(conn_fd.second, read_success_buf, MAXBUFSIZE);
                        write(conn_fd.second, chunk4, strlen(chunk4));
                        break;
                    case 2:
                        chunk_ids = "4 1";
                        write(conn_fd.second, chunk_ids.c_str(), chunk_ids.length());
                        write(conn_fd.second, chunk4, strlen(chunk4));
                        read(conn_fd.second, read_success_buf, MAXBUFSIZE);
                        write(conn_fd.second, chunk1, strlen(chunk1));
                        break;
                    case 3:
                        chunk_ids = "1 2";
                        write(conn_fd.second, chunk_ids.c_str(), chunk_ids.length());
                        write(conn_fd.second, chunk1, strlen(chunk1));
                        read(conn_fd.second, read_success_buf, MAXBUFSIZE);
                        write(conn_fd.second, chunk2, strlen(chunk2));
                        break;
                    case 4:
                        chunk_ids = "2 3";
                        write(conn_fd.second, chunk_ids.c_str(), chunk_ids.length());
                        write(conn_fd.second, chunk2, strlen(chunk2));
                        read(conn_fd.second, read_success_buf, MAXBUFSIZE);
                        write(conn_fd.second, chunk3, strlen(chunk3));
                        break;
                }
            }
            break;
            
        case 3:
            for(const auto& conn_fd : conn_fds) {
                chunk_ids.clear();
                bzero(read_success_buf, sizeof(read_success_buf));
                switch(conn_fd.first) {
                    case 1:
                        chunk_ids = "2 3";
                        write(conn_fd.second, chunk_ids.c_str(), chunk_ids.length());
                        write(conn_fd.second, chunk2, strlen(chunk2));
                        read(conn_fd.second, read_success_buf, MAXBUFSIZE);
                        write(conn_fd.second, chunk3, strlen(chunk3));
                        break;
                    case 2:
                        chunk_ids = "3 4";
                        write(conn_fd.second, chunk_ids.c_str(), chunk_ids.length());
                        write(conn_fd.second, chunk3, strlen(chunk3));
                        read(conn_fd.second, read_success_buf, MAXBUFSIZE);
                        write(conn_fd.second, chunk4, strlen(chunk4));
                        break;
                    case 3:
                        chunk_ids = "4 1";
                        write(conn_fd.second, chunk_ids.c_str(), chunk_ids.length());
                        write(conn_fd.second, chunk4, strlen(chunk4));
                        read(conn_fd.second, read_success_buf, MAXBUFSIZE);
                        write(conn_fd.second, chunk1, strlen(chunk1));
                        break;
                    case 4:
                        chunk_ids = "1 2";
                        write(conn_fd.second, chunk_ids.c_str(), chunk_ids.length());
                        write(conn_fd.second, chunk1, strlen(chunk1));
                        read(conn_fd.second, read_success_buf, MAXBUFSIZE);
                        write(conn_fd.second, chunk2, strlen(chunk2));
                        break;
                }
            }
            break;
            
    }
}

int main(int argc, const char* argv[]) {
    if (argc != 2) {
        std::cout << USAGE << std::endl;
        exit(1);
    }
    
    std::string client_conf_filename(argv[1]);
    
    struct addrinfo hints, *server;
    bzero(&hints, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;
    
    int sock; //fd for server socket
    bool error = false;
    
    std::string request;
    for(;;) {
        std::cout << "> ";
        std::getline(std::cin, request);
        
        std::stringstream request_ss(request);
        std::string command, filename, subdir;
        request_ss >> command;
        request_ss >> filename;
        request_ss >> subdir;
        
        
        if (command == "LIST" || command == "list" || command == "ls") {
            conf client_conf = read_conf(client_conf_filename);
            // connected_servers <id, conn_fd>
            std::vector<std::pair<int, int>> connected_servers;
            for (const auto& server : client_conf.servers) {
                struct sockaddr_in serv_addr;
                serv_addr.sin_family = AF_INET;
                serv_addr.sin_port = htons(std::get<2>(server));
                serv_addr.sin_addr.s_addr = inet_addr(std::get<1>(server).c_str());
                
                int server_sock_fd;
                if ((server_sock_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
                    std::cout << "Socket creation error" << std::endl;
                    exit(1);
                }
                
                if (connect(server_sock_fd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
                {
                    std::cout << "failed to connect to port: " << std::get<2>(server) << std::endl;
                    continue;
                }
                
                connected_servers.push_back(std::make_pair(std::get<0>(server), server_sock_fd));
            }
            
            // send auth to all servers that are able to connect
            if(!userIsValid(client_conf.client_username, client_conf.client_password, connected_servers)) {
                std::cout << client_conf.client_username + " does not have access to this server. Please provide new login credentials." << std::endl;
                continue;
            }
            
            for (const auto& connected_server : connected_servers) {
                write(connected_server.second, request.c_str(), request.length());
            }
            
            list(connected_servers, subdir);
            
            closeServers(connected_servers);
        }
        else if (command == "GET" || command == "get") {
            if (filename.empty()) {
                std::cout << "use GET <filename>" << std::endl;
                continue;
            }
            
            conf client_conf = read_conf(client_conf_filename);
            
            // connected_servers <id, conn_fd>
            std::vector<std::pair<int, int>> connected_servers;
            for (const auto& server : client_conf.servers) {
                struct sockaddr_in serv_addr;
                serv_addr.sin_family = AF_INET;
                serv_addr.sin_port = htons(std::get<2>(server));
                serv_addr.sin_addr.s_addr = inet_addr(std::get<1>(server).c_str());
                
                int server_sock_fd;
                if ((server_sock_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
                    std::cout << "Socket creation error" << std::endl;
                    exit(1);
                }
                
                if (connect(server_sock_fd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
                {
                    std::cout << "failed to connect to port: " << std::get<2>(server) << std::endl;
                    continue;
                }
                
                connected_servers.push_back(std::make_pair(std::get<0>(server), server_sock_fd));
            }
            
            // send auth to all servers that are able to connect
            if(!userIsValid(client_conf.client_username, client_conf.client_password, connected_servers)) {
                std::cout << client_conf.client_username + " does not have access to this server. Please provide new login credentials." << std::endl;
                continue;
            }
            
            // send request to servers
            for(const auto& connected_server : connected_servers) {
                write(connected_server.second, request.c_str(), request.length());
            }
            
            // do get
            get(filename, connected_servers);

            closeServers(connected_servers);
            
            std::cout << "wrote " << filename << " from dfs." << std::endl;
        
        }
        else if (command == "PUT" || command == "put") {
            if (filename.empty()) {
                std::cout << "use PUT <filename>" << std::endl;
                continue;
            }
            
            conf client_conf = read_conf(client_conf_filename);
            
            // connected_servers <id, conn_fd>
            std::vector<std::pair<int, int>> connected_servers;
            for (const auto& server : client_conf.servers) {
                struct sockaddr_in serv_addr;
                serv_addr.sin_family = AF_INET;
                serv_addr.sin_port = htons(std::get<2>(server));
                serv_addr.sin_addr.s_addr = inet_addr(std::get<1>(server).c_str());
                
                int server_sock_fd;
                if ((server_sock_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
                    std::cout << "Socket creation error" << std::endl;
                    exit(1);
                }
                
                if (connect(server_sock_fd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
                {
                    std::cout << "Connection failed, all servers must be up for PUT" << std::endl;
                    exit(1);
                }
                
                connected_servers.push_back(std::make_pair(std::get<0>(server), server_sock_fd));
            }
            
            // send auth to all servers that are able to connect
            if(!userIsValid(client_conf.client_username, client_conf.client_password, connected_servers)) {
                std::cout << client_conf.client_username + " does not have access to this server. Please provide new login credentials." << std::endl;
                continue;
            }
            
            // send request to servers
            for(const auto& connected_server : connected_servers) {
                write(connected_server.second, request.c_str(), request.length());
            }
            
            put(filename, connected_servers);
            
            closeServers(connected_servers);
            
            std::cout << "added " << filename << " to dfs." << std::endl;
        }
        else if (command == "MKDIR" || command == "mkdir") {
            if (filename.empty()) {
                std::cout << "use MKDIR <dir name>" << std::endl;
                continue;
            }
            
            conf client_conf = read_conf(client_conf_filename);
            
            // connected_servers <id, conn_fd>
            std::vector<std::pair<int, int>> connected_servers;
            for (const auto& server : client_conf.servers) {
                struct sockaddr_in serv_addr;
                serv_addr.sin_family = AF_INET;
                serv_addr.sin_port = htons(std::get<2>(server));
                serv_addr.sin_addr.s_addr = inet_addr(std::get<1>(server).c_str());
                
                int server_sock_fd;
                if ((server_sock_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
                    std::cout << "Socket creation error" << std::endl;
                    exit(1);
                }
                
                if (connect(server_sock_fd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
                {
                    std::cout << "Connection failed, all servers must be up for MKDIR" << std::endl;
                    exit(1);
                }
                
                connected_servers.push_back(std::make_pair(std::get<0>(server), server_sock_fd));
            }
            
            // send auth to all servers that are able to connect
            if(!userIsValid(client_conf.client_username, client_conf.client_password, connected_servers)) {
                std::cout << client_conf.client_username + " does not have access to this server. Please provide new login credentials." << std::endl;
                continue;
            }
            
            // send request to servers
            for(const auto& connected_server : connected_servers) {
                write(connected_server.second, request.c_str(), request.length());
            }
            
            closeServers(connected_servers);
            
            std::cout << "added " << filename << " to dfs." << std::endl;
        }
        else {
            std::cout << "Unrecognized command, try again" << std::endl;
        }
    }
}
