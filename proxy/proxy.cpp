#include <iostream>
#include <string>
#include <thread>
#include <sstream>
#include <fstream>
#include <chrono>
#include <ctime>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <errno.h>
#include <stdio.h>
#include <csignal>
#include <regex>
#include <map>
#include <mutex>
#include <vector>

// progrma specifics
#define USAGE "./proxy <port> [<timeout>]"
#define BUFSIZE 1028

#define BLACKLIST ".proxy-blacklist.txt"

std::map<std::string, std::string> proxy_cache;
std::mutex proxy_cache_lock;

int client_sock_fd;
std::mutex client_sock_lock;

std::vector<int> server_sock_list;
std::mutex server_sock_lock;

std::vector<std::thread> thread_pool; // only accessed by sig handler so no need for a lock

void request_cleanup(int conn_fd) {
    close(conn_fd);
    std::cout << std::endl;
}

void close_all_connections() {
    client_sock_lock.lock();
    close(client_sock_fd);
    client_sock_lock.unlock();
    
    server_sock_lock.lock();
    for(int i = 0; i < server_sock_list.size(); i++){
        close(server_sock_list[i]);
    }
    server_sock_lock.unlock();
}

void join_threads() {
    for(int i = 0; i < thread_pool.size(); i++) {
        thread_pool[i].join();
    }
}

void signal_handler(int signum) {
    std::cout << "Received signal, closing sockets and exiting." << std::endl;
    close_all_connections();
    join_threads();
    exit(1);
}

void connection_handler(int client_conn_fd, int timeout){
    
    // set timeout if provided
    if(timeout) {
        struct timeval tv;
        tv.tv_sec = timeout;
        setsockopt(client_conn_fd, SOL_SOCKET, SO_RCVTIMEO, (const char *) &tv, sizeof(tv));
    }
    
    while(1) {
        char client_read_buf[BUFSIZE];
        bzero(client_read_buf, sizeof(client_read_buf));
        
        int bytes_read = read(client_conn_fd, client_read_buf, BUFSIZE);
        if(bytes_read <= 0) {
            if (errno == EWOULDBLOCK) {
				std::cout << "closing connection because of request timeout" << std::endl;
                close(client_conn_fd);
                return;
            }
            else {
                std::string err_body = "<html><body text='red'>500 Internal Server Error</body></html>";
                std::string int_err = "HTTP/1.1 500 Internal Server Error\r\nContent-Type: text html\r\nContent-Length: " + std::to_string(err_body.length()) + "\r\n\r\n" + err_body;
                write(client_conn_fd, int_err.c_str(), int_err.length()*sizeof(char));
            }
            request_cleanup(client_conn_fd);
            return;
        }
        std::string request = std::string(client_read_buf);
        
        
        // parse header
        std::stringstream recv_data(client_read_buf);
        
        // request line
        std::string req_line;
        std::getline(recv_data, req_line);
        if (req_line == "") {
            continue;
        }
        std::stringstream elements(req_line);
        
        std::string cmd, uri, version, pipeline;
        elements >> cmd;
        elements >> uri;
        elements >> version;
    
        
        // get hostname from uri
        std::string hostname;
        if (uri.back() == '/') {
            uri.pop_back();
        }
        
        // check if prefix is provided
        std::size_t found_pre = uri.find("://");
        if (found_pre != std::string::npos) {
            hostname = uri.substr(found_pre+3);
        }
        else {
            hostname = uri;
        }
        
        // check if link was provided
        std::string link;
        std::size_t found_link = hostname.find_last_of("/");
        if (found_link == std::string::npos) {
            link = "";
        }
        else {
            link = hostname.substr(found_link+1);
            hostname = hostname.substr(0, found_link);
        }
        
        // check if port was provided
        int server_port;
        std::size_t found_port = hostname.find_last_of(":");
        if (found_port == std::string::npos) {
            server_port = 80;
        }
        else {
            try {
                server_port = stoi(hostname.substr(found_port+1));
            }
            catch(std::exception e){
                std::cout << "server port must be an integer" << std::endl;
                return;
            }
            hostname = hostname.substr(0, found_port);
        }
        
        /*
        std::cout << "------------DEBUG-----------" << std::endl;
        std::cout << "CMD:" << cmd << std::endl;
        std::cout << "hostname:" << hostname << std::endl;
        std::cout << "Version:" << version << std::endl;
        std::cout << "Server port:" << server_port << std::endl;
        std::cout << "Link:" << link << std::endl;
        std::cout << "------------DEBUG-----------" << std::endl;
        */
        
        // validate the command and version
        if (cmd != "GET" or (version != "HTTP/1.0" and version != "HTTP/1.1")){
            std::string err_body = "<html><body>400 Bad Request</body></html>";
            std::string bad_req_out = "HTTP/1.1 400 Bad Request\r\nContent-Type: text/html\r\nContent-Length: " + std::to_string(err_body.length()) + "\r\n\r\n" + err_body;
            write(client_conn_fd, bad_req_out.c_str(), bad_req_out.length()*sizeof(char));
            request_cleanup(client_conn_fd);
            return;
        }
        
        // check for hostname in chache
        proxy_cache_lock.lock();
        std::string firstIP;
        if (proxy_cache.find(hostname) == proxy_cache.end()) {
            // not found, resolve ip and store in cache
            struct hostent *local = gethostbyname(hostname.c_str());
            if (!local) {
                std::string err_body = "<html><body text='red'>404 Not Found Reason URL does not exist: " + uri + "</body></html>";
                std::string not_found = version + " 404 Not Found\r\nContent-Type: text/html\r\nContent-Length:  " + std::to_string(err_body.length()) + "\r\n\r\n" + err_body;
                write(client_conn_fd, not_found.c_str(), not_found.length()*sizeof(char));
                close(client_conn_fd);
                proxy_cache_lock.unlock();
                return;
            }
            struct in_addr ** addr_list = (struct in_addr **)local->h_addr_list;

            // Get the first one;
            firstIP = inet_ntoa(*addr_list[0]);
            proxy_cache.insert(std::pair<std::string, std::string> (hostname, firstIP));
        }
        else {
            // found
            std::cout << "found " << hostname << " in cache" << std::endl;
            firstIP = proxy_cache[hostname];
        }
        std::cout << "Resolved " << hostname << " --> " << firstIP << std::endl;
        proxy_cache_lock.unlock();
        
        // check both hostname and IP in blacklist
        std::ifstream in_file(BLACKLIST);
        //std::stringstream file_ss;
        std::string line;
        while (std::getline(in_file, line)) {
            if (line == uri or line == hostname or line == firstIP){
                // found in blacklist, abort
                std::string err_body = "<html><body text='red'>403 Forbidden</body></html>";
                std::string not_found = version + " 403 Not Forbidden\r\nContent-Type: text/html\r\nContent-Length:  " + std::to_string(err_body.length()) + "\r\n\r\n" + err_body;
                write(client_conn_fd, not_found.c_str(), not_found.length()*sizeof(char));
                close(client_conn_fd);
                return;
            }
        }
        
        
        
        if(cmd == "GET") { // skeleton for if we need to add more commands in the future
            std::cout << "full req:\n" << request << std::endl;
            // connect to server
            int server_sock_fd;
            if ((server_sock_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
                printf("\n Socket creation error \n");
                return;
            }
            server_sock_lock.lock();
            server_sock_list.push_back(server_sock_fd);
            server_sock_lock.unlock();
            
            struct sockaddr_in serv_addr;
            serv_addr.sin_family = AF_INET;
            serv_addr.sin_port = htons(server_port);
            serv_addr.sin_addr.s_addr = inet_addr(firstIP.c_str());
            
            if (connect(server_sock_fd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
            {
                printf("\nConnection Failed \n");
                return;
            }
            std::cout << "Established connection with " << uri << std::endl;
            
            // send request to server
            if (link == "") {
                std::string request = cmd + " / "  + version + "\r\nHost: " + hostname + "\r\n\r\n";
                std::cout << "Req: " << request << std::endl;
                if (write(server_sock_fd, request.c_str(), request.length()) < 0) {
                    std::cout << "write to server failed, abort" << std::endl;
                    request_cleanup(client_conn_fd);
                    return;
                }
            }
            else {
                std::string request_with_link = cmd + " " + "/" + link + " " + version;
                std::cout << "ReqLink: " << request_with_link << std::endl;
                if (write(server_sock_fd, request_with_link.c_str(), request_with_link.length()) < 0) {
                    std::cout << "write to server failed, abort" << std::endl;
                    request_cleanup(client_conn_fd);
                    return;
                }
            }
        
            
            // setup read timeout in case server doesnt reply
            struct timeval tv;
            tv.tv_sec = 5;
            setsockopt(server_sock_fd, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof tv);
            
            // receive from server, write to client
            char server_read_buf[BUFSIZE];
            while(1) {
                if (read(server_sock_fd, server_read_buf, sizeof(server_read_buf)) <= 0) {
                    if (errno == EWOULDBLOCK) {
                        std::cout << "No response from server in 5 seconds" << std::endl;
                    }
                    // eof
                    break;
                }
                /*if (write(client_conn_fd, server_read_buf, strlen(server_read_buf)) < 0) {
                    std::cout << "write from server failed" << std::endl;
                    request_cleanup(client_conn_fd);
                    return;
                }*/
                write(client_conn_fd, server_read_buf, strlen(server_read_buf));
                bzero(server_read_buf, BUFSIZE);
            }
            std::cout << "wrote to client successfully" << std::endl;
        }
        
        
        std::cout << std::endl;
    }
    request_cleanup(client_conn_fd);
}

int main(int argc, char*argv[]) {
    
	// arg parsing and error checking

    if(argc != 2 and argc != 3) {
        std::cout << USAGE << std::endl;
        return 1;
    }
    
    int client_port;
    try {
        client_port = std::stoi(argv[1]);
        
    }
    catch(std::exception& e)
    {
        std::cout << "<port> must be an integer" << '\n';
        return 1;
    }
    if(client_port <= 1024) {
        std::cout << "port must be greater than 1024" << std::endl;
        return 1;
    }
    
    int timeout;
    if (argc == 3) {
        try {
            timeout = std::stoi(argv[2]);
        }
        catch(std::exception& e)
        {
            std::cout << "<timeout> must be a positive integer" << '\n';
            return 1;
        }
        if (timeout <= 0) {
            std::cout << "<timeout> must be a positive integer" << '\n';
            return 1;
        }
    }
    else {
        timeout = 0; // no timeout
    }
	
	// setup
    
    signal(SIGINT, signal_handler);
    client_sock_fd = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in servaddr;
    
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htons(INADDR_ANY);
    servaddr.sin_port = htons(client_port);
    
    bind(client_sock_fd,(struct sockaddr *) &servaddr, sizeof(servaddr));
    listen(client_sock_fd, 128);
    
    if (timeout) {
        std::cout << "Started proxy with:\n" << "host - localhost\n" << "port - "<< client_port << "\ntimeout - " << timeout << std::endl << std::endl;
    }
    else {
        std::cout << "Started proxy with:\n" << "host - localhost\n" << "port - "<< client_port << std::endl << std::endl;
    }
    
    while(1) { // only stop is SIGINT
        int client_conn_fd;
        client_conn_fd = accept(client_sock_fd,(struct sockaddr*) NULL, NULL);
        std::thread server_worker(connection_handler, client_conn_fd, timeout);
        //std::cout << "Connected to client " << client_conn_fd << std::endl;
        server_worker.join();
    }
}

