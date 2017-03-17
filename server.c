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

int max(int a, int b){
  if (a > b)
    return a;
  else
    return b;
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
  //memcpy(completed , msgp+18, 1);
  
  printf("Receiving Packet ACK#: %d SEQ#: %d ACK: %u  FIN: %u SYN: %u \n\n"   ,*acknowledgement_number , *sequence_number , *ACK , *FIN , *SYN);
 
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
  //memcpy(msgp+18, &completed, 1);
   memcpy(msgp+19 , data, bytes_read);
  
   printf("Sending Packet ACK#: %d SEQ#: %d ACK: %u  FIN: %u SYN: %u comp: %c bytes: %u\n\n"   ,acknowledgement_number , sequence_number , ACK , FIN , SYN ,completed,bytes_read);
  return;
}

//basic udp server reference: https://www.cs.cmu.edu/afs/cs/academic/class/15213-f99/www/class26/udpserver.c

int main(int argc, char *argv[])
{
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

  double elapsedTime = 0;
  struct timeval t1 ,t2;
  // restrictions for our TCP implementation
  int max_packet_length = 1024; //includes header, in bytes
  int max_sequence_number = 30720; //bytes    
  int window_size_req = 5120; //bytes
  int rto_val = 500; //in ms
  char completed = '0';
  int cwnd = 5;
  int ssthresh = (window_size_req/max_packet_length) * 3 / 4; // TODO: Test this value
  int ss = 1;
  int ca = 0;
  int fastretransmit = 0;
  int fastrecovery = 0;
  int duplicate_ack_count = 0;
  int ca_acks_count = 0;
  int retransmit_packet = 0;
  int sliding_window[window_size_req/max_packet_length];
  struct timeval timer_window[window_size_req/max_packet_length];
  unsigned short source; 
  unsigned short destination;
  unsigned int sequence_number;
  unsigned int acknowledgement_number;
  unsigned int prev_acknowledgement_number = -1;
  unsigned short ACK;
  unsigned short SYN;
  unsigned short FIN;
  unsigned short window_size = window_size_req/max_packet_length; // change
  unsigned short bytes_read = 0;
  struct hostent *hostp;
  int handshake = 0;
  int closing = 0;  
  FILE *fp;
  int connected = 0;

  unsigned int last_file_ack_number = 0; //set when last data packet from file sent out
  unsigned int latest_sequence_number = 0;

  //timer
  struct timeval Timer;
 
  //we are using a hard coded port for our application so source = dest port
  source = portno;
  destination = source;
  char file_data[1000];
  bzero(file_data ,1000);

     
  
  Timer.tv_sec = 0;
  Timer.tv_usec = rto_val/8;

  while(running){
    
    FD_ZERO(&active_fd_set);
    FD_SET(sockfd, &active_fd_set);
    clilen = sizeof(cli_addr);
    n = select(sockfd + 1, &active_fd_set, NULL, NULL, &Timer);
    //n = select(sockfd + 1, &active_fd_set, NULL, NULL, NULL); //don't use timer yet for testing
    
    if (n < 0) {
      close(sockfd);
      //ERROR OCCURED TODO: handle error
    } else if (n == 0 && handshake) { //timeout occured during handshake 
      //resend SYNACK to client
      gettimeofday(&t2, NULL);
      elapsedTime = (t2.tv_sec - t1.tv_sec) * 1000.0;      // sec to ms
      elapsedTime += (t2.tv_usec - t1.tv_usec) / 1000.0;   // us to ms
      printf("Elapsed Time: %f\n", elapsedTime);
      if(elapsedTime >= rto_val){
        gettimeofday(&t1, NULL);
        n = sendto(sockfd, buffer, sizeof(buffer), 0, (struct sockaddr *) &cli_addr, clilen); // can resend like this as we havent changed the values of anything in the buffer yet
        if (n < 0)  
        error("ERROR in three way handshake: sendto");                  
      }
        Timer.tv_sec = 0; //reset timer
        Timer.tv_usec = rto_val/8;
    } else if (n == 0 && closing) { //timeout occured during closing
      // resend ACK & FIN to client  

        Timer.tv_sec = 0; //reset timer
        Timer.tv_usec = rto_val/8;
    } else if(n == 0 && connected ==1){ //check data timers
      int i = 0;
      
      for(; i < cwnd; i++){
                //printf("Loop i: %d\n",i );          
                gettimeofday(&t2, NULL);
                elapsedTime = (t2.tv_sec - timer_window[i].tv_sec) * 1000.0;      // sec to ms
                elapsedTime += (t2.tv_usec - timer_window[i].tv_usec) / 1000.0;   // us to ms
                //printf("Elapsed Time: %f\n", elapsedTime);

        if(elapsedTime >= rto_val && sliding_window[i] != 0){
         printf("Retransmitting \n");
          //retransmit lost packet
          unsigned int dropped_seq = sliding_window[i];
          //If we are going to have timeout come here then we need a way to check that and if so update dropped_seq accordingly AND FLAGS somehow
          int pkt_24_offset_1 = (dropped_seq/max_packet_length) * 24;
          int pkt_24_offset_2 = (latest_sequence_number/max_packet_length)*24;
          printf("dropped_seq: %d latest_sequence_number: %d pkt_24_offset_1: %d pkt_24_offset_2: %d\n",dropped_seq,latest_sequence_number , pkt_24_offset_1 , pkt_24_offset_2 );
          //fseek(fp, dropped_seq - latest_sequence_number - pkt_24_offset_1 - pkt_24_offset_2, SEEK_CUR);
          fseek(fp, dropped_seq - pkt_24_offset_1 -1000, SEEK_SET);
          //send corresponding packet
          int bytes_read = 0;
          int p = 0;

          for(;p<1000;p++){ //Change upper limit later
            n = fread(file_data +p, 1,1,fp ); 
            if(n != 1){ //n != size of elements means we read whole file
                completed ='1';
                last_file_ack_number = sliding_window[i] + 1; //Set last file ack number here
                break;
            }
            bytes_read++;         
          } 

          ACK = 0;
          SYN = 0;
          FIN = 0;
          
          sequence_number = sliding_window[i];
          EncodeTCPHeader(buffer, file_data, completed,bytes_read, sequence_number, acknowledgement_number, ACK, SYN, FIN, window_size);
          gettimeofday(&timer_window[i], NULL); // reset corresponding timer
          n = sendto(sockfd, buffer, sizeof(buffer), 0, (struct sockaddr *) &cli_addr, clilen);
          //fseek(fp, latest_sequence_number - dropped_seq + bytes_read - pkt_24_offset_1 - pkt_24_offset_2, SEEK_CUR);
          fseek(fp, latest_sequence_number - pkt_24_offset_2, SEEK_SET);
          
          printf("Retransmitting seq_num: %d  index: %d\n", sequence_number,i);
          break;
          }
      }

      Timer.tv_sec = 0;
      Timer.tv_usec = rto_val/8;

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
          connected =1;
          
          acknowledgement_number = sequence_number + 1; 
        
          sequence_number = 0;
          handshake = 1;
          //respond to client
          bzero(buffer, buffer_size);
          EncodeTCPHeader(buffer, file_data, completed,0,sequence_number, acknowledgement_number, ACK, SYN, FIN, window_size);
          gettimeofday(&t1, NULL);
          n = sendto(sockfd, buffer, sizeof(buffer), 0, (struct sockaddr *) &cli_addr, clilen);
          if (n < 0)  
            error("ERROR in three way handshake: sendto");
      
         

        }else if (ACK) { //RECIEVED ACK
          //3 cases: part of 3 way handshake,  ack for closing, or ack for regular data,
          printf("ack_num: %d  last_num: %d\n", acknowledgement_number , last_file_ack_number);
          if(completed == '1' && acknowledgement_number == last_file_ack_number){
            //TODO: Have server start connection teardown 
                      printf("Sending client FIN\n");
                      FIN = 1;
                      ACK = 0;
                      SYN = 0;
                    EncodeTCPHeader(buffer, file_data,completed,0,sequence_number, acknowledgement_number, ACK, SYN, FIN, window_size);
                    n = sendto(sockfd, buffer, sizeof(buffer), 0, (const struct sockaddr* ) &cli_addr, clilen);
                    if (n < 0)  
                      error("ERROR in FIN init");

                    closing = 1;



          }else if(handshake){
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

              //initialize the sliding window
            
                    int j = 0;
                    for(; j < cwnd ; j++){
                        if(j != 0){
                          sliding_window[j] = sliding_window[j-1] + max_packet_length;
                        }else {
                          sliding_window[j] = sequence_number;
                        }
                        //send corresponding packet
                        int bytes_read = 0;
                        int i = 0;
                        for(;i<1000;i++){ //Change upper limit later
                            if(completed == '0'){
                              n = fread(file_data +i, 1,1,fp ); 
                              if(n != 1){ //n != size of elements means we read whole file
                                
                                completed ='1';
                                last_file_ack_number = sliding_window[j] + 1; //Set last file ack number here
                                printf("Read the last bytes. :%d\n\n\n\n\n\n\n\n\n\n\n\n\n" ,last_file_ack_number );
                                break;
                              }
                              bytes_read++;
                            }
                        }     
                        
                        sequence_number = sliding_window[j]; 
                        latest_sequence_number = sequence_number;
                        ACK = 0;
                        SYN = 0;
                        FIN = 0;
                        
                        EncodeTCPHeader(buffer, file_data, completed,bytes_read, sequence_number, acknowledgement_number, ACK, SYN, FIN, window_size);
                        //Initialize corresponding timer
                        printf("Got here j: %d\n",j);
                        gettimeofday(timer_window +j, NULL); 
                        printf("After time\n");
                        n = sendto(sockfd, buffer, sizeof(buffer), 0, (struct sockaddr *) &cli_addr, clilen);

                        if(completed == '1'){
                          break;
                        }

                    }
            

            //goto data_transfer_label;

            } else if(closing) {
   


            } else {
              //Data Transfer
                data_transfer_label:

                //send packets
                if(retransmit_packet){
                  retransmit_packet = 0;
                  //retransmit lost packet
                  unsigned int dropped_seq = 0;
                  //If we are going to have timeout come here then we need a way to check that and if so update dropped_seq accordingly AND FLAGS somehow
                  fseek(fp, dropped_seq - latest_sequence_number, SEEK_CUR);
                  //send corresponding packet
                  int bytes_read = 0;
                  int i = 0;
                  for(;i<1000;i++){ //Change upper limit later
                    n = fread(file_data +i, 1,1,fp ); 
                    if(n != 1){ //n != size of elements means we read whole file
                      completed ='1';
                      //last_file_ack_number = sliding_window[j] + 1; //Set last file ack number here
                      break;
                    }
                  bytes_read++;         
                  } 

                  ACK = 0;
                  SYN = 0;
                  FIN = 0;
                  EncodeTCPHeader(buffer, file_data, completed,bytes_read, sequence_number, acknowledgement_number, ACK, SYN, FIN, window_size);
                  n = sendto(sockfd, buffer, sizeof(buffer), 0, (struct sockaddr *) &cli_addr, clilen);
                  fseek(fp, latest_sequence_number - dropped_seq + bytes_read, SEEK_CUR);


                } else {
                   // TODO: DELETE THIS LINE!!!!
                  //TODO. sliding window is not being initialized. hence old_elements is 0 and the if statement is not entered. check for max seq number
                  //find how many element to remove from sliding window
                  int i = 0;
                  for(; i < cwnd; i++){
                    printf("i: %d\n",i );
                    printf("latest_seq: %d  ack_num: %d   sliding_window[i]: %d\n", latest_sequence_number , acknowledgement_number , sliding_window[i]);
                    if(sliding_window[i] > acknowledgement_number && sliding_window[i] <=  latest_sequence_number){ // is sequence number >= ack number
                      break;
                    }
                  }
                 // printf("finished first loop\n");
                  int old_elements = i;

                  //shift received elements out of window
                  printf("removing elements\n");
                  for(; i > 0; i--){ 
                    printf("i: %d\n",i );
                    int j = 0;
                    for(; j < cwnd ; j++){
                      sliding_window[j] = sliding_window[j + 1];
                      printf("%d ", sliding_window[j]);
                    }
                    printf("\n");
                  }

                  //shift array of timers TODO: Check this
                  if((old_elements != 0) && (completed != '1')){
                    int m = 0;
                    for(; m + old_elements < cwnd; m++){
                       timer_window[m] = timer_window[m + old_elements];

                    }
                  }

 
                  if((old_elements != 0) && (completed != '1')){
                    printf("GOt here,  compl: %c\n" , completed);
                    int j = 0;
                    for(; j < cwnd; j++){
                      printf("before sliding_window[j]: %d  ", sliding_window[j]);
                      if(j >= cwnd - old_elements ){
                        if(j != 0){
                          sliding_window[j] = sliding_window[j-1] + max_packet_length;
                        }else {
                          sliding_window[j] = latest_sequence_number + max_packet_length;
                        }
                        printf("after sliding_window[j]: %d\n", sliding_window[j]);
                        //send corresponding packet
                        int bytes_read = 0;
                        i = 0;
                        for(;i<1000;i++){ //Change upper limit later
                            n = fread(file_data +i, 1,1,fp ); 
                            if(n != 1){ //n != size of elements means we read whole file
                              completed ='1';
                              last_file_ack_number = sliding_window[j] + 1; //Set last file ack number here
                              break;
                            }
                            bytes_read++;
                             
                        }     
                        
                        sequence_number = sliding_window[j]; 
                        latest_sequence_number = sequence_number;
                        ACK = 0;
                        SYN = 0;
                        FIN = 0;
                        
                        EncodeTCPHeader(buffer, file_data, completed,bytes_read, sequence_number, acknowledgement_number, ACK, SYN, FIN, window_size);
                        gettimeofday(timer_window +j, NULL);  //start corresponding timer
                        n = sendto(sockfd, buffer, sizeof(buffer), 0, (struct sockaddr *) &cli_addr, clilen);

                        if(completed == '1'){
                          break;
                        }


                      }
                    }
                  }

                }
              }

        }else if (FIN) { //CLOSING CONNECTION

                      FIN = 0;
                      ACK = 1;
                      SYN = 0;
                    EncodeTCPHeader(buffer, file_data,completed,0,sequence_number, acknowledgement_number, ACK, SYN, FIN, window_size);
                    n = sendto(sockfd, buffer, sizeof(buffer), 0, (const struct sockaddr* ) &cli_addr, clilen);
                    if (n < 0)  
                      error("ERROR in FIN init");

                    //TODO: - DO TIMED WAIT stuff
                  Timer.tv_sec = 0; //reset timer
                  Timer.tv_usec = 2 * rto_val; 

                  //after timed wait
                  fclose(fp);
                  close(sockfd);
                  return 0;

        } else { 

        }

      }
    }

  }


}

