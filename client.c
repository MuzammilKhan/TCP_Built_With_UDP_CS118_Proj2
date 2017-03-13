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
void DecodeTCPHeader(char* msgp, char* data , unsigned short * bytes_read ,unsigned int* sequence_number, unsigned int* acknowledgement_number, unsigned short* ACK, unsigned short* SYN, unsigned short* FIN, unsigned short* window_size){
  bzero(data , 1000);
  memcpy(sequence_number, msgp, 4); 
  memcpy(acknowledgement_number, msgp+4, 4);
  memcpy(ACK, msgp+8, 2);
  memcpy(SYN, msgp+10, 2);
  memcpy(FIN, msgp+12, 2);
  memcpy(window_size, msgp+14, 2);
  memcpy(bytes_read, msgp+16, 2);  
  memcpy(data, msgp+18, 1000);
   printf("rec: %s\n",data );    
  
  return;
}

/*Encode "TCP header" at msgp. Set checksum and other fields not explicitly set here to 0*/
void EncodeTCPHeader(char* msgp, char* data ,unsigned short  bytes_read ,unsigned int sequence_number, unsigned int acknowledgement_number, unsigned short ACK, unsigned short SYN, unsigned short FIN, unsigned short window_size){
 	bzero(msgp,1000);
  memset(msgp, 0, 1024); //set header to 0
  memcpy(msgp, &sequence_number, 4);
  memcpy(msgp+4,&acknowledgement_number, 4);

  memcpy(msgp +8, &ACK, 2);
  memcpy(msgp+10,&SYN, 2);
  memcpy(msgp+12,&FIN, 2);
  memcpy(msgp+14, &window_size, 2);
   memcpy(msgp+16, &bytes_read ,2);
   memcpy(msgp+18 , data, bytes_read);
   printf("bytes: %u\n", *(msgp+16));
 
   printf("sent: %s\n", data);
 
  
  return;
}

int main(int argc, char **argv) {
    

    int max_packet_length = 1024; //same as server
    int mac_sequence_number = 30720; //same as server
    int rto_val = 500 * 1000; // This value will only change for the extra credit part

    FILE *fp;
    char file_data[1000];
    bzero(file_data ,1000);

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
    if (argc != 4) {
       fprintf(stderr,"usage: %s <hostname> <port> <filename>\n", argv[0]);
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

         
      memset(buf, 0, BUFSIZE);   //reset memory

      unsigned short source; 
      unsigned short destination;
      unsigned int sequence_number = 0;
      unsigned int acknowledgement_number = 0;
      unsigned short ACK;
      unsigned short SYN;
      unsigned short FIN;
      unsigned short window_size = 5120;
      unsigned short bytes_read = 0;
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


      if(firsttransmission) {
          ACK = 0;
          SYN = 1;
          FIN = 0;
                  
          acknowledgement_number = 0; // WHAT ABOUT THIS ONE?        
          sequence_number = 0; // CHANGE THIS

          bzero(buf, BUFSIZE);
          EncodeTCPHeader(buf, file_data,0,sequence_number, acknowledgement_number, ACK, SYN, FIN, window_size);
          printf("Sending packet SYN\n");
          n = sendto(sockfd, buf, sizeof(buf), 0, (const struct sockaddr* ) &serveraddr, serverlen);
          if (n < 0)  
            error("ERROR in three way handshake: sendto");

          fp = fopen("received.data" , "w");

          firsttransmission = 0;
          Timer.tv_sec = 0; //reset timer
          Timer.tv_usec = rto_val;        
      } else {

          //n = select(sockfd + 1, &active_fd_set, NULL, NULL, &Timer);
          n = select(sockfd + 1, &active_fd_set, NULL, NULL, NULL); //don't use timer yet for testing

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

            if(timeout) {
              timeout = 0; // reset timeout tracker 
              if (handshake || closing) {

              n = sendto(sockfd, buf, sizeof(buf), 0, (struct sockaddr *) &serveraddr, serverlen); // can resend like this as we havent changed the values of anything in the buffer yet
              if (n < 0)  
              error("ERROR in three way handshake: sendto");
              Timer.tv_sec = 0; //reset timer
              Timer.tv_usec = rto_val;          
              } else {
                //time out in data transmission 

              }
            }


          } else {         

              if (FD_ISSET(sockfd, &active_fd_set)) { // new connection request
                bzero(buf, BUFSIZE);
                n = recvfrom(sockfd, buf, BUFSIZE, 0, (struct sockaddr *) &serveraddr, &serverlen);
                if(n < 0)
                    error("ERROR recvfrom");
                
                DecodeTCPHeader(buf, file_data, &bytes_read,&sequence_number, &acknowledgement_number, &ACK, &SYN, &FIN, &window_size);
                printf("Receiving packet %d\n", sequence_number); // IS THIS RIGHT?


                if(SYN && ACK) {
                  //reply with ACK & filename
                  ACK = 1;
                  SYN = 0;
                  FIN = 0;
                    
                  int tmp = acknowledgement_number;      
                  acknowledgement_number = sequence_number + 1; // WHAT ABOUT THIS ONE?        
                  sequence_number = tmp; // CHANGE THIS

                  bzero(buf, BUFSIZE);
                  EncodeTCPHeader(buf, file_data,0,sequence_number, acknowledgement_number, ACK, SYN, FIN, window_size);

                  //put filename into buffer after pseudo tcp header
                  strncpy(buf+18, argv[3], strlen(argv[3])); //CHECK THIS!!!

                  printf("Sending packet %d\n", acknowledgement_number); // IS THIS RIGHT?
                  n = sendto(sockfd, buf, sizeof(buf), 0, (const struct sockaddr* ) &serveraddr, serverlen);
                  if (n < 0)  
                    error("ERROR in three way handshake: sendto");

                  handshake = 0;
                  Timer.tv_sec = 0; //reset timer
                  Timer.tv_usec = rto_val;   
                } else if (ACK) {
                  if(closing) { 
                          // what to do here?
                  } else {


                  }

                } else if (FIN) {
                  //send ack to server and begin timed wait with 2* RTO
                  ACK = 1;
                  SYN = 0;
                  FIN = 0;
                  
                  int tmp = acknowledgement_number;       
                  acknowledgement_number = sequence_number + 1; // WHAT ABOUT THIS ONE?        
                  sequence_number = tmp; // CHANGE THIS

                  bzero(buf, BUFSIZE);
                  EncodeTCPHeader(buf, file_data,0,sequence_number, acknowledgement_number, ACK, SYN, FIN, window_size);
                  printf("Sending packet %d\n", acknowledgement_number); // IS THIS RIGHT?
                  n = sendto(sockfd, buf, sizeof(buf), 0, (const struct sockaddr* ) &serveraddr, serverlen);
                  if (n < 0)  
                    error("ERROR in three way handshake: sendto");

                  firsttransmission = 0;
                  Timer.tv_sec = 0; //reset timer
                  Timer.tv_usec = 2 * rto_val; //FIX THIS VALUE

                } else {
                  // data transfer

                  //check if file completely received
                  //if so then inititate closing
                  //else do the following

                  //put data from buffer in file
                  printf("writing data to file\n");
                  int i = 0;
                  
                  printf("file_data: %s\n",file_data );
                  printf("num: %lu size: %lu\n",strlen(file_data) , sizeof(file_data) );
                  //fprintf(fp , "%s",file_data );
                  fwrite(file_data , 1 , bytes_read , fp);
                  //for(;i<1000;i++){ //Change upper limit later
                  //  fwrite( file_data + i, 1 , 1,fp); // ADJUST SIZE 
                  //}
                  fclose(fp);
                  //reply with ACK
                  ACK = 1;
                  SYN = 0;
                  FIN = 0;
                        
                  int tmp = acknowledgement_number;
                  acknowledgement_number = sequence_number + 1; // WHAT ABOUT THIS ONE?        
                  sequence_number = tmp; // CHANGE THIS

                  bzero(buf, BUFSIZE);
                  EncodeTCPHeader(buf, file_data,0,sequence_number, acknowledgement_number, ACK, SYN, FIN, window_size);
                  printf("Sending packet %d\n", acknowledgement_number); // IS THIS RIGHT?
                  n = sendto(sockfd, buf, sizeof(buf), 0, (const struct sockaddr* ) &serveraddr, serverlen);
                  if (n < 0)  
                    error("ERROR in three way handshake: sendto");

                  Timer.tv_sec = 0; //reset timer
                  Timer.tv_usec = rto_val; 
                }

    
              } 
          }
        }
    }
    fclose(fp);

    return 0;
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