//Danny DeRuiter

#include <sys/socket.h>
#include <netinet/in.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <limits.h>

#define PACKET_SIZE 512
#define WINDOW_SIZE 5

int allPacketsAcknowledged(unsigned char buf [WINDOW_SIZE][PACKET_SIZE]);
unsigned short checkSum(char *data, int len);

int main()
{
  int sockfd = socket(AF_INET,SOCK_DGRAM,0);
  if(sockfd < 0)
  {
    printf("There was an error creating the socket\n");
    return 1;
  }

  //initialize sockets
  fd_set sockets;
  FD_ZERO(&sockets);
  struct stat file_stat;

  //get port number
  char port[20];
  printf("Enter a port: ");
  fgets(port, LINE_MAX, stdin);

  struct sockaddr_in serveraddr, clientaddr;
  serveraddr.sin_family=AF_INET;
  serveraddr.sin_port=htons(atoi(port));
  serveraddr.sin_addr.s_addr=INADDR_ANY;

  //bind port
  int e = bind(sockfd,(struct sockaddr*)&serveraddr,sizeof(serveraddr));
  if (e < 0)
  {
    fprintf(stderr, "Bind");
    return 1;
  }
  //keep server alive indefinitely
  while(1)
  {
    int len = sizeof(clientaddr);

    //set timeout for 1 sec
    struct timeval timeout = {1,0};
    setsockopt(sockfd,SOL_SOCKET,SO_RCVTIMEO,(char*)&timeout,sizeof(struct timeval));

    char fileBuf[58];
    char fileName[50];
    int recvlen = recvfrom(sockfd,fileBuf,sizeof(fileBuf),0,(struct sockaddr*)&clientaddr,&len);
    if (recvlen<0)
    {
      continue;
    }

    unsigned short theirSum;
    memcpy(&theirSum,&fileBuf[4],2);
    memset(&fileBuf[4],0,4);
    unsigned short ourSum = checkSum(fileBuf,58);
    //checksum is not valid, just continue for now
    if (ourSum != theirSum)
    {
      continue;
    }
    //copy file name
    memcpy(fileName,&fileBuf[8],50);
    fileName[recvlen]='\0';

    FILE *fp;
    fp = fopen(fileName,"rb");
    char sizeBuf[12];
    int dataType = -1;
    unsigned long fileSize;
    memcpy(sizeBuf,&dataType,4);
    //file doesn't exist
    if (fp == NULL)
    {
      //let client know file DNE
      fileSize = -1;
      memcpy(&sizeBuf[8],&fileSize,4);

      memset(&sizeBuf[4],0,4);
      unsigned short ourSum = checkSum(sizeBuf,12);
      memcpy(&sizeBuf[4],&ourSum,2);

      sendto(sockfd,sizeBuf,12,0,(struct sockaddr*)&clientaddr,sizeof(clientaddr));
      printf("File not found\n\n");
      continue;
    }
    //file exists
    else
    {
      printf("File '%s' found.\n", fileName);
      if(stat(fileName, &file_stat) < 0)
      {
	       fprintf(stderr, "file stat");
	       return 1;
      }
      //file size for client
      fileSize = (unsigned long) file_stat.st_size;
      memcpy(&sizeBuf[8],&fileSize,4);

      memset(&sizeBuf[4],0,4);
      unsigned short ourSum = checkSum(sizeBuf,12);
      memcpy(&sizeBuf[4],&ourSum,2);

      sendto(sockfd,sizeBuf,12,0,(struct sockaddr*)&clientaddr,sizeof(clientaddr));
      printf("Sent File Size: %lu\n",fileSize);
    }

    unsigned char windowBuf [WINDOW_SIZE][PACKET_SIZE] = {0};
    int m;

    for(m = 0; m < WINDOW_SIZE; m++)
    {
      memset(&windowBuf[m],-1,4);
    }

    unsigned int cur = 0;
    unsigned int finalTimeout = 0;
    unsigned char buff[PACKET_SIZE];
    char ackbuff[8];

    while(1)
    {

      memset(buff,0,PACKET_SIZE);
      memset(ackbuff,0,8);

      int windowStartNum;
      memcpy(&windowStartNum,windowBuf,4);

      if(feof(fp) && allPacketsAcknowledged(windowBuf))
      {
	       printf("Received all acks\n\n");
	       break;
      }
      else if(!feof(fp) && windowStartNum == -1)
      {
	      //Send is not done. Ready to send another packet
	      //read from file
	      int nread = fread(&buff[12],1,500,fp);

	      //sequence number and data length
	      memcpy(buff,&cur,4);
	      memcpy(&buff[4],&nread,4);

	      memset(&buff[8],0,4);
	      unsigned short dataSum = checkSum(buff,nread+12);
	      memcpy(&buff[8],&dataSum,2);

	      sendto(sockfd, buff, nread+12, 0,(struct sockaddr*)&clientaddr,sizeof(clientaddr));
	      printf("Sending Sequence Number: %d. Sent %d bytes\n",cur,nread+12);

	      //shift windowBuf left and put current at the end
	      int sl;
	      for(sl=0; sl < 4; sl++)
        {
	        memcpy(&windowBuf[sl],&windowBuf[sl+1],PACKET_SIZE);
	      }

	      memcpy(&windowBuf[4],buff,PACKET_SIZE);
	      cur++;
      }
      else
      {
        printf("Win num: %d\n", windowStartNum);
	      //Send is not done. Wait for acks.

	       printf("Waiting for Acks\n");
	      int recvlen = recvfrom(sockfd,ackbuff,sizeof(ackbuff),0,(struct sockaddr*)&clientaddr,&len);

	      if (recvlen < 0)
        {
	         printf("Timeout on waiting for acks\n");

	          //check if we need to break out after sending same stuff 5 times.
	          if(finalTimeout >= 5)
            {
	             printf("Final timeout on waiting for acks. Client must be done.\n\n");
	            break;
            }
	          finalTimeout++;

	          //resend unacked packets
	          int i;
	          int num;
	          int dataLen;
	          //loop through the window to find unsent packets
	          for (i = 0;i < WINDOW_SIZE; i++)
            {
	            memcpy(&num,&windowBuf[i],4);
	            if(num != -1)
              {
	              memcpy(&dataLen,&windowBuf[i][4],4);
	              memcpy(buff,&windowBuf[i],dataLen+12);
	              //resend packet
	              sendto(sockfd, buff, dataLen+12, 0,(struct sockaddr*)&clientaddr,sizeof(clientaddr));
	              printf("Seq num sent: %d\n",num);
	            }
	          }
	        }
          else
          {
	            //got an ack
	            unsigned short theirSum;
	            memcpy(&theirSum,&ackbuff[4],2);
	            memset(&ackbuff[4],0,4);
	            unsigned short ackSum = checkSum(ackbuff,8);
	            if (ackSum != theirSum)
              {
	               printf("Invalid Checksum\n");
	              continue;
	            }

	            unsigned int ackNum;
	            memcpy(&ackNum, ackbuff, 4);
	            printf("Received Ack Number: %d\n",ackNum);

	            //set windowBuf num flag to -1
	            int i;
	            int temp;
	            int flag = -1;
	            for (i = 0; i < 5;i++)
              {
	                memcpy(&temp,&windowBuf[i],4);
	                if(temp == ackNum)
                  {
	                   memcpy(&windowBuf[i],&flag,4);
	                    finalTimeout = 0;
	                }
	            }

	        }
        }
      }
    }
  return 0;
}

int allPacketsAcknowledged(unsigned char buf [WINDOW_SIZE][PACKET_SIZE]){
  int temp = 0;
  int ackCount = 0;
  int i;
  for(i=0; i < 5; i++){
    memcpy(&temp,&buf[i],4);
    if (temp == -1){
      ackCount++;
    }
  }
  if(ackCount == 5){
    return 1;
  }
  return 0;
}

unsigned short checkSum(char *data, int len){
  unsigned int sum = 0;
  int i;

  //checksum
  for (i = 0; i < len - 1; i += 2){
    unsigned short temp = *(unsigned short *) &data[i];
    sum += temp;
  }

  if(len%2)       //in odd length cases take care of left over byte
    sum += (unsigned short) *(unsigned char *)&data[len];

  //shift carry over
  while (sum>>16)
    sum = (sum & 0xFFFF) + (sum >> 16);

  return ~sum;
}
