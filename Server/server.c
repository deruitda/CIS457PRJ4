//Danny DeRuiter

#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <netinet/in.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/sendfile.h>

/**************************************
 * Server
 *************************************/

int main(int argc, char **argv)
{
        int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
        int port;
        int clientsocket;
		char filename[20];
		FILE *fp;
		
        //get the port
        printf("Enter a port: ");
        scanf("%d", &port);
        //server is specifying its own address
        struct sockaddr_in serveraddr, clientaddr;
        serveraddr.sin_family = AF_INET;
        serveraddr.sin_port = htons(port);
        serveraddr.sin_addr.s_addr = INADDR_ANY; //a catchall saying use any IP addr this computer has

        //Bind: Tie addr to socket saying when something is to be done with socket, use this addr
        bind(sockfd, (struct sockaddr*)&serveraddr, sizeof(serveraddr));

        //listens to specified socket
        //10 is number of turns until we stop listening
        listen(sockfd, 10);

        int len = sizeof(clientaddr);

        //Accept will fill in client addr struct
        //Accept is a blocking call
		//clientsocket = accept(sockfd, (struct sockaddr*)&clientaddr, &len);
		//if(clientsocket == -1)
		//{
		//	fprintf(stderr, "could not accept client connection...\n");
		//	exit(1);
		//}
		//printf("connection established...\n");
		
		//get file size
		memset(filename, 0, 20);
        //recv(clientsocket, filename, sizeof(filename), 0);
		recv(sockfd, filename, sizeof(filename), 0);
		printf("Received: %s\n", filename);
		if((fp = fopen(filename, "r")) == NULL)
		{
			printf("file not found\n");
		}
		printf("file %s found\n", filename);
		int size;
		fseek(fp, 0, SEEK_END);
		size = ftell(fp);
		fseek(fp, 0, SEEK_SET);
		printf("file size: %i\n", size);
		
		//send file size
		printf("sending file size...\n");
		write(sockfd, &size, sizeof(size));
		
		//Send file as array of bytes
		printf("sending picture as byte array\n");
		char buff[size];
		while(!feof(fp)) 
		{
			fread(buff, 1, sizeof(buff), fp);
			write(clientsocket, buff, sizeof(buff));
			bzero(buff, sizeof(buff));
		}
		
        close(sockfd);
        printf("Connection closed\n");
		return 0;
}
