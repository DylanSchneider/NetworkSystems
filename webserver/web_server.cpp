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

// progrma specifics
#define USAGE "./webserver <port> </path/to/doc root>"
#define BUFSIZE 1028

// text headers
#define HTML_TEXT "text/html"
#define TXT_TEXT "text/plain"
#define PNG_TEXT "image/png"
#define GIF_TEXT "image/gif"
#define JPG_TEXT "image/jpg"
#define CSS_TEXT "text/css"
#define JS_TEXT "application/javascript"
int sock_fd = -1; // global so signal handler can close socket

void signal_handler(int signum) {
    std::cout << "Received signal, closing socket and exiting." << std::endl;
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

// write post data to filepath
int post(int conn_fd, std::string version, std::string filepath, bool keep_alive, std::string post_data) {
    
    // read file
    std::ifstream file_to_edit(filepath);
    if(!file_to_edit) {
        std::string err_body = "<html><body text='red'>404 Not Found Reason URL does not exist: " + filepath + "<body></html>";
        std::string not_found = version + " 404 Not Found\r\nContent-Type: text/html\r\nContent-Length:  " + std::to_string(err_body.length()) + "\r\n\r\n" + err_body;
        write(conn_fd, not_found.c_str(), not_found.length()*sizeof(char));
        return 1;
    }
    
    // only implemented for html files
    std::string file_handle = filepath.substr(filepath.find_last_of(".")+1);
    if(file_handle != "html"){
        std::string err_body = "<html><body>501 Not Implemented File Type: \"" + file_handle + "\" </body></html>";
        std::string content_type = version + " 501 Not Imeplemented Error\r\nContent-Type: text/html\r\nContent-Length: " + std::to_string(err_body.length()) + "\r\n\r\n" + err_body;
        write(conn_fd, content_type.c_str(), content_type.length()*sizeof(char));
        return 1;
    }
    
    // write post data to top of new file
    std::string tmpfile_name = "tmp.html";
    std::ofstream tmpfile (tmpfile_name);
    tmpfile << "<html>\n\t<body>\n\t\t<pre>\n\t\t\t<h1>\n\t\t\t\t" << post_data << "\n\t\t\t</h1>\n\t\t</pre>\n\t</body>\n</html>\n";
    
    std::string line;
    while(getline(file_to_edit, line)) {
        tmpfile << line + "\n";
    }
    tmpfile.close();
    file_to_edit.close();
    
    // unlink original file, move tmpfile to orignal name
    std::remove(filepath.c_str());
    std::rename(tmpfile_name.c_str(), filepath.c_str());
    
    // send back contents of new file to client
    int get_err = get(conn_fd, version, filepath, keep_alive);
    if(get_err) {
        return 2;
    }
    return 0;
}

void connection_handler(int conn_fd, std::string doc_root){
    
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
        
        // request command line
        std::string req_line;
        std::getline(recv_data, req_line);
		std::cout << "request: " << req_line << std::endl;
        std::stringstream elements(req_line);
        std::string cmd, uri, version, pipeline;
        elements >> cmd;
        elements >> uri;
        elements >> version;
        
        // host line
        std::string host_line;
        std::getline(recv_data, host_line);
        std::string host = host_line.substr(host_line.find(" ") + 1);
        
        // connection line
        std::string connection_line;
        std::getline(recv_data, connection_line);
        std::string conn = connection_line.substr(connection_line.find(" ") + 1);
        bool keep_alive = false;
        std::cout << "CONN: " << conn << std::endl;
        if(conn == "Keep-alive\r") {
			std::cout << "Connection: Keep-alive" << std::endl;
            keep_alive = true;
        }
        
        if(keep_alive) {
            struct timeval tv;
            tv.tv_sec = 10;  // 10 sec timeout
            setsockopt(conn_fd, SOL_SOCKET, SO_RCVTIMEO, (const char *) &tv, sizeof(tv));
        }
        
        
        if((cmd != "GET" and cmd != "POST") or (version != "HTTP/1.0" and version != "HTTP/1.1")){
            std::string err_body = "<html><body>400 Bad Request</body></html>";
            std::string bad_req_out = "HTTP/1.1 400 Bad Request\r\nContent-Type: text/html\r\nContent-Length: " + std::to_string(err_body.length()) + "\r\n\r\n" + err_body;
            write(conn_fd, bad_req_out.c_str(), bad_req_out.length()*sizeof(char));
            request_cleanup(conn_fd);
            return;
        }
        
        /*
        std::cout << "CMD:" << cmd << std::endl;
        std::cout << "URI:" <<uri << std::endl;
        std::cout << "Version:" <<version << std::endl;
        */
        
        if(doc_root.back() == '/') {
            doc_root.erase(doc_root.size()-1);
        }
        
        std::string filepath;
        if(uri == "/") {
            filepath = doc_root + "/index.html";
        }
        else {
            filepath = doc_root + uri;
        }
        
        if(cmd == "GET") {
            int get_err = get(conn_fd, version, filepath, keep_alive);
            if(get_err) {
                std::cout << "GET exited with error" << std::endl;
                request_cleanup(conn_fd);
                return;
            }
        }
        else if(cmd == "POST") {
            // get the post contents
            std::string blank_line;
            std::getline(recv_data, blank_line); // should be blank
            
            std::string post_data;
            std::string post_line;
            while(std::getline(recv_data, post_line)) { // should be POST data
                post_data += post_line + "\n";
            }
            
            int post_err = post(conn_fd, version, filepath, keep_alive, post_data);
            if(post_err) {
                switch(post_err) {
                    case 1:
                        std::cout << "POST exited with error" << std::endl;
                        break;
                    case 2:
                        std::cout << "POST exited while sending data back to client (GET error)" << std::endl;
                        break;
                }
                request_cleanup(conn_fd);
                return;
            }
            
        }

        if(!keep_alive) {
        	request_cleanup(conn_fd);
			return;
        }
        std::cout << std::endl;
    }
}

int main(int argc, char*argv[]) {
    
	// arg parsing and error checking

    if(argc != 3) {
        std::cout << USAGE << std::endl;
        return 1;
    }
    
    int port;
    try {
        port = std::stoi(argv[1]);
    }
    catch(std::exception& e)
    {
        std::cout << e.what() << '\n';
        return 1;
    }
    if(port <= 1024) {
        std::cout << "port must be greater than 1024" << std::endl;
        return 1;
    }
    
    std::string doc_root = argv[2];
    struct stat stat_buf;
    if(stat(doc_root.c_str(), &stat_buf) != 0) {
        std::cout << "'" << doc_root << "' does not exist." << std::endl;
        return 1;
    }
	
	// setup
    
    signal(SIGINT, signal_handler);
    
    struct sockaddr_in servaddr;
    
    sock_fd = socket(AF_INET, SOCK_STREAM, 0);
    
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htons(INADDR_ANY);
    servaddr.sin_port = htons(port);
    
    bind(sock_fd,(struct sockaddr *) &servaddr, sizeof(servaddr));
    listen(sock_fd, 128);
    
	std::cout << "Started web server with:\n" << "host - localhost\n" << "port - "<< port << "\ndoc root - " << doc_root << std::endl << std::endl;
    
    int conn_fd;
    while(1) {
        conn_fd = accept(sock_fd,(struct sockaddr*) NULL, NULL);
        std::thread server_worker(connection_handler, conn_fd, doc_root);
        server_worker.join();
    }
    close(sock_fd);
    exit(0);
}

