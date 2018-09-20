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
    
    int sbytes, rbytes;                     // bytes for sending and receiving
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
        //bzero(received, sizeof(received));
        
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
            sbytes = sendto(sock, menu_option, strlen(menu_option), 0, (struct sockaddr*) &remote, remote_size) == -1);
            if (sbytes == -1)
            {
                printf("error sending message\n");
                exit(1);
            }
#ifdef DEBUG
            printf("sent %d bytes\n", sbytes);
#endif
            for (;;)
            {
                rbytes = recvfrom(sock, received, sizeof(received), 0, (struct sockaddr*) &remote, &remote_size);
                if (rbytes == -1)
                {
                    printf("error receiving message\n");
                    exit(1);
                }
                
                if (strcmp(received, eof) == 0)
                {
                    break;
                }
#ifdef DEBUG
                printf("received %d bytes\n", rbytes);
#endif
                printf("%s", received);
                memset(received, 0, MAXBUFSIZE);
            }
        }
        
        else if (strcmp(menu_option, "exit") == 0)
        {
            sbytes = sendto(sock, menu_option, strlen(menu_option), 0, (struct sockaddr*) &remote, remote_size) == -1);
            if (sbytes == -1)
            {
                printf("error sending message\n");
                exit(1);
            }
            rbytes = recvfrom(sock, received, sizeof(received), 0, (struct sockaddr*) &remote, &remote_size) == -1);
            if (rbytes == -1)
            {
                printf("error receiving message\n");
                exit(1);
            }
            printf("%s\n", received);
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
            
            if (sendto(sock, menu_option, strlen(menu_option), 0, (struct sockaddr*) &remote, remote_size) == -1)
            {
                printf("error sending message\n");
                exit(1);
            }
            if (recvfrom(sock, received, sizeof(received), 0, (struct sockaddr*) &remote, &remote_size) == -1)
            {
                printf("error receiving message\n");
                exit(1);
            }
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
                rbytes = recvfrom(sock, received, sizeof(received), 0, (struct sockaddr*) &remote, &remote_size);
                if (rbytes == -1)
                {
                    printf("error receiving message\n");
                    exit(1);
                }
                else if (strcmp(received, eof) == 0)
                {
                    break;
                }
                write(file, received, rbytes);
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
            
            int file;
            int bytes;
            if ((file = open(filename, O_RDONLY)) < 0)
            {
                printf("Unable to open %s\n", filename);
                continue;
            }
            printf("Successfully opened %s\n", filename);
            if (sendto(sock, menu_option, strlen(menu_option), 0, (struct sockaddr*) &remote, remote_size) == -1)
            {
                printf("error sending message\n");
                exit(1);
            }
            
            char buf[MAXBUFSIZE];
            while ((bytes = read(file, buf, MAXBUFSIZE)) > 0)
            {
                if (sendto(sock, buf, bytes, 0, (struct sockaddr*) &remote, remote_size) == -1)
                {
                    printf("error sending message\n");
                    exit(1);
                }
                bzero(buf, MAXBUFSIZE);
            }
            printf("Done sending %s\n", filename);
            char msg[] = eof;
            if (sendto(sock, msg, sizeof(msg), 0, (struct sockaddr*) &remote, remote_size) == -1)
            {
                printf("error sending message\n");
                exit(1);
            }
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
            
            if (sendto(sock, menu_option, strlen(menu_option), 0, (struct sockaddr*) &remote, remote_size) == -1)
            {
                printf("error sending message\n");
                exit(1);
            }
            if (recvfrom(sock, received, sizeof(received), 0, (struct sockaddr*) &remote, &remote_size) == -1)
            {
                printf("error receiving message\n");
                exit(1);
            }
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
