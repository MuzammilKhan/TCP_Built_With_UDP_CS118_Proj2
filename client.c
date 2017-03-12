/* 
 * udpclient.c - A simple UDP client
 * usage: udpclient <host> <port>
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h> 
#include <netdb.h>


#define BUFSIZE 1024

/* 
 * error - wrapper for perror
 */
void error(char *msg) {
    perror(msg);
    exit(0);
}


/*Decode basic "TCP header" at msgp and get desired values. Following data types chosen so that encoding is easy later. Using referenced
data types instead of pointers for clarity about sizes. ACK, SYN, and FIN use only 1 bit. Ignoring checksum and other fields. */
void DecodeTCPHeader(char* msgp, char* data ,unsigned int* sequence_number, unsigned int* acknowledgement_number, unsigned short* ACK, unsigned short* SYN, unsigned short* FIN, unsigned short* window_size){
  
  memcpy(sequence_number, msgp, 4); 
  memcpy(acknowledgement_number, msgp+4, 4);
  memcpy(ACK, msgp+8, 2);
  memcpy(SYN, msgp+10, 2);
  memcpy(FIN, msgp+12, 2);
  memcpy(window_size, msgp+14, 2);  
  memcpy(data, msgp+16,50);
   printf("rec: %s\n",data );    
  printf("cli_seq_rec: %d\n", *sequence_number ); //debugging
  printf("cli_ack_rec: %d\n", *acknowledgement_number ); //debugging
  printf("cli_SYN_rec: %u\n\n", *SYN );
  return;
}

/*Encode "TCP header" at msgp. Set checksum and other fields not explicitly set here to 0*/
void EncodeTCPHeader(char* msgp, char* data ,unsigned int sequence_number, unsigned int acknowledgement_number, unsigned short ACK, unsigned short SYN, unsigned short FIN, unsigned short window_size){
  printf("cli_seq_sent: %u\n",sequence_number);
  printf("cli_ack_sent: %u\n",acknowledgement_number);
  printf("cli_SYN_sent: %u\n\n",SYN );
  memset(msgp, 0, 1024); //set header to 0
  memcpy(msgp, &sequence_number, 4);
  memcpy(msgp+4,&acknowledgement_number, 4);

  memcpy(msgp +8, &ACK, 2);
  memcpy(msgp+10,&SYN, 2);
  memcpy(msgp+12,&FIN, 2);
  memcpy(msgp+14, &window_size, 2);
 
  
  return;
}

int main(int argc, char **argv) {
    

    int max_packet_length = 1024; //same as server
    int mac_sequence_number = 30720; //same as server
    int rto_val = 500 * 1000; // This value will only change for the extra credit part


    int sockfd, portno, n;
    int serverlen;
    struct sockaddr_in serveraddr;
    struct hostent *server;
    char *hostname;
    char buf[BUFSIZE];
    char file_buffer[1000];
    struct timeval Timer;
    fd_set active_fd_set;
    /* check command line arguments */
    if (argc != 3) {
       fprintf(stderr,"usage: %s <hostname> <port>\n", argv[0]);
       exit(0);
    }
    hostname = argv[1];
    portno = atoi(argv[2]);

    /* socket: create the socket */
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) 
        error("ERROR opening socket");

    /* gethostbyname: get the server's DNS entry */
    server = gethostbyname(hostname);
    if (server == NULL) {
        fprintf(stderr,"ERROR, no such host as %s\n", hostname);
        exit(0);
    }

    /* build the server's Internet address */
    bzero((char *) &serveraddr, sizeof(serveraddr));
    serveraddr.sin_family = AF_INET;
    bcopy((char *)server->h_addr, 
      (char *)&serveraddr.sin_addr.s_addr, server->h_length);
    serveraddr.sin_port = htons(portno);


      int buffer_size = 256;
      char buffer[buffer_size];          
      memset(buffer, 0, buffer_size);   //reset memory

      unsigned short source; 
      unsigned short destination;
      unsigned int sequence_number = 0;
      unsigned int acknowledgement_number = 0;
      unsigned short ACK;
      unsigned short SYN;
      unsigned short FIN;
      unsigned short window_size = 5120;

      int handshake = 1;
      int closing = 0;
      int timeout = 0;
      int firsttransmission = 1;

      //we are using a hard coded port for our application so source = dest port
      source = portno;
      destination = source;
      int connected = 0;
      char temp_buf[512];
      serverlen = sizeof(serveraddr);

        Timer.tv_sec = 0;
      Timer.tv_usec = rto_val;    
    while(1){
    /* get a message from the user */
        
      FD_ZERO(&active_fd_set);
      FD_SET(sockfd, &active_fd_set);

      n = select(sockfd + 1, &active_fd_set, NULL, NULL, &Timer);

      bzero(buf, BUFSIZE);
      bzero(temp_buf , BUFSIZE);
      /*
      printf("Please enter msg: ");
      fgets(temp_buf, BUFSIZE, stdin);
      bzero(file_buffer,1000);
      int rem = remove("rec.txt");
      if(rem != 0)
        printf("Error in deleting file rec.txt\n");
   
        FILE* fp;
        
        fp = fopen("rec.txt" , "a");
        */


    if (n < 0) {
      close(sockfd);
      //ERROR OCCURED TODO: handle error
    } else if (n == 0){
      //go through timers and check for timeout
      //if timeout occurs set timeout variable to 1
    } else {         

      if(timeout) {
        timeout = 0; // reset timeout tracker 
        if (handshake || closing) {
        n = sendto(sockfd, buffer, sizeof(buffer), 0, (struct sockaddr *) &cli_addr, clilen); // can resend like this as we havent changed the values of anything in the buffer yet
        if (n < 0)  
        error("ERROR in three way handshake: sendto");
        Timer.tv_sec = 0; //reset timer
        Timer.tv_usec = rto_val;          
        } else {
          //time out in data transmission 

        }
      }
        

      if(firsttransmission) {
          ACK = 0;
          SYN = 1;
          FIN = 0;
          
          
          acknowledgement_number = 0; // WHAT ABOUT THIS ONE?
        
          sequence_number = 5000; // CHANGE THIS

          bzero(buffer, buffer_size);
          EncodeTCPHeader(buffer, file_data,sequence_number, acknowledgement_number, ACK, SYN, FIN, window_size);
          n = sendto(sockfd, buf, sizeof(buf), 0, (const struct sockaddr* ) &serveraddr, serverlen);
          if (n < 0)  
            error("ERROR in three way handshake: sendto");

          firsttransmission = 0;
          Timer.tv_sec = 0; //reset timer
          Timer.tv_usec = rto_val;        
      } else{

        if (FD_ISSET(sockfd, &active_fd_set)) { // new connection request
          bzero(buffer, buffer_size);
        n = recvfrom(sockfd, buffer, buffer_size, 0, (struct sockaddr *) &serveraddr, &serverlen);
        if(n < 0)
            error("ERROR recvfrom");
        
        DecodeTCPHeader(buffer, file_data, &sequence_number, &acknowledgement_number, &ACK, &SYN, &FIN, &window_size);


        if(SYN && ACK) {
          //reply with ACK

        } else if (ACK) {
          //2 cases: either data transfer or part of closing

        } else if (FIN) {
          //send ack to server and begin timed wait

        } else {

        }



        }

         
      } 

    /*
      if(connected == 0){
          SYN = 1;
          ACK = 0;
          FIN = 0;
          sequence_number = 2000;
          
          EncodeTCPHeader(buf, file_buffer,sequence_number, acknowledgement_number, ACK, SYN, FIN, window_size );          
          n = sendto(sockfd, buf, sizeof(buf), 0, (const struct sockaddr* ) &serveraddr, serverlen);
          if (n < 0) 
            error("ERROR in sendto");
          bzero(buf, BUFSIZE);
          n = recvfrom(sockfd, buf, sizeof(buf), 0, ( struct sockaddr*  ) &serveraddr, &serverlen);
          DecodeTCPHeader(buf,  file_buffer,&sequence_number, &acknowledgement_number, &ACK, &SYN, &FIN,  &window_size);
          
          if (n < 0) 
            error("ERROR in recvfrom");
          
          SYN = 0;
          ACK = 1;
          acknowledgement_number = sequence_number + 1;
          sequence_number = 2001;
          
          bzero(buf, BUFSIZE);
          EncodeTCPHeader(buf,file_buffer ,sequence_number, acknowledgement_number, ACK, SYN, FIN, window_size);          
          n = sendto(sockfd, buf, sizeof(buf), 0, (const struct sockaddr* ) &serveraddr, serverlen);
          if (n < 0) 
            error("ERROR in sendto");


          bzero(buf, BUFSIZE);
          bzero(file_buffer , 100);
          n = recvfrom(sockfd, buf, sizeof(buf), 0, ( struct sockaddr*  ) &serveraddr, &serverlen);
          DecodeTCPHeader(buf,  file_buffer,&sequence_number, &acknowledgement_number, &ACK, &SYN, &FIN,  &window_size);
          

          //fwrite( file_buffer, 1 , sizeof(file_buffer),fp);
          fprintf(fp, "%s",file_buffer );
          printf("Rec1: %s\n", file_buffer);

          bzero(buf, BUFSIZE);
          n = recvfrom(sockfd, buf, sizeof(buf), 0, ( struct sockaddr*  ) &serveraddr, &serverlen);
          DecodeTCPHeader(buf,  file_buffer,&sequence_number, &acknowledgement_number, &ACK, &SYN, &FIN,  &window_size);
         

         
          fprintf(fp, "%s",file_buffer );
          //fwrite( file_buffer, 1 , sizeof(file_buffer),fp);
            printf("Rec2: %s\n", file_buffer);       
          connected =1;
          fclose(fp);
      }


      //test file transfer
      */

    }
       


/*
        EncodeTCPHeader(buf, sequence_number, acknowledgement_number, ACK, SYN, FIN, window_size);
        
        
        n = sendto(sockfd, buf, sizeof(buf), 0, (const struct sockaddr* ) &serveraddr, serverlen);
        if (n < 0) 
          error("ERROR in sendto");
        
        
        bzero(buf, BUFSIZE);
        n = recvfrom(sockfd, buf, sizeof(buf), 0, ( struct sockaddr*  ) &serveraddr, &serverlen);
        DecodeTCPHeader(buf,  &sequence_number, &acknowledgement_number, &ACK, &SYN, &FIN,  &window_size);
          
        if (n < 0) 
          error("ERROR in recvfrom");
        
 */      
        
    }
    return 0;
}