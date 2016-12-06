#include <sys/socket.h>
#include <netinet/in.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>

int allAcksReceived(unsigned char buf [5][512]);
void printAcksArray(unsigned char buf [5][512]);
unsigned short calcCheckSum(char *data, int len);

int main(int argc, char** argv){
  int sockfd = socket(AF_INET,SOCK_DGRAM,0);
  if(sockfd < 0){
    printf("There was an error creating the socket\n");
    return 1;
  }

  //set and initialize file decriptors
  fd_set sockets;
  FD_ZERO(&sockets);
  struct stat file_stat;

  //get port number from command line
  char* portNumStr;
  portNumStr = argv[1];
  int portNum;
  portNum = atoi(portNumStr);
  printf("Port Number: %d\n", portNum);
  if (portNum <= 0){
    printf("Invalid Port Number\n");
    return 0;
  }

  struct sockaddr_in serveraddr, clientaddr;
  serveraddr.sin_family=AF_INET;
  serveraddr.sin_port=htons(portNum);
  serveraddr.sin_addr.s_addr=INADDR_ANY;

  int e = bind(sockfd,(struct sockaddr*)&serveraddr,sizeof(serveraddr));
  if (e < 0){
    printf("Failed to bind Port %d\n",portNum);
    return 1;
  }

  while(1){
    int len=sizeof(clientaddr);

    struct timeval timeout={1,0}; //set timeout for 1 sec
    setsockopt(sockfd,SOL_SOCKET,SO_RCVTIMEO,(char*)&timeout,sizeof(struct timeval));

    //printf("Waiting for client connection on port %d\n",portNum);
    char fileNameBuf[58];
    char file_name[50];
    int recvlen = recvfrom(sockfd,fileNameBuf,sizeof(fileNameBuf),0,(struct sockaddr*)&clientaddr,&len);
    if (recvlen<0){
      continue;
    }

    //TODO validate checksum on received file name
    unsigned short validate;
    memcpy(&validate,&fileNameBuf[4],2);
    memset(&fileNameBuf[4],0,4);
    unsigned short ourSum = calcCheckSum(fileNameBuf,58);
    if (ourSum != validate){
      continue;
    }

    memcpy(file_name,&fileNameBuf[8],50);
    //chop off end of the 50 bytes
    file_name[recvlen]='\0';

    printf("Checking for file name: %s\n",file_name);


    FILE *fp;
    fp = fopen(file_name,"rb");
    char fileSizeBuf[12];
    int dataType = -1;
    unsigned long fileSize;
    memcpy(fileSizeBuf,&dataType,4);
    if (fp == NULL){
      //send message stating if file does not exist
      fileSize = -1;
      memcpy(&fileSizeBuf[8],&fileSize,4);

      //TODO calculate checksum
      memset(&fileSizeBuf[4],0,4);
      unsigned short ourSum = calcCheckSum(fileSizeBuf,12);
      memcpy(&fileSizeBuf[4],&ourSum,2);

      sendto(sockfd,fileSizeBuf,12,0,(struct sockaddr*)&clientaddr,sizeof(clientaddr));
      printf("File does not exist.\n\n");
      continue;
    } else {
      printf("File exists.\n");

      if(stat(file_name, &file_stat) < 0){
	printf("Error on file stats\n");
	return 1;
      }
      fileSize = (unsigned long) file_stat.st_size;
      memcpy(&fileSizeBuf[8],&fileSize,4);

      //TODO calculate checksum
      memset(&fileSizeBuf[4],0,4);
      unsigned short ourSum = calcCheckSum(fileSizeBuf,12);
      memcpy(&fileSizeBuf[4],&ourSum,2);

      //send file size
      sendto(sockfd,fileSizeBuf,12,0,(struct sockaddr*)&clientaddr,sizeof(clientaddr));
      printf("Sent File Size: %lu\n",fileSize);
    }

    //acknowledgement array and temp buff
    unsigned char tempbuf [5][512] = {0};
    int m;
    //set our acks array to all be -1
    for(m = 0; m < 5; m++){
      memset(&tempbuf[m],-1,4);
    }

    unsigned int cur = 0;
    unsigned int finalTimeout = 0;
    unsigned char buff[512];
    char ackbuff[8];

    while(1) {

      memset(buff,0,512);
      memset(ackbuff,0,8);

      int windowStartNum;
      memcpy(&windowStartNum,tempbuf,4);

      if(feof(fp) && allAcksReceived(tempbuf)){
	printf("Received all acks\n\n");
	break;
	//nread > 0 &&
      } else if(!feof(fp) && windowStartNum == -1) {
	//Send is not done. Ready to send another packet

	//read from file
	int nread = fread(&buff[12],1,500,fp);

	//sequence number and data length
	memcpy(buff,&cur,4);
	memcpy(&buff[4],&nread,4);

	//TODO calculate checksum
	memset(&buff[8],0,4);
	unsigned short dataCheckSum = calcCheckSum(buff,nread+12);
	memcpy(&buff[8],&dataCheckSum,2);

	sendto(sockfd, buff, nread+12, 0,(struct sockaddr*)&clientaddr,sizeof(clientaddr));
	printf("Sending Sequence Number: %d. Sent %d bytes\n",cur,nread+12);

	//shift tempbuf left and put current at the end
	int sl;
	for(sl=0; sl < 4; sl++){
	  memcpy(&tempbuf[sl],&tempbuf[sl+1],512);
	}
	memcpy(&tempbuf[4],buff,512);
	cur++;

	printAcksArray(tempbuf);

      } else {
    printf("Win num: %d\n", windowStartNum);
	//Send is not done. Wait for acks.

	printf("Waiting for Acks\n");
	int recvlen = recvfrom(sockfd,ackbuff,sizeof(ackbuff),0,(struct sockaddr*)&clientaddr,&len);

	if (recvlen < 0){
	  printf("Timeout on waiting for acks\n");

	  //check if we need to break out after sending same stuff 5 times.
	  if(finalTimeout >= 5){
	    printf("Final timeout on waiting for acks. Client must be done.\n\n");
	    break;
	  }
	  finalTimeout++;

	  //Resend the packets we have not received acks for.
	  int rs;
	  int ackNumber;
	  int dataLength;
	  //loop through tempbuf and resend packets that did not get acknowledgement
	  for (rs=0;rs < 5;rs++){
	    memcpy(&ackNumber,&tempbuf[rs],4);
	    if(ackNumber != -1){
	      memcpy(&dataLength,&tempbuf[rs][4],4);
	      //12 for header info
	      memcpy(buff,&tempbuf[rs],dataLength+12);
	      //resend packet
	      sendto(sockfd, buff, dataLength+12, 0,(struct sockaddr*)&clientaddr,sizeof(clientaddr));
	      printf("Sending Sequence Number: %d. Sent %d bytes\n",ackNumber,dataLength);
	    }
	  }
	} else {
	  //there was not a timeout. Ack received.

	  //TODO validate checksum.
	  unsigned short ackValidate;
	  memcpy(&ackValidate,&ackbuff[4],2);
	  memset(&ackbuff[4],0,4);
	  unsigned short ackCheckSum = calcCheckSum(ackbuff,8);
	  if (ackCheckSum != ackValidate){
	    printf("Corrupt Acknowledgement\n");
	    continue;
	  }

	  unsigned int ackNum;
	  memcpy(&ackNum,ackbuff,4);
	  printf("Received Ack Number: %d\n",ackNum);

	  //set tempbuf acknumber flag to -1
	  int f;
	  int temp;
	  int flag = -1;
	  for (f=0;f < 5;f++){
	    memcpy(&temp,&tempbuf[f],4);
	    if(temp == ackNum){
	      memcpy(&tempbuf[f],&flag,4);
	      finalTimeout = 0;
	    }
	  }

	  printAcksArray(tempbuf);
	}
      }
    }
  }
  return 0;
}

int allAcksReceived(unsigned char buf [5][512]){
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

void printAcksArray(unsigned char buf [5][512]){
  int temp = 0;
  int datalen = 0;
  int i;
  printf("Current acks array: ");
  for(i=0; i < 5; i++){
    memcpy(&temp,&buf[i],4);
    memcpy(&datalen,&buf[i][4],4);
    printf("%d(%d) ",temp,datalen);
  }
  printf("\n");
}

unsigned short calcCheckSum(char *data, int len){
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
