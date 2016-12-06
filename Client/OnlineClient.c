#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <time.h>
#include <stdlib.h>
#include <string.h>

unsigned short calcCheckSum(char *data, int len);

int main(int argc, char** argv){
  int sockfd = socket(AF_INET,SOCK_DGRAM,0);
  if(sockfd<0){
    printf("There was an error creating the socket\n");
    return 1;
  }

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

  //get ip address from command line
  char* ipAddrStr;
  ipAddrStr = argv[2];
  printf("IP Address: %s\n", ipAddrStr);

  struct sockaddr_in serveraddr;
  serveraddr.sin_family=AF_INET;
  serveraddr.sin_port=htons(portNum);
  serveraddr.sin_addr.s_addr=inet_addr(ipAddrStr);

  while(1){
    int len = sizeof(serveraddr);

    struct timeval timeout={1,0}; //set timeout for 1 sec
    setsockopt(sockfd,SOL_SOCKET,SO_RCVTIMEO,(char*)&timeout,sizeof(struct timeval));

    //send file name to retrieve
    printf("Specify name of file to retrieve or type \"exit\": ");
    char file_name[50];
    char fileNameBuf[58];
    int dataType = -2;
    fgets(file_name,50,stdin);
    //chop off newline character from user input
    file_name[strlen(file_name) - 1] = '\0';

    memcpy(fileNameBuf,&dataType,4);
    memcpy(&fileNameBuf[8],file_name,50);

    //send filename to the server

    //TODO calculate checksum
    memset(&fileNameBuf[4],0,4);
    unsigned short nameCheckSum = calcCheckSum(fileNameBuf,58);
    memcpy(&fileNameBuf[4],&nameCheckSum,2);

    //sendto(sockfd,fileNameBuf,58,0,(struct sockaddr*)&serveraddr,sizeof(serveraddr));

    //disconnect on input "exit"
    if(!strcmp(file_name, "exit")){
      return 0;
    }

    //Check if file existed on server and receive size

    char file_size[12];
    unsigned long remaining_bytes = 0;
    unsigned short sizeValidate;
    unsigned short sizeCheckSum;
    int recvlen;



    do {
      sendto(sockfd,fileNameBuf,58,0,(struct sockaddr*)&serveraddr,sizeof(serveraddr));
      printf("Sent file name.\n");
      recvlen = recvfrom(sockfd, file_size, 12, 0,(struct sockaddr*)&serveraddr,&len);

      //TODO validate checksum
      memcpy(&sizeValidate,&file_size[4],2);
      memset(&file_size[4],0,4);
      sizeCheckSum = calcCheckSum(file_size,12);

      memcpy(&dataType,file_size,4);
      memcpy(&remaining_bytes,&file_size[8],4);

    } while (recvlen < 0 || dataType != -1 || sizeValidate != sizeCheckSum);

    if (remaining_bytes==-1){
      //file does not exist
      printf("File does not exist.\n\n");
      continue;
    }

    printf("Received File Size: %lu\n",remaining_bytes);

    //create and open a file
    FILE *received_file;
    received_file = fopen(file_name, "w");

    char recvBuff[512];
    int bytesReceived = 0;

    unsigned char tempbuf [5][512] = {0};
    unsigned short ackCheckSum;
    memset(tempbuf,-1,512*5);

    unsigned int cur = 0;
    //int serverFlag = 0;

    /* Receive data in chunks of 512 bytes */
    while(remaining_bytes > 0) {
      memset(recvBuff,0,512);

      bytesReceived = recvfrom(sockfd, recvBuff, sizeof(recvBuff),0,(struct sockaddr*)&serveraddr,&len);


      if(bytesReceived > 0){

	int dataLength;
	memcpy(&dataLength,&recvBuff[4],4);

	//TODO validate checksum on recvbuf
	unsigned short dataValidate;
	memcpy(&dataValidate,&recvBuff[8],2);
	memset(&recvBuff[8],0,4);
	unsigned short dataCheckSum = calcCheckSum(recvBuff,dataLength+12);


	if(dataCheckSum == dataValidate){

	  printf("Received %d bytes. ",bytesReceived);



	  unsigned int recvNum = 0;
	  memcpy(&recvNum,recvBuff,4);

	  unsigned char ackbuff[8]={0};

	  //check if we have already received this packet
	  if(recvNum < cur){
	    printf("Already Received. Received %d but was expecting %d.\n",recvNum,cur);
	    //resend acknowledgement

	    memcpy(&ackbuff,&recvNum,4);

	    //TODO calculate checksum
	    memset(&ackbuff[4],0,4);
	    ackCheckSum = calcCheckSum(ackbuff,8);
	    memcpy(&ackbuff[4],&ackCheckSum,2);

	    sendto(sockfd,ackbuff,8,0,(struct sockaddr*)&serveraddr,sizeof(serveraddr));
	    printf("Resending Acknowledgement Number %d\n",recvNum);
	  } else if (recvNum == cur){
	    printf("In Order: %d\n",recvNum);
	    //write to file
	    fwrite(&recvBuff[12], 1,bytesReceived-12,received_file);
	    remaining_bytes -= (bytesReceived-12);
	    printf("From in order writing Sequence Number: %d. There are %lu Bytes Remaining\n",cur,remaining_bytes);

	    //send acknowledgement
	    memcpy(&ackbuff,&recvNum,4);

	    //TODO calculate checksum
	    memset(&ackbuff[4],0,4);
	    ackCheckSum = calcCheckSum(ackbuff,8);
	    memcpy(&ackbuff[4],&ackCheckSum,2);

	    printf("Sending in order Acknowledgement Number %d\n",recvNum);
	    sendto(sockfd,ackbuff,8,0,(struct sockaddr*)&serveraddr,sizeof(serveraddr));
	    cur++;

	    //check if there is stuff in the tempbuf to also write to file
	    int i;
	    for(i=0;i<5;i++){
	      int tempAckNum;
	      memcpy(&tempAckNum,&tempbuf[i],4);
	      //printf("Writing Sequence Number: %d\n",cur);
	      if(tempAckNum == cur){
		int dataLength;
		memcpy(&dataLength,&tempbuf[i][4],4);

		//write to file
		fwrite(&tempbuf[i][12], 1,dataLength,received_file);
		remaining_bytes -= dataLength;
		printf("From tempbuf writing Sequence Number: %d. There are %lu Bytes Remaining\n",cur,remaining_bytes);
		//reset memory in our tempbuffer
		memset(&tempbuf[i],-1,512);
		//memset(&tempbuf[i]+4,0,508);
		cur++;
		//go back to beginning of loop and check again if the next sequence number
		i=-1;
	      }
	    }
	  } else {
	    printf("Out of Order. Expecting %d but received %d:\n",cur,recvNum);
	    //This is not in the correct order. Put in our temp buffer and send an ack
	    //send acknowledgement

	    int i;
	    for(i=0;i<5;i++){
	      int tempAckNum;
	      memcpy(&tempAckNum,&tempbuf[i],4);
	      //printf("Writing Sequence Number: %d\n",cur);
	      if(tempAckNum == -1){
		printf("Found spot in tempbuf for Sequence Number: %d. There are %lu Bytes Remaining\n",recvNum,remaining_bytes);
		//store in the tempbuf
		memcpy(&tempbuf[i],recvBuff,512);

		//send acknowledgement
		memcpy(ackbuff,&recvNum,4);

		//TODO calculate checksum
		memset(&ackbuff[4],0,4);
		ackCheckSum = calcCheckSum(ackbuff,8);
		memcpy(&ackbuff[4],&ackCheckSum,2);

		printf("Sending out of order Acknowledgement Number %d\n",recvNum);
		sendto(sockfd,ackbuff,8,0,(struct sockaddr*)&serveraddr,sizeof(serveraddr));

		//break out of for loop so we only store once
		break;
	      }
	    }
	  }
	} else {
	    printf("INVALID CHECKSUM\n");
	}
      }
    }

    printf("Transfer Successful\n\n");
    fclose(received_file);
  }
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
