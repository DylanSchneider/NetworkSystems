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
#include <errno.h>

#define MAXBUFSIZE 100
#define PROMPT "> "
#define eof "-1"

// Uncomment for debugging:
// #define DEBUG

void print_menu();

int main (int argc, char * argv[])
{

	int sock;                               //this will be our socket
    struct sockaddr_in remote;              //"Internet socket address structure"
    socklen_t remote_size = sizeof(remote);
    
    int file;
    int send_bytes, receive_bytes, read_bytes, write_bytes ; // bytes for sending, receiving, reading and writing
	char buffer[MAXBUFSIZE];
    char menu_option[MAXBUFSIZE];
    char received[MAXBUFSIZE];
    
	if (argc < 3)
	{
        printf("USAGE: ./client <server_ip> <server_port>\n");
		exit(1);
	}

	/******************
	  Here we populate a sockaddr_in struct with
	  information regarding where we'd like to send our packet 
	  i.e the Server.
	 ******************/
	bzero(&remote,sizeof(remote));               //zero the struct
	remote.sin_family = AF_INET;                 //address family
	remote.sin_port = htons(atoi(argv[2]));      //sets port to network byte order
	remote.sin_addr.s_addr = inet_addr(argv[1]); //sets remote IP address

	//Causes the system to create a generic socket of type UDP (datagram)
	if ((sock = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
	{
		printf("unable to create socket");
	}
    
    printf("Client started with connection to %s on port %s\n", argv[1], argv[2]);
    print_menu();
    
    for (;;)
    {
        bzero(menu_option, sizeof(menu_option));
        bzero(received, sizeof(received));
        
        printf(PROMPT);
        fgets(menu_option, MAXBUFSIZE, stdin);
        
        if ((strlen(menu_option) > 0) && (menu_option[strlen(menu_option) - 1] == '\n'))
        {
            menu_option[strlen(menu_option) - 1] = '\0';
        }
        
        if (strlen(menu_option) == 0)
        {
            continue;
        }
        else if (strcmp(menu_option, "menu") == 0)
        {
            print_menu();
        }
        
        else if (strcmp(menu_option, "ls") == 0)
        {
            send_bytes = sendto(sock, menu_option, strlen(menu_option), 0, (struct sockaddr*) &remote, remote_size) == -1;
            if (send_bytes == -1)
            {
                printf("error sending message\n");
                exit(1);
            }
#ifdef DEBUG
            printf("sent begin msg with size of %d bytes\n", send_bytes);
#endif
            for (;;)
            {
                receive_bytes = recvfrom(sock, received, sizeof(received), 0, (struct sockaddr*) &remote, &remote_size);
                if (receive_bytes == -1)
                {
                    printf("error receiving message\n");
                    exit(1);
                }
                
                if (strcmp(received, eof) == 0)
                {
                    break;
                }
#ifdef DEBUG
                printf("%s: received %d bytes\n", menu_option, receive_bytes);
#endif
                printf("%s", received);
                memset(received, 0, MAXBUFSIZE);
            }
        }
        
        else if (strcmp(menu_option, "exit") == 0)
        {
            send_bytes = sendto(sock, menu_option, strlen(menu_option), 0, (struct sockaddr*) &remote, remote_size);
            if (send_bytes == -1)
            {
                printf("error sending message\n");
                exit(1);
            }
#ifdef DEBUG
            printf("%s: sent %d bytes\n", menu_option, send_bytes);
#endif
            printf("Exiting server\n");
            printf("Exiting client\n");
            close(sock);
            exit(0);
        }
        
        else if (strstr(menu_option, "get ") != NULL)
        {
            char copy[MAXBUFSIZE];
            strcpy(copy, menu_option);
            char *cmd = strtok(copy, " ");
            char *filename = strtok(NULL, " ");
            if (filename == NULL)
            {
                printf("Must provide a filename\n");
                continue;
            }
            
            send_bytes = sendto(sock, menu_option, strlen(menu_option), 0, (struct sockaddr*) &remote, remote_size);
            if (send_bytes == -1)
            {
                printf("error sending message\n");
                exit(1);
            }
#ifdef DEBUG
            printf("%s: sent %d bytes\n", cmd, send_bytes);
#endif
            
            receive_bytes = recvfrom(sock, received, sizeof(received), 0, (struct sockaddr*) &remote, &remote_size);
            if (receive_bytes == -1)
            {
                printf("error receiving message\n");
                exit(1);
            }
#ifdef DEBUG
            printf("%s: received %d bytes\n", cmd, receive_bytes);
#endif
            printf("%s\n", received);
            
            char err_received[MAXBUFSIZE];
            strcpy(err_received, "Unable to open ");
            strcat(err_received, filename);
            if (strstr(received, err_received) != NULL)
            {
                continue;
            }
            printf("receiving...\n");
            
            int file;
            if ((file = open(filename, O_RDWR|O_CREAT, 0666)) < 0)
            {
                printf("couldnt open %s for writing.\n", filename);
                continue;
            }
            for (;;)
            {
                receive_bytes = recvfrom(sock, received, sizeof(received), 0, (struct sockaddr*) &remote, &remote_size);
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
        
        else if (strstr(menu_option, "put ") != NULL)
        {
            char copy[MAXBUFSIZE];
            strcpy(copy, menu_option);
            char *cmd = strtok(copy, " ");
            char *filename = strtok(NULL, " ");
            if (filename == NULL)
            {
                printf("Must provide a filename\n");
                continue;
            }
            
            if ((file = open(filename, O_RDONLY)) < 0)
            {
                printf("Unable to open %s\n", filename);
                continue;
            }
            printf("Successfully opened %s\n", filename);
            send_bytes = sendto(sock, menu_option, strlen(menu_option), 0, (struct sockaddr*) &remote, remote_size);
            if (send_bytes == -1)
            {
                printf("error sending message\n");
                exit(1);
            }
#ifdef DEBUG
            printf("%s: sent %d bytes\n", cmd, send_bytes);
#endif
            
            while ((read_bytes = read(file, buffer, MAXBUFSIZE)) > 0)
            {
#ifdef DEBUG
                printf("%s: read %d bytes\n", cmd, read_bytes);
#endif
                send_bytes = sendto(sock, buffer, read_bytes, 0, (struct sockaddr*) &remote, remote_size);
                if (send_bytes == -1)
                {
                    printf("error sending message\n");
                    exit(1);
                }
#ifdef DEBUG
                printf("%s: sent %d bytes\n", cmd, send_bytes);
#endif
                bzero(buffer, MAXBUFSIZE);
            }
            printf("Done sending %s\n", filename);
            char msg[] = eof;
            if (sendto(sock, msg, sizeof(msg), 0, (struct sockaddr*) &remote, remote_size) == -1)
            {
                printf("error sending message\n");
                exit(1);
            }
#ifdef DEBUG
            printf("%s: sent %d bytes\n", cmd, send_bytes);
#endif
            close(file);
        }
        
        else if (strstr(menu_option, "delete ") != NULL)
        {
            char copy[MAXBUFSIZE];
            strcpy(copy, menu_option);
            char *cmd = strtok(copy, " ");
            char *filename = strtok(NULL, " ");
            if (filename == NULL)
            {
                printf("Must provide a filename\n");
                continue;
            }
            
            send_bytes = sendto(sock, menu_option, strlen(menu_option), 0, (struct sockaddr*) &remote, remote_size);
            if (send_bytes == -1)
            {
                printf("error sending message\n");
                exit(1);
            }
#ifdef DEBUG
            printf("%s: sent %d bytes\n", cmd, send_bytes);
#endif
            receive_bytes = recvfrom(sock, received, sizeof(received), 0, (struct sockaddr*) &remote, &remote_size);
            if (receive_bytes == -1)
            {
                printf("error receiving message\n");
                exit(1);
            }
#ifdef DEBUG
            printf("%s: received %d bytes\n", cmd, receive_bytes);
#endif
            printf("%s\n", received);
        }
        else {
            printf("Invalid command, try again.\n");
        }
    }
	close(sock);
    return 0;
}

void print_menu()
{
    printf("Command Menu\n");
    printf("****************************************************\n");
    printf("menu - show list of client commands\n");
    printf("get <filename> - get the file from the server\n");
    printf("put <filename> - put the file on the server\n");
    printf("delete <filename> \n");
    printf("ls \n");
    printf("exit\n");
    printf("****************************************************\n");
}
