//Danny DeRuiter

#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <limits.h>

/**************************************
 * Client
 *************************************/
int main(int argc, char **argv)
{
    char port[20];
    char ip_address[20];
    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
		FILE *fp;
    char recvBuff[256];
		char filename[20];
		memset(recvBuff, '0', sizeof(recvBuff));
		memset(filename, 0, sizeof(filename));

		//get the port first
    printf("Enter a port: ");
    fgets(port, LINE_MAX, stdin);

    //get the IP address
    printf("Enter an IP address: ");
    fgets(ip_address, LINE_MAX, stdin);

    if(sockfd < 0)
    {
      printf("There was an error creating the socket\n");
      return 1;
    }
    struct sockaddr_in serveraddr;
    serveraddr.sin_family = AF_INET; //what time of socket it is
    serveraddr.sin_port = htons((atoi(port))); //this is so the server knows where to return data
    serveraddr.sin_addr.s_addr = inet_addr(ip_address);

    int len = sizeof(struct sockaddr_in);

		//send the file name to the server
		printf("Enter the file you want to receive: ");
		fgets(filename, sizeof(filename), stdin);
    filename[strlen(filename)-1] = '\0';
		sendto(sockfd, filename, strlen(filename), 0, (struct sockaddr*)&serveraddr, sizeof(struct sockaddr_in));

		//get file size from server
		int size;
		recvfrom(sockfd, &size, sizeof(size), 0, (struct sockaddr*)&serveraddr, &len);
		printf("file size received: %i\n", size);

		//read the array of bytes from the server
		char bytes[size];
    int bytesReceived = 0;
    printf("Filename: %s\n", filename);
    printf("Writing bytes to file...\n");
    fp = fopen(filename, "w");
		while(bytesReceived < size)
    {
      bytesReceived += recvfrom(sockfd, bytes, sizeof(bytes), 0, (struct sockaddr*)&serveraddr, &len);
      printf("Bytes Received: %d\n", bytesReceived);
      if(strcmp(bytes, "EOF") == 0)
        break;
      //convert bytes to file
      printf("Received: %s\n\n", bytes);
      fwrite(bytes, 1, sizeof(bytes), fp);
      bzero(bytes, sizeof(bytes));
    }

    fclose(fp);
		printf("File transfer complete\n");
		close(sockfd);
    return 0;
}
