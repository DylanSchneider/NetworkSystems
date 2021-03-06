#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <signal.h>
#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/time.h>
#include <stdlib.h>
#include <memory.h>
#include <string.h>

#define MAXBUFSIZE 100
#define eof "-1"

// Uncomment for debugging:
// #define DEBUG

int main (int argc, char * argv[] )
{
	int sock;                           //This will be our socket
	struct sockaddr_in sin, remote;     //"Internet socket address structure"
    socklen_t remote_size = sizeof(remote); //length of the sockaddr_in structure
    
    int file;
    int send_bytes, receive_bytes, read_bytes, write_bytes ; // bytes for sending, receiving, reading and writing
	char buffer[MAXBUFSIZE];             //a buffer to store our received message
	if (argc != 2)
	{
        printf ("USAGE: ./server <port>\n");
		exit(1);
	}

	/******************
	  This code populates the sockaddr_in struct with
	  the information about our socket
	 ******************/
	bzero(&sin,sizeof(sin));                    //zero the struct
	sin.sin_family = AF_INET;                   //address family
	sin.sin_port = htons(atoi(argv[1]));        //htons() sets the port # to network byte order
	sin.sin_addr.s_addr = INADDR_ANY;           //supplies the IP address of the local machine

	//Causes the system to create a generic socket of type UDP (datagram)
	if ((sock = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
	{
		printf("unable to create socket");
        exit(1);
	}

	/******************
	  Once we've created a socket, we must bind that socket to the 
	  local address and port we've supplied in the sockaddr_in struct
	 ******************/
	if (bind(sock, (struct sockaddr *)&sin, sizeof(sin)) < 0)
	{
		printf("unable to bind socket\n");
        exit(1);
	}
    
    printf("Started server on port %s\n\n", argv[1]);

	for(;;)
    {
		bzero(buffer,sizeof(buffer));
		receive_bytes = recvfrom(sock, buffer, MAXBUFSIZE, 0, (struct sockaddr*) &remote, &remote_size);
        if (receive_bytes == -1)
        {
            printf("error receiving message\n");
            exit(1);
        }
#ifdef DEBUG
        printf("received begin msg with size of %d\n", receive_bytes);
#endif
        
		printf("The client says %s\n", buffer);
        
        if (strcmp(buffer, "ls") == 0)
        {
            FILE *fp;
            char output[MAXBUFSIZE];
            if ((fp = popen("/bin/ls", "r")) == NULL)
            {
                printf("Failed to run ls.\n" );
                exit(1);
            }
            
            while (fgets(output, MAXBUFSIZE, fp) != NULL) {
#ifdef DEBUG
                printf("%s: read %d bytes\n", cmd, strlen(output));
#endif
                send_bytes = sendto(sock, output, strlen(output), 0, (struct sockaddr*) &remote, remote_size);
                if (send_bytes == -1)
                {
                    printf("error sending message\n");
                    exit(1);
                }
#ifdef DEBUG
                printf("%s: sent %d bytes\n", cmd, send_bytes);
#endif

            }
            // send eof message to stop receiving
            char msg[] = eof;
            send_bytes = sendto(sock, msg, sizeof(msg), 0, (struct sockaddr*) &remote, remote_size);
            if (send_bytes == -1)
            {
                printf("error sending message\n");
                exit(1);
            }
#ifdef DEBUG
            printf("%s: sent %d bytes\n", cmd, send_bytes);
#endif
            printf("successfully sent ls\n");
        }
        
        else if (strcmp(buffer, "exit") == 0)
        {
            printf("Exiting\n");
            close(sock);
            exit(0);
        }
        
        else if (strstr(buffer, "get ") != NULL)
        {
            char *cmd = strtok(buffer, " ");
            char *filename = strtok(NULL, " ");
            
            printf("Attempting to open %s\n", filename);
            char begin_msg[MAXBUFSIZE];
            if ((file = open(filename, O_RDONLY)) < 0)
            {
                strcpy(begin_msg, "Unable to open ");
                strcat(begin_msg, filename);
                printf("Unable to open %s, moving on.\n\n", filename);
                sendto(sock, begin_msg, sizeof(begin_msg), 0, (struct sockaddr*) &remote, remote_size);
                if (send_bytes == -1)
                {
                    printf("error sending message\n");
                    exit(1);
                }
#ifdef DEBUG
                printf("%s: sent %d bytes\n", cmd, send_bytes);
#endif
                continue;
            }
            else{
                strcpy(begin_msg, "Successfully opened ");
                strcat(begin_msg, filename);
                send_bytes = sendto(sock, begin_msg, sizeof(begin_msg), 0, (struct sockaddr*) &remote, remote_size);
                if (send_bytes == -1)
                {
                    printf("error sending message\n");
                    exit(1);
                }
#ifdef DEBUG
                printf("%s: sent %d bytes\n", cmd, send_bytes);
#endif
                printf("Successfully opened %s, sending...\n", filename);
            }
            
            char buf[MAXBUFSIZE];
            while ((read_bytes = read(file, buf, MAXBUFSIZE)) > 0)
            {
#ifdef DEBUG
                printf("%s: read %d bytes\n", cmd, read_bytes);
#endif
                send_bytes = sendto(sock, buf, read_bytes, 0, (struct sockaddr*) &remote, remote_size);
                if (send_bytes == -1)
                {
                    printf("error sending message\n");
                    exit(1);
                }
#ifdef DEBUG
                printf("%s: sent %d bytes\n", cmd, send_bytes);
#endif
            }
            char eof_msg[] = eof;
            send_bytes = sendto(sock, eof_msg, sizeof(eof_msg), 0, (struct sockaddr*) &remote, remote_size);
            if (send_bytes == -1)
            {
                printf("error sending message\n");
                exit(1);
            }
#ifdef DEBUG
            printf("%s: sent %d bytes\n", cmd, send_bytes);
#endif
            
            printf("Done sending %s\n", filename);
            close(file);
        }
        
        else if (strstr(buffer, "put ") != NULL)
        {
            char copy[MAXBUFSIZE];
            strcpy(copy, buffer);
            char *cmd = strtok(copy, " ");
            char *filename = strtok(NULL, " ");

            if ((file = open(filename, O_RDWR|O_CREAT, 0666)) < 0)
            {
                char msg[MAXBUFSIZE];
                strcpy(msg, "Could not open ");
                strcat(msg, filename);
                send_bytes = sendto(sock, msg, sizeof(msg), 0, (struct sockaddr*) &remote, remote_size);
                if (send_bytes == -1)
                {
                    printf("error sending message\n");
                    exit(1);
                }
#ifdef DEBUG
                printf("%s: sent %d bytes\n", cmd, send_bytes);
#endif
                continue;
            }
            
            char received[MAXBUFSIZE];
            for(;;)
            {
                receive_bytes = recvfrom(sock, received, MAXBUFSIZE, 0, (struct sockaddr*) &remote, &remote_size);
                if (receive_bytes == -1)
                {
                    printf("error receiving message\n");
                    exit(1);
                }
#ifdef DEBUG
                printf("%s: received %d bytes\n", cmd, receive_bytes);
#endif
                if (strcmp(received, eof) == 0)
                {
                    break;
                }
                write_bytes = write(file, received, receive_bytes);
#ifdef DEBUG
                printf("wrote %d bytes to %s\n", write_bytes, filename);
#endif
            }
            printf("Successfully wrote %s\n", filename);
            close(file);
        }
        
        else if (strstr(buffer, "delete ") != NULL)
        {
            char *cmd = strtok(buffer, " ");
            char *filename = strtok(NULL, " ");
            
            int delete = unlink(filename);
            char msg[MAXBUFSIZE];
            if (delete == 0){
                strcpy(msg, "Successfully deleted ");
                strcat(msg, filename);
            }
            else {
                strcpy(msg, "Failed to delete ");
                strcat(msg, filename);
            }
            
            send_bytes = sendto(sock, msg, sizeof(msg), 0, (struct sockaddr*) &remote, remote_size);
            if (send_bytes== -1)
            {
                printf("error sending message\n");
                exit(1);
            }
#ifdef DEBUG
            printf("%s: sent %d bytes\n", cmd, send_bytes);
#endif
            printf("Successfully deleted %s\n", filename);
        }
        
        printf("\n");


	}
	close(sock);
}

