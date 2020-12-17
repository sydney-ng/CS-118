#ifndef HEADER_FILE
#define HEADER_FILE

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/socket.h>
#include <netdb.h>
#include <poll.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <dirent.h>
#include <string.h>
#include <time.h>
#include <stdbool.h>
#include <sys/stat.h>
#include <fcntl.h>
#define BACKLOG 10
#define HEADER_LENGTH 12
#define MAX_SEQUENCE_NUMBER 25600
#define MAX_PAYLOAD_LENGTH 512
#define MIN_CWND_SIZE 512
#define MAX_CWND_SIZE 10240

struct UDP_packet {
	uint32_t sequence_number; 
    uint32_t ACK_number; 
    uint8_t ACK_flag; 
    uint8_t SYN_flag; 
    uint8_t FIN_flag;
    uint8_t connection_ID;

    char header[HEADER_LENGTH];
    char payload[MAX_PAYLOAD_LENGTH];
	
	unsigned char entire_packet[524]; 
}; 

int increment_seq_number(int sequence_num, int val){
    if (sequence_num + val <= MAX_SEQUENCE_NUMBER){
    	sequence_num+=val; 
    }
    else {
    	sequence_num = MAX_SEQUENCE_NUMBER - (sequence_num + val);
    }
    return sequence_num; 
}

void send_packet(int SYN_flag, int FIN_flag, int sequence_number, int ACK_number, int connection_ID, int ACK_flag, int sockfd, struct sockaddr_in myAddr) {
	struct UDP_packet pack; 
	/*create header */
	pack.sequence_number = sequence_number; 
    pack.ACK_number = ACK_number; 
    pack.connection_ID = connection_ID;
	pack.ACK_flag = ACK_flag; 
    pack.SYN_flag = SYN_flag; 
    pack.FIN_flag = FIN_flag;

    /* use this to format the header */
    unsigned char header_buffer [HEADER_LENGTH]; 
    
    *(unsigned *) header_buffer = pack.sequence_number; 
    *(unsigned *) (header_buffer + 4) = pack.ACK_number; 
    *(unsigned *) (header_buffer + 8) = pack.connection_ID; 
    *(unsigned *) (header_buffer + 9) = pack.ACK_flag; 
    *(unsigned *) (header_buffer + 10) = pack.SYN_flag; 
    *(unsigned *) (header_buffer + 11) = pack.FIN_flag;

    char payload_buf [MAX_PAYLOAD_LENGTH]; //Why was this declared 
    memset ((char *) payload_buf, 0, sizeof(payload_buf)); 

    memcpy(pack.entire_packet, header_buffer, 12);
    //we need to send out FIN if it has an empty payload
    //fprintf (stdout, "to send over is: %s\n", pack.entire_packet);
    int sendto_ret = sendto(sockfd, &pack.entire_packet, 524, 0, (struct sockaddr*) &myAddr,sizeof(struct sockaddr)); 
    //fprintf (stdout, "successfully sent syn\n");
}

void recv_info(int seqnum, int acknum, int ACK_flag, int SYN_flag, int FIN_flag,int cwnd, int ssthresh) {
                fprintf (stdout, "RECV %d %d %d %d", seqnum, acknum,cwnd,ssthresh); 
            if (ACK_flag)
                fprintf(stdout," ACK");
            if (SYN_flag)
                fprintf(stdout," SYN");
            if (FIN_flag)
                fprintf(stdout," FIN");
            fprintf(stdout,"\n");
}
void send_info(int seqnum, int acknum, int ACK_flag, int SYN_flag, int FIN_flag,int cwnd, int ssthresh) {
    fprintf (stdout, "SEND %d %d %d %d", seqnum, acknum,cwnd,ssthresh); 
    if (ACK_flag)
        fprintf(stdout," ACK");
    if (SYN_flag)
        fprintf(stdout," SYN");
    if (FIN_flag)
        fprintf(stdout," FIN");
    fprintf(stdout,"\n");
}

#endif
