#include "header.h"
#include <math.h>
int sockfd; //for socket connection
int cwnd = MIN_CWND_SIZE;
int ssthresh = 5120;
int exit_flag; 
int noMoreFile = 0;
struct hostent *serv; 
bool dataAck = 0;
time_t last_packet_received_time;
char file[512];
char file_name[512];
int client_sequence_num; 
int client_ACK_number; 
int global_connection_ID; 
char buffer [524];
socklen_t sinSize; 
int num_bytes_read = 0;
struct sockaddr_in myAddr;
struct UDP_packet client_curr_UDP_packet;
int last_seen_ack = -1;  
int itsFINTIME=0;
FILE* fileToSend;
int increment_seq_number(int sequence_number, int val); 
void generate_starting_seq_num (); 
void send_packet(int SYN_flag, int FIN_flag, int sequence_number, int ACK_number, int connection_ID, int ACK_flag, int sock_fd, struct sockaddr_in myAddr); 
bool client_create_packet(); 
void send_file(int SYN_flag, int FIN_flag, int sequence_number, int ACK_number, int connection_ID, int ACK_flag, int sockfd, struct sockaddr_in myAddr);
void slow_start();
void cong_avoid();
void initiate_end();
bool check_for_timeout();
bool while_not_in_timeout_actions(); 


int main(int argc, char * argv[]) {
	srand (time(NULL)); 
	int port; 
	/* preprocessing, grab data for variables */ 
	if (argc != 4){
        fprintf(stderr, "ERROR: Not the correct number of arguments.\n");
        exit(1);
    }

    port = atoi(argv[2]);
    memset ((char *) file, 0, sizeof(file));
    strcpy (file_name, argv[3]);
    fileToSend = fopen(file_name,"r");
    if (!fileToSend){
        fprintf(stdout, "Could not open the file.\n");
    }
    /* set up connection */ 
    if ((sockfd = socket(AF_INET,SOCK_DGRAM,0)) < 0){
        fprintf(stderr,"ERROR: Could not create UDP socket.\n");
    }

    //fprintf (stdout, "vars are %s %s %s %s\n", argv[0], argv[1],argv[2],argv[3]); 
    myAddr.sin_family = AF_INET;
    myAddr.sin_port = htons(port);
    myAddr.sin_addr.s_addr = htonl(INADDR_ANY);
    memset(myAddr.sin_zero,0,sizeof(myAddr.sin_zero));
   
    int addr_info_ret; 
    struct addrinfo hints, *servinfo, *p;

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET; 
    hints.ai_socktype = SOCK_DGRAM;

    // note argv[1] = hostname 
    if ((addr_info_ret = getaddrinfo(argv[1], argv[2], &hints, &servinfo)) != 0) {
        fprintf (stdout, "Error: Couldn't set up the Connection\n") ;
        fclose(fileToSend);
        exit_flag = 3;
    }
    
    for(p = servinfo; p != NULL; p = p->ai_next) {
    	if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
            fprintf (stderr, "Error: socket failure \n");
            continue;
        }
        break; 
    }

    if (!p){
        // looped off the end of the list with no connection
        fprintf (stdout, "Error: Connection failure \n"); 
        fclose(fileToSend); 
        exit_flag = 3; 
    }

    freeaddrinfo(servinfo); 

    struct pollfd fds[1];
    fds[0].fd = sockfd;
    fds[0].events = POLLIN;

    cwnd = 512;
    /* three way handshake */ 
    //fprintf (stdout, "file name is : %s\n", file); 
    generate_starting_seq_num();
    // param 1 => set SYN flag to 1, param 2 => set FIN flag to 0, param 6 => ACK flag
	//fprintf (stdout, "client side: sending over original SYN PACKET with SN %d, ACK_number %d \n", client_sequence_num, client_ACK_number); 
	send_packet(1, 0, client_sequence_num, client_ACK_number, global_connection_ID, 0, sockfd, myAddr); 
    //send_info(client_sequence_num,client_ACK_number,0,1,0,cwnd,ssthresh);

    increment_seq_number(client_sequence_num, sizeof(file)); 

    memset(buffer,0,sizeof(buffer));
    //checkforTImeout
    for(;;) {
        if (noMoreFile)
            return 0;
        int poll_result = poll(fds, 1, 10000);
        if (poll_result < 0) {
            fprintf (stderr, "ERROR: Polling failed\n");
            exit_flag = 1;
            break; 
        }
        else if (poll_result > 0){
            sinSize = sizeof(struct sockaddr_in);
            if (recvfrom(sockfd, buffer, sizeof(buffer), 0, (struct sockaddr*) & myAddr, &sinSize) < 0){
                fprintf (stderr, "%s\n", "ERROR: cannot receive UDP connections\n"); 
            }
            dataAck = 1;
            while_not_in_timeout_actions(); 
        }
        else {
            fprintf(stderr,"Nothing heard from the server. Exiting.\n");
            close(sockfd);
            return 3;
        }

        if (!check_for_timeout()){
            send_file(client_curr_UDP_packet.SYN_flag, client_curr_UDP_packet.FIN_flag, client_sequence_num, client_ACK_number, client_curr_UDP_packet.connection_ID, client_curr_UDP_packet.ACK_flag, sockfd, myAddr); // 1 for setting ACK flag 
            send_info(client_sequence_num,client_ACK_number,client_curr_UDP_packet.ACK_flag,client_curr_UDP_packet.SYN_flag,client_curr_UDP_packet.FIN_flag,cwnd,ssthresh); 
            time(&last_packet_received_time); 
        }

    }

        
        

        
	 
    close(sockfd);
    fclose(fileToSend);
    return exit_flag;
} 

bool client_create_packet(){
    memcpy(&client_curr_UDP_packet.sequence_number , &buffer[0], sizeof(uint32_t));
    //fprintf (stdout, "sequence number: %d\n", client_curr_UDP_packet.sequence_number);
    
    memcpy(&client_curr_UDP_packet.ACK_number , &buffer[4], sizeof(uint32_t));
    //fprintf (stdout, "ACK_number: %d\n", client_curr_UDP_packet.ACK_number);
    last_seen_ack = client_curr_UDP_packet.ACK_number; 
    
    memcpy(&client_curr_UDP_packet.connection_ID , &buffer[8], sizeof(uint8_t));
    //fprintf (stdout, "connection_ID: %d\n", client_curr_UDP_packet.connection_ID);
    
    memcpy(&client_curr_UDP_packet.ACK_flag , &buffer[9], sizeof(uint8_t));
    //fprintf (stdout, "ACK_flag: %d\n", client_curr_UDP_packet.ACK_flag);

    memcpy(&client_curr_UDP_packet.SYN_flag , &buffer[10], sizeof(uint8_t));
    //fprintf (stdout, "SYN_flag: %d\n", client_curr_UDP_packet.SYN_flag);

    memcpy(&client_curr_UDP_packet.FIN_flag , &buffer[11], sizeof(uint8_t));
    //fprintf (stdout, "FIN_flag: %d\n", client_curr_UDP_packet.FIN_flag);

    memcpy(&client_curr_UDP_packet.payload , &buffer[12], sizeof(char)*512);
    //fprintf (stdout, "FIN_flag: %s\n", client_curr_UDP_packet.payload);
    //fprintf (stdout, "buffer is: %s\n", buffer); 
    return true;
}

void send_file(int SYN_flag, int FIN_flag, int sequence_number, int ACK_number, int connection_ID, int ACK_flag, int sockfd, struct sockaddr_in myAddr) {
	struct UDP_packet pack; 
	/*create header */

	pack.sequence_number = sequence_number; 
    pack.ACK_number = ACK_number;
    pack.connection_ID = connection_ID;
	pack.ACK_flag = 1; 
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
    char payload_buf [MAX_PAYLOAD_LENGTH];
    
    int currWindow = 0;

    
        //we need some kind of loop for congestion control 
        //need to fill the current window first before we increment it 
        //send out the packets one at a time
        while (currWindow + sizeof(payload_buf) <= cwnd) {
            //fprintf (stdout, "values in sendfile are: SN %d, AN%d, AF%d, SF%d, FF%d\n", pack.sequence_number, pack.ACK_number, pack.ACK_flag, pack.SYN_flag, pack.FIN_flag); 
            memset ((char *) payload_buf, 0, MAX_PAYLOAD_LENGTH); 
            num_bytes_read=fread(payload_buf,1,sizeof (payload_buf)-1,fileToSend);
            //fprintf(stdout,"Number bytes read, %d\n",num_bytes_read);
            if (!num_bytes_read){
                noMoreFile = 1;
                break;
            }
            memcpy(pack.entire_packet, header_buffer, 12);
            memcpy(&pack.entire_packet[12], payload_buf, 512);            //fprintf (stdout, "to send over is: %s\n", pack.entire_packet);
            int sendto_ret = sendto(sockfd, &pack.entire_packet, 524, 0, (struct sockaddr*) &myAddr,sizeof(struct sockaddr)); 
            usleep(2000);
            //time(&last_packet_received_time); 
            //send_info(pack.sequence_number,pack.ACK_number,pack.ACK_flag,pack.SYN_flag,pack.FIN_flag,cwnd,ssthresh);
            //(1);
            //fprintf (stdout, "successfully sent syn\n");
            //managing windows
            pack.sequence_number += num_bytes_read;
            //if the buffer is partially full, we've done it wrong or that's the end.
            //if (num_bytes_read < sizeof (payload_buf)-1){    
                //noMoreFile = 1;
               // break;
            //}
            currWindow += 512;
        }
        if (noMoreFile) {
            return;
            //initiate_end();
        }
        /* --- CONGSTION CONTROL --- */
        slow_start(); //includes congestion avoidance call if needed

}

/* https://stackoverflow.com/questions/14403951/generate-a-random-number-between-1-and-n-1 */
void generate_starting_seq_num () {
	client_sequence_num = rand() % 25601;
	client_ACK_number = 0;
	global_connection_ID = 1;
} 

void cong_avoid(){
    cwnd += ((512*512)/cwnd);
    if (cwnd > MAX_CWND_SIZE)
        cwnd = MAX_CWND_SIZE;
}

void slow_start() {
    if (cwnd < ssthresh)
        cwnd += 512;
    else 
        cong_avoid();
    if (cwnd > MAX_CWND_SIZE)
        cwnd = MAX_CWND_SIZE;       
}

bool check_for_timeout() {
    time_t curr_time; 
    time(&curr_time); 
    if (!dataAck)
        return true;
    if (noMoreFile)
        return true;
    double diff_t = difftime(last_packet_received_time, curr_time);
    if (diff_t > 0.5){
        if (cwnd/2 > 1024)
            ssthresh = cwnd/2;
        else 
            ssthresh = 1024;
        cwnd = 512;
        return false; 
    }
    time(&last_packet_received_time); 
    return true;
}

void initiate_end() {
    //SEND FIN PACKET
    itsFINTIME = 1;
    send_packet(0,1,client_sequence_num,client_ACK_number,client_curr_UDP_packet.connection_ID,1,sockfd,myAddr);
}

bool while_not_in_timeout_actions() {
    if (noMoreFile)
        return true;
        	//fprintf (stdout, "got a packet from server\n"); 
    if (client_create_packet() == false){
        exit_flag = 1;
        fprintf (stderr, "%s\n", "ERROR: cannot extract information from packet received\n"); 
    } 
    // if receiving part 2 of 3 way handshake from server
    //getting a FIN from server
    else if (client_curr_UDP_packet.FIN_flag == 1) {
       	//fprintf (stdout, "received fin from server, file transaction complete \n"); 
       	client_sequence_num = client_curr_UDP_packet.ACK_number; 
        recv_info(client_curr_UDP_packet.sequence_number,client_curr_UDP_packet.ACK_number,client_curr_UDP_packet.ACK_flag,client_curr_UDP_packet.SYN_flag,client_curr_UDP_packet.FIN_flag,cwnd,ssthresh);
        client_ACK_number = 0;
        send_packet(0, 1, client_sequence_num, 0, client_curr_UDP_packet.connection_ID, 0, sockfd, myAddr); // 1 for setting ACK flag 
        send_info(client_sequence_num,0,0,0,1,cwnd,ssthresh); 
    }
    //if receiving an SYN and ACK from server
    else if (client_curr_UDP_packet.SYN_flag == 1 && client_curr_UDP_packet.ACK_flag == 1) {
       	client_sequence_num = client_curr_UDP_packet.ACK_number; 
        recv_info(client_curr_UDP_packet.sequence_number,client_curr_UDP_packet.ACK_number,client_curr_UDP_packet.ACK_flag,client_curr_UDP_packet.SYN_flag,client_curr_UDP_packet.FIN_flag,cwnd,ssthresh);
        client_ACK_number = (client_curr_UDP_packet.sequence_number +1);
        send_file(0, 0, client_sequence_num, (client_curr_UDP_packet.sequence_number +1), client_curr_UDP_packet.connection_ID, 1, sockfd, myAddr); // 1 for setting ACK flag 
        time(&last_packet_received_time);
    }
    //uploaded to file successfully, receiving a FIN
    else if (client_curr_UDP_packet.SYN_flag == 0 && client_curr_UDP_packet.ACK_flag == 0 && client_curr_UDP_packet.FIN_flag == 1) {
       	client_sequence_num= client_curr_UDP_packet.ACK_number; 
        recv_info(client_curr_UDP_packet.sequence_number,client_curr_UDP_packet.ACK_number,client_curr_UDP_packet.ACK_flag,client_curr_UDP_packet.SYN_flag,client_curr_UDP_packet.FIN_flag,cwnd,ssthresh);
       	client_ACK_number = 0;
        time(&last_packet_received_time);
        send_packet(0, 1, client_sequence_num, 0, client_curr_UDP_packet.connection_ID, 0, sockfd, myAddr); // 1 for setting ACK flag 
       	send_info(client_sequence_num,0,0,0,1,cwnd,ssthresh);
    }
    //if it's a normal packet!
    else if (client_curr_UDP_packet.SYN_flag == 0 && client_curr_UDP_packet.ACK_flag == 1 && client_curr_UDP_packet.FIN_flag == 0){
        recv_info(client_curr_UDP_packet.sequence_number,client_curr_UDP_packet.ACK_number,client_curr_UDP_packet.ACK_flag,client_curr_UDP_packet.SYN_flag,client_curr_UDP_packet.FIN_flag,cwnd,ssthresh);
        client_ACK_number = (client_curr_UDP_packet.sequence_number +1);
        time(&last_packet_received_time);
        //check for if this is for ending a connection
        
        if (itsFINTIME){
            //check for ACKs for two secondes
            time_t begin;
            time_t end;
            time(&begin); //take time once you get ack
            while(1){
                time(&end);
                double diff_t = difftime(begin, end);
                if (fabs(diff_t) > 2.0)
                    return false;
            }
            initiate_end();
        }
                
        //we need to check if SYN, FIN flag are toggled and if payload is zero
        client_sequence_num = client_curr_UDP_packet.ACK_number; 
        send_file(0, 0, client_sequence_num, (client_curr_UDP_packet.sequence_number +1), client_curr_UDP_packet.connection_ID, 1, sockfd, myAddr);
        send_info(client_sequence_num,(client_curr_UDP_packet.sequence_number +1),1,0,0,cwnd,ssthresh); 
    }
    else {
        fprintf (stderr, "ERROR: cannot parse response from server\n"); 
        return false; 
    }
    return true;
}




