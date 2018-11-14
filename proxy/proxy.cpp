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
#include <unistd.h>
#include <errno.h>
#include <stdio.h>
#include <csignal>
#include <regex>

// progrma specifics
#define USAGE "./proxy <port> [<timeout>]"
#define BUFSIZE 1028

// text headers
#define HTML_TEXT "text/html"
#define TXT_TEXT "text/plain"
#define PNG_TEXT "image/png"
#define GIF_TEXT "image/gif"
#define JPG_TEXT "image/jpg"
#define CSS_TEXT "text/css"
#define JS_TEXT "application/javascript"

#define HOSTNAME_REGEX "^(([a-zA-Z0-9]|[a-zA-Z0-9][a-zA-Z0-9\-]*[a-zA-Z0-9])\.)*([A-Za-z0-9]|[A-Za-z0-9][A-Za-z0-9\-]*[A-Za-z0-9])$"

#define BLACKLIST ".proxy-blacklist.txt"

int sock_fd = -1;
bool exit_requested = false;

void signal_handler(int signum) {
    std::cout << "Received signal, closing socket and exiting." << std::endl;
    exit_requested = true;
    if (sock_fd != -1){
        if (close(sock_fd) < 0){
            std::cout << "error closing socket" << std::endl;
        }
    }
    exit(1);
}

void request_cleanup(int conn_fd) {
    if (close(conn_fd) < 0) {
        std::cout << "error closing connection" << std::endl;
        exit(1);
    }
    
    std::cout << std::endl;
}

// read filepath, send to client
int get(int conn_fd, std::string version, std::string filepath, bool keep_alive) {
    
    // read file
    std::ifstream file_to_send(filepath);
    if(!file_to_send) {
        std::string err_body = "<html><body text='red'>404 Not Found Reason URL does not exist: " + filepath + "<body></html>";
        std::string not_found = version + " 404 Not Found\r\nContent-Type: text/html\r\nContent-Length:  " + std::to_string(err_body.length()) + "\r\n\r\n" + err_body;
        write(conn_fd, not_found.c_str(), not_found.length()*sizeof(char));
        return 1;
    }
    
    file_to_send.seekg(0);
    std::string file_str((std::istreambuf_iterator<char>(file_to_send)),
                         std::istreambuf_iterator<char>());
    file_to_send.close();
    
    std::string content_len = std::to_string(file_str.length());
    
    // get response content type
    std::string file_handle = filepath.substr(filepath.find_last_of(".")+1);
    std::string content_type;
    if(file_handle == "html") {
        content_type = HTML_TEXT;
    }
    else if(file_handle == "txt") {
        content_type = TXT_TEXT;
    }
    else if(file_handle == "png") {
        content_type = PNG_TEXT;
    }
    else if(file_handle == "gif") {
        content_type = GIF_TEXT;
    }
    else if(file_handle == "jpg") {
        content_type = JPG_TEXT;
    }
    else if(file_handle == "css") {
        content_type = CSS_TEXT;
    }
    else if(file_handle == "js") {
        content_type = JS_TEXT;
    }
    else {
        std::string err_body = "<html><body>501 Not Implemented File Type: \"" + file_handle + "\" </body></html>";
        content_type = version + " 501 Not Imeplemented Error\r\nContent-Type: text/html\r\nContent-Length: " + std::to_string(err_body.length()) + "\r\n\r\n" + err_body;
        write(conn_fd, content_type.c_str(), content_type.length()*sizeof(char));
        return 1;
    }
    
    // add timestamp to resonse
    std::chrono::system_clock::time_point now = std::chrono::system_clock::now();
    time_t tt;
    tt = std::chrono::system_clock::to_time_t (now);
    
    std::string response_str;
    if(keep_alive) {
        response_str = version + " 200 Document Follows\r\n" + ctime(&tt) + "Content-Type: " + content_type + "\r\nContent-Length: " + content_len + "\r\nConnection: Keep-alive\r\n\r\n" + file_str + "\n";
    }
    else {
        response_str = version + " 200 Document Follows\r\n" + ctime(&tt) + "Content-Type: " + content_type + "\r\nContent-Length: " + content_len + "\r\n\r\n" + file_str + "\n";
    }
    
    int bytes = write(conn_fd, response_str.c_str(), response_str.length()*sizeof(char));
    std::cout << "wrote " << bytes << " bytes." << std::endl;
    return 0;
}

void connection_handler(int conn_fd, int timeout){
    
    if(timeout) {
        struct timeval tv;
        tv.tv_sec = timeout;
        setsockopt(conn_fd, SOL_SOCKET, SO_RCVTIMEO, (const char *) &tv, sizeof(tv));
    }
    
    while(1) {
        char read_buf[BUFSIZE];
        bzero(read_buf, sizeof(read_buf));
        
        int bytes_read = read(conn_fd, read_buf, BUFSIZE);
        if(bytes_read <= 0) {
            if (errno == EWOULDBLOCK) {
				std::cout << "closing connection because of request timeout" << std::endl;
            }
            else {
                std::string err_body = "<html><body text='red'>500 Internal Server Error</body></html>";
                std::string int_err = "HTTP/1.1 500 Internal Server Error\r\nContent-Type: text html\r\nContent-Length: " + std::to_string(err_body.length()) + "\r\n\r\n" + err_body;
                write(conn_fd, int_err.c_str(), int_err.length()*sizeof(char));
            }
            request_cleanup(conn_fd);
            return;
        }
        
        // parse header
        std::stringstream recv_data(read_buf);
        
        // request line
        std::string req_line;
        std::getline(recv_data, req_line);
		std::cout << "request: " << req_line << std::endl;
        std::stringstream elements(req_line);
        
        std::string cmd, uri, version, pipeline;
        elements >> cmd;
        elements >> uri;
        elements >> version;
        
        // validate the command and version
        if (cmd != "GET" or (version != "HTTP/1.0" and version != "HTTP/1.1")){
            std::string err_body = "<html><body>400 Bad Request</body></html>";
            std::string bad_req_out = "HTTP/1.1 400 Bad Request\r\nContent-Type: text/html\r\nContent-Length: " + std::to_string(err_body.length()) + "\r\n\r\n" + err_body;
            write(conn_fd, bad_req_out.c_str(), bad_req_out.length()*sizeof(char));
            request_cleanup(conn_fd);
            return;
        }
        
        // validate Hostname
        if (!std::regex_match(uri, std::regex(HOSTNAME_REGEX))) { // check if they gave us an IP address
            std::cout << "URI given is not a valid Hostname" << std::endl;
            return;
        }
        
        
        // cache check / blacklist check
            // resolve to ip or get from cache
            // add to cache if not in
        
        // check both hostname and IP in blacklist
        
        
    
            
        
        /*
        std::cout << "CMD:" << cmd << std::endl;
        std::cout << "URI:" <<uri << std::endl;
        std::cout << "Version:" << version << std::endl;
        */
        
        if(cmd == "GET") { // skeleton for if we need to add more commands in the future
            int get_err = get(conn_fd, version, filepath, keep_alive);
            if(get_err) {
                std::cout << "GET exited with error" << std::endl;
                request_cleanup(conn_fd);
                return;
            }
        }
    }
    request_cleanup(conn_fd);
}

int main(int argc, char*argv[]) {
    
	// arg parsing and error checking

    if(argc != 2 and argc != 3) {
        std::cout << USAGE << std::endl;
        return 1;
    }
    
    int port;
    try {
        port = std::stoi(argv[1]);
        
    }
    catch(std::exception& e)
    {
        std::cout << "<port> must be an integer" << '\n';
        return 1;
    }
    if(port <= 1024) {
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
    
    struct sockaddr_in servaddr;
    int conn_fd;
    /*
    sock_fd = socket(AF_INET, SOCK_STREAM, 0);
    
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htons(INADDR_ANY);
    servaddr.sin_port = htons(port);
    
    bind(sock_fd,(struct sockaddr *) &servaddr, sizeof(servaddr));
    listen(sock_fd, 128);
    */
    if (timeout) {
        std::cout << "Started proxy with:\n" << "host - localhost\n" << "port - "<< port << "\ntimeout - " << timeout << std::endl << std::endl;
    }
    else {
        std::cout << "Started proxy with:\n" << "host - localhost\n" << "port - "<< port << std::endl << std::endl;
    }
    
    
    while(!exit_requested) {
        conn_fd = accept(sock_fd,(struct sockaddr*) NULL, NULL);
        std::thread server_worker(connection_handler, conn_fd, doc_root);
        server_worker.join();
    }
    close(sock_fd);
    exit(0);
}

