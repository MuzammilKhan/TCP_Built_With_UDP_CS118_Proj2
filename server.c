#include <stdio.h>
#include <sys/types.h>   // definitions of a number of data types used in socket.h and netinet/in.h
#include <sys/socket.h>  // definitions of structures needed for sockets, e.g. sockaddr
#include <netinet/in.h>  // constants and structures needed for internet domain addresses, e.g. sockaddr_in
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>	/* for the waitpid() system call */
#include <signal.h>	/* signal name macros, and the kill() prototype */


void error(char *msg)
{
  perror(msg);
  exit(1);
}

/*Decode the TCP header at msgp and get desired values. Following data types chosen so that encoding is easy later. Using referenced
data types instead of pointers for clarity about sizes. ACK, SYN, and FIN use only 1 bit. Ignoring checksum and other fields. */
void DecodeTCPHeader(char* msgp, unsigned short& source, unsigned short& destination, unsigned int& sequence_number,
 unsigned int& acknowledgement_number, unsigned int& ACK, unsigned int& SYN, unsigned int& FIN, unsigned short& window_size){
  memcpy(&source, msgp, 2);
  memcpy(&destination, msgp+16, 2);  
  memcpy(&sequence_number, msgp+32, 4); 
  memcpy(&acknowledgement_number, msgp+64, 4);
  char tmp; 
  memcpy(&tmp, msgp+102, 1);
  ACK = (tmp & 16) >> 4;
  SYN = tmp & 2 >> 1;
  FIN = tmp & 1;  
  memcpy(window_size, msgp+112, 4);  
  return;
}

/*Encode TCP header at msgp. Set checksum and other fields not explicitly set here to 0*/
void EncodeTCPHeader(char* msgp, unsigned short source, unsigned short destination, unsigned int sequence_number,
 unsigned int acknowledgement_number, unsigned int ACK, unsigned int SYN, unsigned int FIN, unsigned short window_size){
  memset(msgp, 0, 768); //set header to 0
  memcpy(msgp, &source, 2); 
  memcpy(msgp+16,&destination, 2); 
  memcpy(msgp+32, &sequence_number, 4);
  memcpy(msgp+64,&acknowledgement_number, 4);
  //memset(msgp+96, (6 << 5), 1); //setting data offset to 6 32-bit words. Check: This can probably be ignored for our purpose as we assume it
  memset(msgp+102, ((ACK << 4) + (SYN << 1) + FIN), 1);// Assuming other flags to be 0 for our purposes
  memcpy(msgp+112, window_size, 4);  
  return;
}

//basic udp server reference: https://www.cs.cmu.edu/afs/cs/academic/class/15213-f99/www/class26/udpserver.c

int main(int argc, char *argv[])
{
  // restrictions for our TCP implementation
  int max_packet_length = 1024; //includes header, in bytes
  int max_sequence_number = 30720; //bytes    int window_size = 5120; //bytes
  int rto_val = 500; //retransmission timeout value (ms)

  int sockfd, portno, pid;
  socklen_t clilen;
  struct sockaddr_in serv_addr, cli_addr;

  if (argc < 2) {
    fprintf(stderr,"ERROR, no port provided\n");
    exit(1);
  }
  sockfd = socket(AF_INET, SOCK_DGRAM, 0);	//create socket -- UDP
  if (sockfd < 0) 
    error("ERROR opening socket");
  memset((char *) &serv_addr, 0, sizeof(serv_addr));	//reset memory
  //fill in address info
  portno = atoi(argv[1]);
  serv_addr.sin_family = AF_INET;
  serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
  serv_addr.sin_port = htons(portno);
     
  if (bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) 
    error("ERROR on binding");
          
  int n;
  int buffer_size = 256;
  char buffer[buffer_size];			 
  memset(buffer, 0, buffer_size);	//reset memory

  //------- Three Way Handshake -------
  //wait for client to initiate connection
  clilen = sizeof(cli_addr)
  bzero(buffer, buffer_size);
  n = recvfrom(sockfd, buffer, buffer_size, 0, (struct sockaddr *) &cli_addr, &clilen);
  if(n < 0)
    error("ERROR in three way handshake: recvfrom");

  //TODO: Decode header from client message and set values for response

  //response to client
  n = sendto(sockfd, buffer, strlen(buffer), 0, (struct sockaddr *) &cli_addr, clilen);
  if (n < 0)  
    error("ERROR in three way handshake: sendto");
  //connection established at this point

  //wait for client to respond
  bzero(buffer, buffer_size);
  n = recvfrom(sockfd, buffer, buffer_size, 0, (struct sockaddr *) &cli_addr, &clilen);
  if(n < 0)
    error("ERROR in three way handshake: recvfrom");
  //TODO: Decode header from client message. 
  //TODO: Do we retransmit if part of this time out?

  //------- CONNECTION ESTABLISHED -------

  //keep receiving data from client and respond accordingly
  /*
  while(1){
    bzero(buffer, buffer_size);
    n = recvfrom(sockfd, buffer, buffer_size, 0, (struct sockaddr *) &cli_addr, &clilen);
    if(n < 0)
      error("ERROR in recvfrom");

    n = sendto(sockfd, buffer, strlen(buffer), 0, (struct sockaddr *) &cli_addr, clilen);
    if (n < 0)  
      error("ERROR in sendto");
  }
  */

  close(sockfd);         
  return 0; 
}

