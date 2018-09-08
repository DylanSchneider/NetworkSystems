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
/* You will have to modify the program below */

#define MAXBUFSIZE 100

int main (int argc, char * argv[] )
{


	int sock;                           //This will be our socket
	struct sockaddr_in sin, remote;     //"Internet socket address structure"
	socklen_t remote_size;         //length of the sockaddr_in structure
	int nbytes;                        //number of bytes we receive in our message
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

	remote_size = sizeof(remote);
	
	for(;;) {
		//waits for an incoming message
		bzero(buffer,sizeof(buffer));
		nbytes = recvfrom(sock, buffer, MAXBUFSIZE, 0, (struct sockaddr*) &remote, &remote_size);

		printf("The client says %s\n", buffer);
        
        if (strcmp(buffer, "ls") == 0)
        {
            FILE *fp;
            char output[1035];
            
            if ((fp = popen("/bin/ls", "r")) == NULL)
            {
                printf("Failed to run ls.\n" );
                exit(1);
            }
            
            while (fgets(output, sizeof(output)-1, fp) != NULL) {
                nbytes = sendto(sock, output, sizeof(output), 0, (struct sockaddr*) &remote, remote_size);
            }
            char msg[] = "-1";
            nbytes = sendto(sock, msg, sizeof(msg), 0, (struct sockaddr*) &remote, remote_size);
            
        }
        
        else if (strcmp(buffer, "exit") == 0)
        {
            char msg[] = "Exiting server...";
            nbytes = sendto(sock, msg, sizeof(msg), 0, (struct sockaddr*) &remote, remote_size);
            close(sock);
            exit(0);
        }
        
        else if (strstr(buffer, "get ") != NULL)
        {
            char *cmd = strtok(buffer, " ");
            char *filename = strtok(NULL, " ");
            
            int file;
            int bytes;
            printf("Attempting to open %s\n", filename);
            if ((file = open(filename, O_RDONLY)) < 0)
            {
                printf("Unable to open %s\n", filename);
                char msg[MAXBUFSIZE];
                strcpy(msg, "Unable to open ");
                strcat(msg, filename);
                sendto(sock, msg, sizeof(msg), 0, (struct sockaddr*) &remote, remote_size);
                continue;
            }
            
            char buf[MAXBUFSIZE];
            while ((bytes = read(file, buf, MAXBUFSIZE)) > 0)
            {
                
                sendto(sock, buf, bytes, 0, (struct sockaddr*) &remote, remote_size);
                printf("\tSent %d bytes\n", bytes);
            }
            printf("Done sending %s\n", filename);
            char msg[] = "-1";
            nbytes = sendto(sock, msg, sizeof(msg), 0, (struct sockaddr*) &remote, remote_size);
            close(file);
        }
        
        else if (strstr(buffer, "put ") != NULL)
        {
            char copy[MAXBUFSIZE];
            strcpy(copy, buffer);
            char *cmd = strtok(copy, " ");
            char *filename = strtok(NULL, " ");

            int file;
            if ((file = open(filename, O_WRONLY|O_CREAT)) < 0)
            {
                char msg[MAXBUFSIZE];
                strcpy(msg, "Could not open ");
                strcat(msg, filename);
                nbytes = sendto(sock, msg, sizeof(msg), 0, (struct sockaddr*) &remote, remote_size);
                continue;
            }
            
            char received[MAXBUFSIZE];
            for (;;)
            {
                if (recvfrom(sock, received, sizeof(received), 0, (struct sockaddr*) &remote, &remote_size) == -1)
                {
                    printf("error receiving message");
                    exit(1);
                }
                else if (strcmp(received, "-1") == 0)
                {
                    break;
                }
                write(file, received, sizeof(received));
            }
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
            
            nbytes = sendto(sock, msg, sizeof(msg), 0, (struct sockaddr*) &remote, remote_size);
        }
        printf("\n");


	}
	close(sock);
}

