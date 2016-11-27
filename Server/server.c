//Danny DeRuiter

#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <limits.h>
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
    int port;
    char buf[LINE_MAX];
    FILE *fp;
    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);

    if(sockfd < 0)
    {
      printf("Error creating socket\n");
      return 1;
    }

    //get the port
    printf("Enter a port: ");
    scanf("%d", &port);
    //server is specifying its own address
    struct sockaddr_in serveraddr, clientaddr;
    serveraddr.sin_family = AF_INET;
    serveraddr.sin_port = htons(port);
    serveraddr.sin_addr.s_addr = INADDR_ANY; //a catchall saying use any IP addr this computer has

    //Bind: Tie addr to socket saying when something is to be done with socket, use this addr
    if(bind(sockfd, (struct sockaddr*)&serveraddr, sizeof(serveraddr)) != 0)
    {
      fprintf(stderr, "Bind");
      exit(1);
    }

    int len = sizeof(struct sockaddr_in);
		memset(buf, 0, sizeof(buf));
    //recv(clientsocket, filename, sizeof(filename), 0);
		recvfrom(sockfd, buf, sizeof(buf), 0, (struct sockaddr*)&serveraddr, &len);
    //buf[strlen(buf)-1] = '\0';
    char *filename = malloc(strlen(buf));
    memcpy(filename, buf, strlen(buf));
    printf("Received: %s\n", filename);
		if((fp = fopen(filename, "r")) == NULL)
		{
			printf("file not found\n");
      exit(1);
		}
		printf("file %s found\n", filename);
		int size;
		fseek(fp, 0, SEEK_END);
		size = ftell(fp);
		fseek(fp, 0, SEEK_SET);
		printf("file size: %i\n", size);

		//send file size
		printf("sending file size...\n");
		sendto(sockfd, &size, sizeof(size), 0, (struct sockaddr*)&serveraddr, sizeof(struct sockaddr_in));

		//Send file as array of bytes
		printf("sending picture as byte array\n");
		char buff[size];
		while(!feof(fp))
		{
			fread(buff, 1, sizeof(buff), fp);
			sendto(sockfd, buff, sizeof(buff), 0, (struct sockaddr*)&serveraddr, sizeof(struct sockaddr_in));
      printf("Sent: %s\n", buff);
      bzero(buff, sizeof(buff));
		}
    char *end = "EOF";
    sendto(sockfd, end, sizeof(end), 0, (struct sockaddr*)&serveraddr, sizeof(struct sockaddr_in));
    printf("File sent successfully\n");
    close(sockfd);
		return 0;
}
