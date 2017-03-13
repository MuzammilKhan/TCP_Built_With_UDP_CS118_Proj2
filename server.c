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
void DecodeTCPHeader(char* msgp, char* data, char *completed,unsigned short * bytes_read ,unsigned int* sequence_number, unsigned int* acknowledgement_number, unsigned short* ACK, unsigned short* SYN, unsigned short* FIN, unsigned short* window_size){
  bzero(data , 1000);
  memcpy(sequence_number, msgp, 4); 
  memcpy(acknowledgement_number, msgp+4, 4);
  memcpy(ACK, msgp+8, 2);
  memcpy(SYN, msgp+10, 2);
  memcpy(FIN, msgp+12, 2);  
  memcpy(window_size, msgp+14, 2);
  memcpy(bytes_read , msgp+16, 2);
  memcpy(completed , msgp+18, 1);
  
  printf("Receiving Packet \n\n");
 
  return;
}

/*Encode "TCP header" at msgp. Set checksum and other fields not explicitly set here to 0*/
void EncodeTCPHeader(char* msgp,char* data , char completed,unsigned short  bytes_read ,unsigned int sequence_number, unsigned int acknowledgement_number, unsigned short ACK, unsigned short SYN, unsigned short FIN, unsigned short window_size){
  bzero(msgp , 1024);
  memset(msgp, 0, 1024); //set header to 0
  memcpy(msgp, &sequence_number, 4);
  memcpy(msgp+4,&acknowledgement_number, 4);

  memcpy(msgp +8, &ACK, 2);
  memcpy(msgp+10,&SYN, 2);
  memcpy(msgp+12,&FIN, 2);
  
  memcpy(msgp+14, &window_size, 2);
  memcpy(msgp+16, &bytes_read, 2);
  memcpy(msgp+18, &completed, 1);
   memcpy(msgp+19 , data, bytes_read);
   printf("completed: %c\n", completed);
   printf("Sending Packet\n\n");
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
  char completed = '0';
  //timer
  struct timeval Timer;

  int sockfd, portno, pid;
  socklen_t clilen;
  struct sockaddr_in serv_addr, cli_addr;
  int running = 1; //set to 0 when closing server
  fd_set active_fd_set; // set for holding sockets

  if (argc < 2) {
    fprintf(stderr,"usage: %s <port>\n", argv[0]);
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
  unsigned short bytes_read = 0;
  struct hostent *hostp;
  int handshake = 0;
  int closing = 0;
  
  FILE *fp;
 
  //we are using a hard coded port for our application so source = dest port
  source = portno;
  destination = source;
  char file_data[1000];
  bzero(file_data ,1000);

     
  
  Timer.tv_sec = 0;
  Timer.tv_usec = rto_val;

  while(running){
    
    FD_ZERO(&active_fd_set);
    FD_SET(sockfd, &active_fd_set);
    clilen = sizeof(cli_addr);
    //n = select(sockfd + 1, &active_fd_set, NULL, NULL, &Timer);
    n = select(sockfd + 1, &active_fd_set, NULL, NULL, NULL); //don't use timer yet for testing
    
    if (n < 0) {
      close(sockfd);
      //ERROR OCCURED TODO: handle error
    } else if (n == 0 && handshake) { //timeout occured during handshake 
      //resend SYNACK to client
        n = sendto(sockfd, buffer, sizeof(buffer), 0, (struct sockaddr *) &cli_addr, clilen); // can resend like this as we havent changed the values of anything in the buffer yet
        if (n < 0)  
        error("ERROR in three way handshake: sendto");
        Timer.tv_sec = 0; //reset timer
        Timer.tv_usec = rto_val;
    } else if (n == 0 && closing) { //timeout occured during closing
      // resend ACK & FIN to client  

        Timer.tv_sec = 0; //reset timer
        Timer.tv_usec = rto_val;
    } else { 
      if (FD_ISSET(sockfd, &active_fd_set)) { // new connection request
        bzero(buffer, buffer_size);
        n = recvfrom(sockfd, buffer, buffer_size, 0, (struct sockaddr *) &cli_addr, &clilen);
        if(n < 0)
            error("ERROR recvfrom");
        
        DecodeTCPHeader(buffer, file_data,&completed ,&bytes_read ,&sequence_number, &acknowledgement_number, &ACK, &SYN, &FIN, &window_size);
        

        if(SYN){ // THREE WAY HANDSHAKE
         
          ACK = 1;
          SYN = 1;
          FIN = 0;
          
          
          acknowledgement_number = sequence_number + 1; 
        
          sequence_number = 0;
          handshake = 1;
          //respond to client
          bzero(buffer, buffer_size);
          EncodeTCPHeader(buffer, file_data, completed,0,sequence_number, acknowledgement_number, ACK, SYN, FIN, window_size);
          n = sendto(sockfd, buffer, sizeof(buffer), 0, (struct sockaddr *) &cli_addr, clilen);
          if (n < 0)  
            error("ERROR in three way handshake: sendto");

          Timer.tv_sec = 0; //reset timer
          Timer.tv_usec = rto_val;
        
         

        }else if (ACK) { //RECIEVED ACK
          //3 cases: part of 3 way handshake,  ack for closing, or ack for regular data,
          //TODO: fill in how to handle ACKS

//Pseudocode for tcp congestion control
  //TODO: all variables used in this need to be initialized somewhere before
  //TODO: need to consider case where ack may overflow 

          //Determine cwnd and ssthresh
          if(ss) { //Slow Start
            if(acknowledgement_number > prev_acknowledgement_number ){
              prev_acknowledgement_number = acknowledgement_number;
              duplicate_ack_count = 0;
              cwnd = cwnd + 1;
              if(cwnd > ssthresh){
                ss = 0;
                ca = 1;
              }
            }else {
              duplicate_ack_count = duplicate_ack_count + 1;
              if(duplicate_ack_count == 3) { // go to fast retransmit
                duplicate_ack_count = 0;
                ss = 0;
                fastretransmit = 1;
                goto fastretransmit_label;
              }
            } 

          }else if (ca) { //Congestion Avoidance
            if(acknowledgement_number > prev_acknowledgement_number ){
              prev_acknowledgement_number = acknowledgement_number;
              duplicate_ack_count = 0;
              ca_acks_count = ca_acks_count + 1;
              if(cwnd == ca_acks_count){
                ca_acks_count = 0;
                cwnd = cwnd + 1;
              }             
            } else{
              duplicate_ack_count = duplicate_ack_count + 1;
              if(duplicate_ack_count == 3) { // go to fast retransmit
                duplicate_ack_count = 0;
                ca = 0;
                fastretransmit = 1;
                goto fastretransmit_label
              }             
            }

          }else if (fastretransmit){ //Fast Retransmit
            fastretransmit_label;
            ssthresh = max(cwnd/2 , 2); //cwnd/2 should automatically floor in C
            ssthresh = ssthresh + 3;
            retransmit_packet = 1;
            fastretransmit = 0;
            fastrecovery = 1;

          }else if (fastrecovery){ //Fast Recovery
              if(acknowledgement_number > prev_acknowledgement_number) {
                prev_acknowledgement_number = acknowledgement_number;
                duplicate_ack_count = 0;
                fastrecovery = 0;
                cwnd = ssthresh;
                ss = 1;
              }else{ //duplicate ack
                cwnd = cwnd + 1;
              }

          } else { //Error checking - should never be entered
            //error - we should never enter this
          }
          //cwnd and ssthresh determined by this point


          //send packets
          if(retransmit_packet){
            retransmit_packet = 0;
            //retransmit lost packet

          } else {
            //send packets according to cwnd

            //remove elements we already have an ACK for from sliding window


            //put in new sequence numbers to send in sliding window

            //send new elements

          }



//end pseudocode








          if(handshake){
            //handshake complete at this point
            handshake = 0;
            ACK = 0;
            SYN = 0;
            FIN = 0;  
            // read filename from buffer
            // open file if it exists
            // start sedning data
            
            fp = fopen(buffer + 19 , "r"); 
            if(fp == NULL){
              //if it doesnt exist need to start closing connection?
              error("File doesn't exist"); // Error checking, actually need to implement closing later
            }


              int i = 0;
              
              for(;i<1000;i++){ //Change upper limit later
                 n = fread(file_data +i, 1,1,fp ); 
                 if(n != 1){ //n != size of elements means we read whole file
                  completed ='1';
                  break;
                 }
                 bytes_read++;
                 
              }
              printf("bytes_read: %d\n", bytes_read );
              bzero(buffer, buffer_size);
              EncodeTCPHeader(buffer, file_data, completed,bytes_read, sequence_number, acknowledgement_number, ACK, SYN, FIN, window_size);
              n = sendto(sockfd, buffer, sizeof(buffer), 0, (struct sockaddr *) &cli_addr, clilen);




          } else if(closing) {
            //Close server at this point
              close(sockfd);         
              return 0; 

          } else {
            //adjust window stuff
          
          }

        }else if (FIN) { //CLOSING CONNECTION
          //TODO: closing stuff
          printf("In FIN in server.\n");
          closing = 1;
          ACK = 1;
          SYN = 0;
          FIN = 0;        
          
          int tmp = acknowledgement_number;
          acknowledgement_number = sequence_number + 1;         
          sequence_number = tmp; //CHANGE THIS

          //respond to client
          bzero(buffer, buffer_size);
          EncodeTCPHeader(buffer, file_data,completed ,0,sequence_number, acknowledgement_number, ACK, SYN, FIN, window_size);
          n = sendto(sockfd, buffer, sizeof(buffer), 0, (struct sockaddr *) &cli_addr, clilen);
          if (n < 0)  
            error("ERROR in three way handshake: sendto");

          //close wait stuff????
          ACK = 0;
          SYN = 0;
          FIN = 1;        
          
          //int tmp = acknowledgement_number;
          //acknowledgement_number = sequence_number + 1; //CHANGE THIS - WHAT SHOULD THIS BE        
          //sequence_number = tmp; //CHANGE THIS - WHAT SHOULD THIS BE       

          //respond to client
          bzero(buffer, buffer_size);
          EncodeTCPHeader(buffer, file_data, completed,0,sequence_number, acknowledgement_number, ACK, SYN, FIN, window_size);
          n = sendto(sockfd, buffer, sizeof(buffer), 0, (struct sockaddr *) &cli_addr, clilen);
          if (n < 0)  
            error("ERROR in three way handshake: sendto");
          
          Timer.tv_sec = 0; //reset timer
          Timer.tv_usec = rto_val;
          bzero(buffer, buffer_size);
          n = recvfrom(sockfd, buffer, buffer_size, 0, (struct sockaddr *) &cli_addr, &clilen);
          if(n < 0)
              error("ERROR recvfrom");
          
          DecodeTCPHeader(buffer, file_data,&completed ,&bytes_read ,&sequence_number, &acknowledgement_number, &ACK, &SYN, &FIN, &window_size);
          
          if(ACK == 1){
            printf("Recvd client ACK and closing socket\n");
            close(sockfd);
            fclose(fp);
            return 0;

          }


        } else { 

        }

      }
    }

  }


}

