//Danny DeRuiter

#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <limits.h>
#define PACKET_SIZE 512
#define WINDOW_SIZE 5
unsigned short checkSum(char *data, int len);

/**************************************
 * Client
 *************************************/
int main(int argc, char **argv)
{
    char port[20];
    char ip_address[20];
    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);

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

    int len = sizeof(serveraddr);

		//send the file name to the server
		printf("Enter the file you want to receive: ");
    char fileNameBuf[58];
    char filename[20];
    int dataType = -2;
		fgets(filename, sizeof(filename), stdin);
    filename[strlen(filename)-1] = '\0';

    memcpy(fileNameBuf, &dataType, 4);
    memcpy(&fileNameBuf[8], filename, 50);
    printf("File name: %s\n", &fileNameBuf[8]);
    //checksum
    memset(&fileNameBuf[4], 0, 4);
    unsigned short fileSum = checkSum(fileNameBuf, 58);
    printf("Checksum: %d\n", fileSum);
    memcpy(&fileNameBuf[4], &fileSum, 2);

    char size[12];
    unsigned long bytesRemaining = 0;
    unsigned short sizeValidate;
    unsigned short sizeSum;

    int recvlen;

    //loop until we get a valid reply
    do
    {
        sendto(sockfd, fileNameBuf, sizeof(fileNameBuf), 0, (struct sockaddr*)&serveraddr, sizeof(serveraddr));
        printf("Sent: %s\n", fileNameBuf);
        recvlen = recvfrom(sockfd, size, sizeof(size), 0, (struct sockaddr*)&serveraddr, &len);
        memcpy(&sizeValidate, &size[4], 2);
        memset(&size[4], 0, 4);
        sizeSum = checkSum(size, 12);

        memcpy(&dataType, size, 4);
        memcpy(&bytesRemaining, &size[8], 4);
    } while(recvlen < 0 || dataType != -1 || sizeValidate != sizeSum);

    if(bytesRemaining == -1)
    {
      printf("File does not exist.\n");
      exit(1);
    }

    printf("Received File Size: %lu\n", bytesRemaining);
    FILE *fp;
    fp = fopen(filename, "w");
    char recvBuff[PACKET_SIZE];
    int bytesReceived = 0;

    unsigned char windowBuf[WINDOW_SIZE][PACKET_SIZE] = {0};
    unsigned short ackSum;
    memset(windowBuf, -1, PACKET_SIZE*5);
    unsigned int cur = 0;

    while(bytesRemaining > 0)
    {
      memset(recvBuff, 0, PACKET_SIZE);
      bytesReceived = recvfrom(sockfd, recvBuff, sizeof(recvBuff), 0, (struct sockaddr*)&serveraddr, &len);

      if(bytesReceived > 0)
      {
        int dataLen;
        memcpy(&dataLen, &recvBuff[4], 4);

        unsigned short theirSum;
        memcpy(&theirSum, &recvBuff[8], 2);
        memset(&recvBuff[8], 0, 4);
        unsigned short ourSum = checkSum(recvBuff, dataLen+12);
        //valid packet
        if(ourSum == theirSum)
        {
          printf("Received %d bytes\n", bytesReceived);
          unsigned int recvNum = 0;
          memcpy(&recvNum, recvBuff, 4);
          unsigned char ackBuff[8] = {0};

          //check if packet has already been received
          if(recvNum < cur)
          {
            printf("Received packet %d already, expected %d.\n", recvNum, cur);
            //resend ack
            memcpy(&ackBuff, &recvNum, 4);
            memset(&ackBuff[4], 0, 4);
            ackSum = checkSum(ackBuff, 8);
            memcpy(&ackBuff[4], &ackSum, 2);

            sendto(sockfd, ackBuff, sizeof(ackBuff), 0, (struct sockaddr*)&serveraddr, sizeof(serveraddr));
            printf("Resening ack num %d\n", recvNum);
          }
          //in order packet, write to file
          else if (recvNum == cur)
          {
            printf("Seq num in order: %d\n", recvNum);
            fwrite(&recvBuff[12], 1, bytesReceived-12, fp);
            bytesRemaining -= (bytesReceived - 12);
            printf("Bytes remaining: %lu\n", bytesRemaining);

            //send acks
            memcpy(&ackBuff, &recvNum, 4);
            memset(&ackBuff[4], 0, 4);
            ackSum = checkSum(ackBuff, 8);
            memcpy(&ackBuff[4], &ackSum, 2);

            sendto(sockfd, ackBuff, sizeof(ackBuff), 0, (struct sockaddr*)&serveraddr, sizeof(serveraddr));
            printf("Sent in order ack: %d\n", recvNum);
            cur++;

            //check if windowBuf has bytes leftover
            int i;
            for(i = 0; i < 5; i++)
            {
              int tempNum;
              memcpy(&tempNum, &windowBuf[i], 4);
              if(tempNum == cur)
              {
                int dataLen;
                memcpy(&dataLen, &windowBuf[i][4], 4);
                fwrite(&windowBuf[i][12], 1, dataLen, fp);
                bytesRemaining -= dataLen;
                printf("From temp seq: %d Bytes Remaining: %lu\n", cur, bytesRemaining);
                memset(&windowBuf[i], -1, PACKET_SIZE);
                cur++;
                //check again if the next seq num is in temp
                i=-1;
              }
            }
          }
          else
          {
            printf("Out of order, Expected %d but got %d\n", cur, recvNum);
            int i;
            for(i = 0; i < 5; i++)
            {
              int tempNum;
              memcpy(&tempNum, &windowBuf[i], 4);
              if(tempNum == -1)
              {
                printf("Found spot in temp buf for seq: %d Bytes remaining: %lu\n", recvNum, bytesRemaining);
                memcpy(&windowBuf[i], recvBuff, PACKET_SIZE);

                //send ack
                memcpy(ackBuff, &recvNum, 4);
                memset(&ackBuff[4], 0, 4);
                ackSum = checkSum(ackBuff, 8);
                memcpy(&ackBuff[4], &ackSum, 2);
                printf("Sending out of order ack: %d\n", recvNum);
                sendto(sockfd, ackBuff, sizeof(ackBuff), 0, (struct sockaddr*)&serveraddr, sizeof(serveraddr));
                break;
              }
            }
          }
        }
        else
        {
          printf("Checksum Invalid\n");
        }
      }
    }

    fclose(fp);
		printf("File transfer complete\n");
		close(sockfd);
    return 0;
}

unsigned short checkSum(char *data, int len){
  unsigned int sum = 0;
  int i;
  
  //checksum
  for (i = 0; i < len - 1; i += 2)
  {
    unsigned short temp = *(unsigned short *) &data[i];
    sum += temp;
  }

  //in odd length cases take care of left over byte
  if(len%2)
  {
    sum += (unsigned short) *(unsigned char *)&data[len];
  }

  //shift carry over
  while (sum>>16)
  {
    sum = (sum & 0xFFFF) + (sum >> 16);
  }
  return ~sum;
}
