#include <stdio.h>
#include <sys/types.h>   // definitions of a number of data types used in socket.h and netinet/in.h
#include <sys/socket.h>  // definitions of structures needed for sockets, e.g. sockaddr
#include <netinet/in.h>  // constants and structures needed for internet domain addresses, e.g. sockaddr_in
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>	/* for the waitpid() system call */
#include <signal.h>	/* signal name macros, and the kill() prototype */
#include <sys/time.h> // for timer
#include <fcntl.h> // for open
#include <unistd.h> // for close
#include <netdb.h>
void error(char *msg)
{
  perror(msg);
  exit(1);
}

/*Decode basic "TCP header" at msgp and get desired values. Following data types chosen so that encoding is easy later. Using referenced
data types instead of pointers for clarity about sizes. ACK, SYN, and FIN use only 1 bit. Ignoring checksum and other fields. */
void DecodeTCPHeader(char* msgp, char* data,unsigned int* sequence_number, unsigned int* acknowledgement_number, unsigned short* ACK, unsigned short* SYN, unsigned short* FIN, unsigned short* window_size){
  memcpy(sequence_number, msgp, 4); 
  memcpy(acknowledgement_number, msgp+4, 4);
  memcpy(ACK, msgp+8, 2);
  memcpy(SYN, msgp+10, 2);
  memcpy(FIN, msgp+12, 2);  
  memcpy(window_size, msgp+14, 2);
  
  
  printf("srv_seq_rec: %d\n", *sequence_number ); //debugging
  printf("srv_ack_rec: %d\n", *acknowledgement_number ); //debugging
  printf("srv_SYN_rec: %u\n\n", *SYN);
  return;
}

/*Encode "TCP header" at msgp. Set checksum and other fields not explicitly set here to 0*/
void EncodeTCPHeader(char* msgp,char* data ,unsigned int sequence_number, unsigned int acknowledgement_number, unsigned short ACK, unsigned short SYN, unsigned short FIN, unsigned short window_size){
  printf("srv_seq_sent: %u\n",sequence_number);
  printf("srv_ack_sent: %u\n",acknowledgement_number);
  printf("srv_SYN_sent: %u\n\n",SYN );
  memset(msgp, 0, 1024); //set header to 0
  memcpy(msgp, &sequence_number, 4);
  memcpy(msgp+4,&acknowledgement_number, 4);

  memcpy(msgp +8, &ACK, 2);
  memcpy(msgp+10,&SYN, 2);
  memcpy(msgp+12,&FIN, 2);
  
  memcpy(msgp+14, &window_size, 2);
   memcpy(msgp+16 , data,50);
   printf("sent: %s\n", data);
  return;
}

//basic udp server reference: https://www.cs.cmu.edu/afs/cs/academic/class/15213-f99/www/class26/udpserver.c

int main(int argc, char *argv[])
{
  // restrictions for our TCP implementation
  int max_packet_length = 1024; //includes header, in bytes
  int max_sequence_number = 30720; //bytes    
  int window_size_req = 5120; //bytes
  int rto_val = 500 * 1000; //retransmission timeout value (ms) but select uses us, so we multiply by 1000

  //timer
  struct timeval Timer;

  int sockfd, portno, pid;
  socklen_t clilen;
  struct sockaddr_in serv_addr, cli_addr;
  int running = 1; //set to 0 when closing server
  fd_set active_fd_set; // set for holding sockets

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
  int buffer_size = 1024;
  char buffer[buffer_size];			 
  memset(buffer, 0, buffer_size);	//reset memory

  unsigned short source; 
  unsigned short destination;
  unsigned int sequence_number;
  unsigned int acknowledgement_number;
  unsigned short ACK;
  unsigned short SYN;
  unsigned short FIN;
  unsigned short window_size = window_size_req; // change
  struct hostent *hostp;
  int handshake = 0;
  int closing = 0;
  
  FILE *fp;
 
  //we are using a hard coded port for our application so source = dest port
  source = portno;
  destination = source;
  char file_data[100];
  bzero(file_data ,100);

  fp = fopen("file.txt" , "r");      
  
  while(running){
    
    FD_ZERO(&active_fd_set);
    FD_SET(sockfd, &active_fd_set);
    Timer.tv_sec = 0;
    Timer.tv_usec = rto_val;
    clilen = sizeof(cli_addr);
    n = select(sockfd + 1, &active_fd_set, NULL, NULL, &Timer);
    
    if (n < 0) {
      close(sockfd);
      //ERROR OCCURED TODO: handle error
    } else if (n == 0) { //timeout occured
      //TODO: handle cases where retransmission is necessary here
    } else { 
      if (FD_ISSET(sockfd, &active_fd_set)) { // new connection request
        bzero(buffer, buffer_size);
        n = recvfrom(sockfd, buffer, buffer_size, 0, (struct sockaddr *) &cli_addr, &clilen);
        if(n < 0)
            error("ERROR recvfrom");
        
        DecodeTCPHeader(buffer, file_data, &sequence_number, &acknowledgement_number, &ACK, &SYN, &FIN, &window_size);
    

        if(SYN){ // THREE WAY HANDSHAKE
         
          ACK = 1;
          SYN = 1;
          FIN = 0;
          
          
          acknowledgement_number = sequence_number + 1; 
        
          sequence_number = 5000;
          handshake = 1;
          //respond to client
          bzero(buffer, buffer_size);
          EncodeTCPHeader(buffer, file_data,sequence_number, acknowledgement_number, ACK, SYN, FIN, window_size);
          n = sendto(sockfd, buffer, sizeof(buffer), 0, (struct sockaddr *) &cli_addr, clilen);
          if (n < 0)  
            error("ERROR in three way handshake: sendto");
        
          bzero(buffer, buffer_size);
          n = recvfrom(sockfd, buffer, buffer_size, 0, (struct sockaddr *) &cli_addr, &clilen);
          if(n < 0)
            error("ERROR recvfrom");
        
          DecodeTCPHeader(buffer, file_data,&sequence_number, &acknowledgement_number, &ACK, &SYN, &FIN, &window_size);


          //test file transfer
            int i = 0;
            for(;i<35;i++){
              fread(file_data +i, 1,1,fp ); 
            }
            
            bzero(buffer, buffer_size);
            EncodeTCPHeader(buffer, file_data,sequence_number, acknowledgement_number, ACK, SYN, FIN, window_size);
            n = sendto(sockfd, buffer, sizeof(buffer), 0, (struct sockaddr *) &cli_addr, clilen);
            printf("Sent1: %s\n",file_data );
           

            bzero(file_data,100);
            for(i = 0;i<35;i++){
              fread(file_data +i, 1,1,fp ); 
            }
            bzero(buffer, buffer_size);
            EncodeTCPHeader(buffer, file_data,sequence_number, acknowledgement_number, ACK, SYN, FIN, window_size);
            n = sendto(sockfd, buffer, sizeof(buffer), 0, (struct sockaddr *) &cli_addr, clilen);
            printf("Sent2: %s\n",file_data );
 
            fclose(fp);




        }else if (ACK) { //RECIEVED ACK
          //3 cases: part of 3 way handshake,  ack for closing, or ack for regular data,
          //TODO: fill in how to handle ACKS
          if(handshake){

          } else if(closing) {

          } else {

          }

        }else if (FIN) { //CLOSING CONNECTION
          //TODO: closing stuff
        } else { //RECEIVED DATA REQUEST

        }

      }
    }

  }

  close(sockfd);         
  return 0; 
}

