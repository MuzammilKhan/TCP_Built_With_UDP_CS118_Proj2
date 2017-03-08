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
void DecodeTCPHeader(char* msgp, unsigned int* sequence_number, unsigned int* acknowledgement_number, unsigned int* ACK, unsigned int* SYN, unsigned int* FIN, unsigned short* window_size){
  
  memcpy(sequence_number, msgp, 4); 
  printf("cli_seq_rec: %d\n", *sequence_number ); //debugging
  memcpy(acknowledgement_number, msgp+32, 4);
  printf("cli_ack_rec: %d\n\n", *acknowledgement_number ); //debugging

  char tmp; 
  memcpy(&tmp, msgp+64, 1);
  *ACK = (tmp & 4) >> 2;
  *SYN = tmp & 2 >> 1;
  *FIN = tmp & 1;  
  memcpy(window_size, msgp+80, 2);  
  return;
}

/*Encode "TCP header" at msgp. Set checksum and other fields not explicitly set here to 0*/
void EncodeTCPHeader(char* msgp, unsigned int sequence_number, unsigned int acknowledgement_number, unsigned int ACK, unsigned int SYN, unsigned int FIN, unsigned short window_size){
  printf("cli_seq_sent: %u\n",sequence_number);
  printf("cli_ack_sent: %u\n\n",acknowledgement_number);
  memset(msgp, 0, 112); //set header to 0
  memcpy(msgp, &sequence_number, 4);
  memcpy(msgp+32,&acknowledgement_number, 4);
  char tmp;
  tmp = ((ACK << 2) + (SYN << 1) + FIN);
  memset(msgp+64, tmp, 1);// Assuming other flags to be 0 for our purposes
  memcpy(msgp+80, &window_size, 2);  

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
      unsigned int sequence_number = 1;
      unsigned int acknowledgement_number = 32;
      unsigned int ACK;
      unsigned int SYN;
      unsigned int FIN;
      unsigned short window_size = 10;

      int handshake = 0;
      int closing = 0;

      //we are using a hard coded port for our application so source = dest port
      source = portno;
      destination = source;
      int connected = 0;
      char temp_buf[512];

    while(1){
    /* get a message from the user */
        
      FD_ZERO(&active_fd_set);
      FD_SET(sockfd, &active_fd_set);
      Timer.tv_sec = 0;
      Timer.tv_usec = rto_val;


      bzero(buf, BUFSIZE);
      bzero(temp_buf , BUFSIZE);
      printf("Please enter msg: ");
      fgets(temp_buf, BUFSIZE, stdin);
        

      if(connected == 0){
          SYN = 1;
          ACK = 1;
          FIN = 1;
      }


        EncodeTCPHeader(buf, sequence_number, acknowledgement_number, ACK, SYN, FIN, window_size);
        
        /* send the message to the server */
        serverlen = sizeof(serveraddr);
        n = sendto(sockfd, buf, sizeof(buf), 0, (const struct sockaddr* ) &serveraddr, serverlen);
        if (n < 0) 
          error("ERROR in sendto");
        
        /* print the server's reply */
        bzero(buf, BUFSIZE);
        n = recvfrom(sockfd, buf, sizeof(buf), 0, ( struct sockaddr*  ) &serveraddr, &serverlen);
        DecodeTCPHeader(buf,  &sequence_number, &acknowledgement_number, &ACK, &SYN, &FIN,  &window_size);
          
        if (n < 0) 
          error("ERROR in recvfrom");
        
       
        
    }
    return 0;
}