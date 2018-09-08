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

void print_menu();
int is_eof(char* buffer, int size);

int main (int argc, char * argv[])
{

	int nbytes;                             // number of bytes send by sendto()
	int sock;                               //this will be our socket
	char buffer[MAXBUFSIZE];
    char menu_option[MAXBUFSIZE];
    char received[MAXBUFSIZE];

	struct sockaddr_in remote;              //"Internet socket address structure"
    socklen_t remote_size = sizeof(remote);
    
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

	/******************
	  sendto() sends immediately.  
	  it will report an error if the message fails to leave the computer
	  however, with UDP, there is no error if the message is lost in the network once it leaves the computer.
	 ******************/
    print_menu();
    for (;;) {
        printf("> ");
        scanf(" %[^\n]", menu_option);
        
        if (strcmp(menu_option, "menu") == 0)
        {
            print_menu();
        }
        
        else if (strcmp(menu_option, "ls") == 0)
        {
            if (sendto(sock, menu_option, sizeof(menu_option), 0, (struct sockaddr*) &remote, remote_size) == -1)
            {
                printf("error sending message");
                exit(1);
            }
            for (;;)
            {
                if (recvfrom(sock, received, sizeof(received), 0, (struct sockaddr*) &remote, &remote_size) == -1)
                {
                    printf("error receiving message");
                    exit(1);
                }
                
                if (strcmp(received, "-1") == 0)
                {
                    break;
                }
                printf("%s", received);
                for (int i=0; i<MAXBUFSIZE; i++) {
                    received[i] = '\0';
                }
            }
        }
        
        else if (strcmp(menu_option, "exit") == 0)
        {
            if (sendto(sock, menu_option, sizeof(menu_option), 0, (struct sockaddr*) &remote, remote_size) == -1)
            {
                printf("error sending message");
                exit(1);
            }
            if (recvfrom(sock, received, sizeof(received), 0, (struct sockaddr*) &remote, &remote_size) == -1)
            {
                printf("error receiving message");
                exit(1);
            }
            printf("%s\n", received);
            close(sock);
            exit(0);
        }
        
        else if (strstr(menu_option, "get ") != NULL)
        {
            char copy[MAXBUFSIZE];
            strcpy(copy, menu_option);
            char *cmd = strtok(copy, " ");
            char *filename = strtok(NULL, " ");
            printf("FILE: %s\n", filename);
            if (sendto(sock, menu_option, sizeof(menu_option), 0, (struct sockaddr*) &remote, remote_size) == -1)
            {
                printf("error sending message");
                exit(1);
            }
            for (;;)
            {
                if (recvfrom(sock, received, sizeof(received), 0, (struct sockaddr*) &remote, &remote_size) == -1)
                {
                    printf("error receiving message");
                    exit(1);
                }
                if (strstr(received, "Unable to open") != NULL)
                {
                    printf("HI%s\n", received);
                    break;
                }
                /*else if (is_eof(received, sizeof(received)) == 0)
                {
                    printf("hit eof: %s\n", received);
                    break;
                }*/
                printf("%s", received);
                
            }
            
        }
        
        else if (strstr(menu_option, "put ") != NULL)
        {
            char copy[MAXBUFSIZE];
            strcpy(copy, menu_option);
            char *cmd = strtok(copy, " ");
            char *filename = strtok(NULL, " ");
            printf("MENU_OPTION:%s\n", menu_option);
            printf("cmd:%s\n", cmd);
            printf("name:%s\n", filename);
        }
        
        else if (strstr(menu_option, "delete ") != NULL)
        {
            if (sendto(sock, menu_option, sizeof(menu_option), 0, (struct sockaddr*) &remote, remote_size) == -1)
            {
                printf("error sending message");
                exit(1);
            }
            if (recvfrom(sock, received, sizeof(received), 0, (struct sockaddr*) &remote, &remote_size) == -1)
            {
                printf("error receiving message");
                exit(1);
            }
            printf("%s\n", received);
        }
        else {
            printf("Invalid command, try again.\n");
        }
        
        
        printf("\n\nEND LOOP1: %s\n\n", received);
        
        for (int i=0; i<MAXBUFSIZE; i++) {
            received[i] = '\0';
        }
        printf("\n\nEND LOOP2: %s\n\n", received);
        
    }
    
    
    /*
	char command[] = "apple";	
	nbytes = **** CALL SENDTO() HERE ****;

	// Blocks till bytes are received
	struct sockaddr_in from_addr;
	int addr_length = sizeof(struct sockaddr);
	bzero(buffer,sizeof(buffer));
	nbytes = **** CALL RECVFROM() HERE ****;  

	printf("Server says %s\n", buffer);
*/
	close(sock);
    return 0;

}

void print_menu()
{
    printf("Welcome to the Basic UDP Client. Valid commands are:\n");
    printf("menu - show list of client commands\n");
    printf("get <filename> - get the file from the server\n");
    printf("put <filename> - put the file on the server\n");
    printf("delete <filename> \n");
    printf("ls \n");
    printf("exit\n\n");
}

int is_eof(char* buffer, int size)
{
    int i;
    for (i=0; i<size; i++)
    {
        if (buffer[i] == EOF)
        {
            return 1;
        }
    }
    return 0;
}

