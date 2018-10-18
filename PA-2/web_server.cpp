#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <iostream>
#include <string>

#define USAGE "./webserver <port> </path/to/doc root>"

void connection_handler(){
    
}

int main(int argc, char*argv[]) {
    
    if (argc != 3) {
        std::cout << USAGE << std::endl;
        return 1;
    }
    
    int port;
    try {
        port = std::stoi(argv[1]);
    }
    catch (std::exception& e)
    {
        std::cout << e.what() << '\n';
        return 1;
    }
    if (port <= 1024) {
        std::cout << "port must be greater than 1024" << std::endl;
        return 1;
    }
    
    std::string doc_root = argv[2];
    try{
        
    }
    
    struct sockaddr_in servaddr;
    int sock_fd, conn_fd;
    
    sock_fd = socket(AF_INET, SOCK_STREAM, 0);
    
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htons(INADDR_ANY);
    servaddr.sin_port = htons(port);
    
    bind(sock_fd, (struct sockaddr *) &servaddr, sizeof(servaddr));
    listen(sock_fd, 128);
    
    while(1){
        conn_fd = accept(sock_fd, (struct sockaddr*) NULL, NULL);
        std::thread server_worker (connection_handler)
        server_worker.join()
    }
    



}
