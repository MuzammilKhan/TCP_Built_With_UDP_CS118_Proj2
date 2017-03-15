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
void DecodeTCPHeader(char* msgp, char* data , char* completed ,unsigned short * bytes_read ,unsigned int* sequence_number, unsigned int* acknowledgement_number, unsigned short* ACK, unsigned short* SYN, unsigned short* FIN, unsigned short* window_size){
  bzero(data , 1000);
  memcpy(sequence_number, msgp, 4); 
  memcpy(acknowledgement_number, msgp+4, 4);
  memcpy(ACK, msgp+8, 2);
  memcpy(SYN, msgp+10, 2);
  memcpy(FIN, msgp+12, 2);
  memcpy(window_size, msgp+14, 2);
  memcpy(bytes_read, msgp+16, 2);
  memcpy(completed, msgp+18, 1);  
  memcpy(data, msgp+19, 1000);
  printf("Receiving Packet ACK#: %d SEQ#: %d ACK: %u  FIN: %u SYN: %u \n\n"   ,*acknowledgement_number , *sequence_number , *ACK , *FIN , *SYN); 
   
  return;
}

/*Encode "TCP header" at msgp. Set checksum and other fields not explicitly set here to 0*/
void EncodeTCPHeader(char* msgp, char* data ,char completed ,unsigned short  bytes_read ,unsigned int sequence_number, unsigned int acknowledgement_number, unsigned short ACK, unsigned short SYN, unsigned short FIN, unsigned short window_size){
 	bzero(msgp,1000);
  memset(msgp, 0, 1024); //set header to 0
  memcpy(msgp, &sequence_number, 4);
  memcpy(msgp+4,&acknowledgement_number, 4);

  memcpy(msgp +8, &ACK, 2);
  memcpy(msgp+10,&SYN, 2);
  memcpy(msgp+12,&FIN, 2);
  memcpy(msgp+14, &window_size, 2);
   memcpy(msgp+16, &bytes_read ,2);
   memcpy(msgp+18, &completed ,1);
   memcpy(msgp+19 , data, bytes_read);
   
  printf("Sending Packet ACK#: %d SEQ#: %d ACK: %u  FIN: %u SYN: %u \n\n"   ,acknowledgement_number , sequence_number , ACK , FIN , SYN);
  
  return;
}

int main(int argc, char **argv) {
    

    int max_packet_length = 1024; //same as server
    int mac_sequence_number = 30720; //same as server
    int rto_val = 500 * 1000; // This value will only change for the extra credit part

    FILE *fp;
    char file_data[1000];
    bzero(file_data ,1000);
    char completed = '0';
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
      unsigned short w_size_num = window_size/1024;
      unsigned short bytes_read = 0;
      int handshake = 1;
      int closing = 0;
      int timeout = 0;
      int firsttransmission = 1;
      int d_buffer_size = w_size_num*1000;
      char data_buffer[d_buffer_size];
      bzero(data_buffer , d_buffer_size);


      int ooo_pkts =  0;
      int expected_seq_num = 4*1024
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
          EncodeTCPHeader(buf, file_data, completed,0,sequence_number, acknowledgement_number, ACK, SYN, FIN, window_size);
          
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
                
                DecodeTCPHeader(buf, file_data, &completed,&bytes_read,&sequence_number, &acknowledgement_number, &ACK, &SYN, &FIN, &window_size);
                

                if(SYN && ACK) {
                  //reply with ACK & filename
                  ACK = 1;
                  SYN = 0;
                  FIN = 0;
                    
                  int tmp = acknowledgement_number;      
                  acknowledgement_number = sequence_number + 1; // WHAT ABOUT THIS ONE?        
                  sequence_number = tmp; // CHANGE THIS

                  bzero(buf, BUFSIZE);
                  EncodeTCPHeader(buf, file_data,completed,0,sequence_number, acknowledgement_number, ACK, SYN, FIN, window_size);

                  //put filename into buffer after pseudo tcp header
                  strncpy(buf+19, argv[3], strlen(argv[3])); //CHECK THIS!!!

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
                  EncodeTCPHeader(buf, file_data,completed,0,sequence_number, acknowledgement_number, ACK, SYN, FIN, window_size);
                  n = sendto(sockfd, buf, sizeof(buf), 0, (const struct sockaddr* ) &serveraddr, serverlen);
                  if (n < 0)  
                    error("ERROR in three way handshake: sendto");

                  firsttransmission = 0;
                  Timer.tv_sec = 0; //reset timer
                  Timer.tv_usec = 2 * rto_val; //FIX THIS VALUE

                } else {
                  
                  
                  int i = 0;
                  
                  //if expected seq_num == seq_num, write to file
                  //else write to buffer in correct order. 
                  //initial seq num = 3
                  printf("seq_num: %d   ack_num: %d bytes_read: %u\n\n",sequence_number,acknowledgement_number,bytes_read );
                    expected_seq_num = sequence_number;


                  if(expected_seq_num == sequence_number){  
                    fwrite(file_data , 1 , bytes_read , fp);
                      expected_seq_num = acknowledgement_number +1;
                   //int i = 0;
                    //for(;i < d_buffer_size - 1000; i++){
                    //  data_buffer[i] = data_buffer[i+1000];
                    //}
                    
                  }
                  else{
                    int diff = sequence_number - expected_seq_num;
                    ooo_pkts++;
                    //memcpy(data_buffer+(diff * 1000) , file_data ,1000);
                  }

                  
                  ACK = 1;
                  SYN = 0;
                  FIN = 0;
                        
                  int tmp = acknowledgement_number;
                  acknowledgement_number = sequence_number + 1; // WHAT ABOUT THIS ONE?        
                  sequence_number = tmp; // CHANGE THIS

                  bzero(buf, BUFSIZE);
                  EncodeTCPHeader(buf, file_data,completed,0,sequence_number, acknowledgement_number, ACK, SYN, FIN, window_size);
                  n = sendto(sockfd, buf, sizeof(buf), 0, (const struct sockaddr* ) &serveraddr, serverlen);
                  if (n < 0)  
                    error("ERROR in three way handshake: sendto");

                  
                  Timer.tv_sec = 0; //reset timer
                  Timer.tv_usec = rto_val; 


                  if(completed == '1'){
                  		printf("Sending client FIN\n");
                  		FIN = 1;
                  		ACK = 0;
                  		SYN = 0;
					          EncodeTCPHeader(buf, file_data,completed,0,sequence_number, acknowledgement_number, ACK, SYN, FIN, window_size);
	                  n = sendto(sockfd, buf, sizeof(buf), 0, (const struct sockaddr* ) &serveraddr, serverlen);
	                  if (n < 0)  
	                    error("ERROR in FIN init");

	                   bzero(buf, BUFSIZE);
		                n = recvfrom(sockfd, buf, BUFSIZE, 0, (struct sockaddr *) &serveraddr, &serverlen);
		                if(n < 0)
		                    error("ERROR recvfrom");
		                
		                DecodeTCPHeader(buf, file_data, &completed,&bytes_read,&sequence_number, &acknowledgement_number, &ACK, &SYN, &FIN, &window_size);
		                
		                if(ACK == 1 && FIN == 0){
		                	bzero(buf, BUFSIZE);
			                printf("Recvd server ACK\n");
			                n = recvfrom(sockfd, buf, BUFSIZE, 0, (struct sockaddr *) &serveraddr, &serverlen);
			                if(n < 0)
			                    error("ERROR recvfrom");
			                
			                DecodeTCPHeader(buf, file_data, &completed,&bytes_read,&sequence_number, &acknowledgement_number, &ACK, &SYN, &FIN, &window_size);
			                
			                if(FIN == 1){
			                	printf("Recvd server FIN\n");
			                	  FIN = 0;
		                  		ACK = 1;
		                  		SYN = 0;
        							  printf("Sending client ACK\n");
        							  EncodeTCPHeader(buf, file_data,completed,0,sequence_number, acknowledgement_number, ACK, SYN, FIN, window_size);
			                  n = sendto(sockfd, buf, sizeof(buf), 0, (const struct sockaddr* ) &serveraddr, serverlen);
			                  if (n < 0)  
			                    error("ERROR in FIN init");
			                fclose(fp);
			                return 0 ;

			                }
		                }

                  }
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