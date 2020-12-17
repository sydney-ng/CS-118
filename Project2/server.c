#include "header.h"
#include <sys/stat.h>

int sockfd; //for socket connection
char buffer [524];
int exit_flag = 0; 
int file_number = 1; //for right now, it will be one TODO: implement for each sequential session
FILE* writefile;  
bool startedFile = false;
int assigned_connection_ID = -1; 
char save_filename[100]; 
struct UDP_packet curr_UDP_packet; 
time_t last_packet_received_time;
int server_sequence_num; 
int server_ACK_number;
int num_bytes_read = 0;
struct sockaddr_in myAddr;
bool post_connection (); 
bool save_file();
int expected_seq_num;
 

bool examine_packet(); 
bool while_not_in_timeout_actions (); 
bool server_create_packet(); 
void send_SYN_to_client();
void server_generate_starting_seq_num();
void debug_packet(); 


/* based on: https://www.tutorialspoint.com/c_standard_library/c_function_difftime.htm */ 
bool check_time() {
    time_t curr_time; 
    time(&curr_time); 
    double diff_t = difftime(last_packet_received_time, curr_time);
    if (diff_t > 10.0){
        save_file();
        exit_flag = 1; 
        close (sockfd); 
        return false; 
    }
    else {
       last_packet_received_time = curr_time;  
    }
    return true; 
}

void handle_SIGQUIT(int sig) 
{ 
    printf("Caught signal %d\n", sig); 
    exit_flag = 0;

    char interrupt_string [10] = "INTERRUPT";
    interrupt_string[9] = '\0';  

    printf("Caught signal %d\n", sig);
    if (writefile != NULL){
        freopen (save_filename, "w", writefile);
        fputs(interrupt_string, writefile);
    }

    //fclose (readfile);
    fclose (writefile); 
} 

void handle_SIGTERM(int sig) 
{ 
    char interrupt_string [10] = "INTERRUPT";
    interrupt_string[9] = '\0';  

    printf("Caught signal %d\n", sig);
    if (writefile != NULL){
        freopen (save_filename, "w", writefile);
        fputs(interrupt_string, writefile);
    }

    exit_flag = 0; 
    //fclose (readfile);
    fclose (writefile);
} 

int main(int argc, char * argv[]) {
    /* part 1: setup */ 
    srand(time(0));
    memset(curr_UDP_packet.header,0, HEADER_LENGTH);
    memset(curr_UDP_packet.payload,0, MAX_PAYLOAD_LENGTH);
    int port;
    memset(buffer,0,sizeof(buffer));

    if (argc != 2){
        fprintf(stderr, "Not the correct number of arguments.\n");
        exit(1);
    }

    port = atoi(argv[1]);
    struct sockaddr_in myAddr;

    struct addrinfo hints, *servinfo, *p;
    int getaddrinfo_ret_val;

    memset(&hints, 0, sizeof hints); 
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_flags = AI_PASSIVE; 


    if ((getaddrinfo_ret_val = getaddrinfo(NULL, argv[1], &hints, &servinfo)) != 0) {
        fprintf(stderr, "Error: Couldn't set up Connection \n");
        exit_flag = 3; 
    }

   for(p = servinfo; p != NULL; p = p->ai_next) {
        int reuse_addr = 1; 
        fcntl(sockfd, F_SETFL, O_NONBLOCK);
        if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
            fprintf (stdout, "Error: Couldn't set up the Connection\n") ;
            continue;
        }
        if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &reuse_addr, sizeof(int)) == -1) {
            fprintf (stdout, "Error: Socket Connection \n") ;
        }
        if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
            fprintf (stdout, "Error: Binding Error \n");
            close(sockfd);
            continue;
        }
        // passed all checks, can finish connection set up
        while (1) {
        if (post_connection() == false) { 
            close(sockfd); 
            return exit_flag; 
            }
        }
    }
    return exit_flag;
} 


bool post_connection (){

    struct pollfd fds_packet[1];
    fds_packet[0].fd = sockfd;
    fds_packet[0].events = POLLIN;

    socklen_t sinSize; 
    signal(SIGQUIT, handle_SIGQUIT); 
    signal(SIGTERM, handle_SIGTERM); 

    for (;;){
        int poll_result = poll(fds_packet, 1, 10000);
        if (poll_result < 0) {
            fprintf (stderr, "ERROR: Polling failed\n");
            exit_flag = 1;
            return false; 
        }
        else if (poll_result > 0){
            if (while_not_in_timeout_actions() == false){
                return false;
            } 
            return true;
        }
        else {
            fprintf (stdout, "Server established a connection but has received no data for 10 seconds since connection establishmen, TIMEOUT for connection %d \n", file_number);  
            close (sockfd);
            exit_flag = false; 
        }
    }
    return false;
}

bool while_not_in_timeout_actions () {

    socklen_t sinSize = sizeof(struct sockaddr_in);
    if (recvfrom(sockfd, buffer, sizeof(buffer), 0, (struct sockaddr*) & myAddr, &sinSize) < 0){
            fprintf (stderr, "%s\n", "ERROR: cannot receive UDP connections\n"); 
    }
    else {
        bool x = server_create_packet();
        //Do a check of the sequence number, 
        

        //COMMENT THIS IF STATEMENT OUT FOR SUCCESS ------
        if (curr_UDP_packet.sequence_number != expected_seq_num) {
            if (startedFile) {
            fprintf(stdout,"WE DROPPED PACKET %d. Retransmitting packet %d.\n",curr_UDP_packet.sequence_number,expected_seq_num);
            send_packet(0, 0, server_sequence_num, expected_seq_num, curr_UDP_packet.connection_ID, 1, sockfd, myAddr); // 1 for setting ACK flag
            send_info(server_sequence_num,expected_seq_num,1,0,0,0,0);
            return true;
            }
        }
        
            //if right, go ahead, if not, drop and send some acknowledgement   
        return examine_packet();
    }
    return false;
}

                
     bool examine_packet() {           
                recv_info(curr_UDP_packet.sequence_number,curr_UDP_packet.ACK_number,curr_UDP_packet.ACK_flag,curr_UDP_packet.SYN_flag,curr_UDP_packet.FIN_flag,0,0);
                if (curr_UDP_packet.SYN_flag == 1){
                    server_generate_starting_seq_num();
                    send_packet(1, 0, server_sequence_num, (curr_UDP_packet.sequence_number + 1), curr_UDP_packet.connection_ID, 1, sockfd, myAddr); // 1 for setting ACK flag
                    send_info(server_sequence_num,(curr_UDP_packet.sequence_number + 1),0,1,0,0,0); 
                    return true;
                }
                else if (curr_UDP_packet.FIN_flag == 1){
                    startedFile = 0;
                    send_packet(0, 1, server_sequence_num, (rand() % 25601) , curr_UDP_packet.connection_ID, 1, sockfd, myAddr); // 1 for setting ACK flag
                    send_info(server_sequence_num,(rand() % 25601),1,0,1,0,0); 
                    file_number++;
                    close(sockfd);
                    return true;
                }
                else if (curr_UDP_packet.SYN_flag == 0 && curr_UDP_packet.FIN_flag == 0 && curr_UDP_packet.ACK_flag == 1){
                    /* get file name */ 
                    startedFile = true;
                    if (save_file() == false) {
                        fprintf (stderr, "%s\n", "ERROR: could not open and save file\n"); 
                        exit_flag = 1; 
                    }
                    server_ACK_number = server_sequence_num + num_bytes_read;
                    if (curr_UDP_packet.payload[0] == '\0'){
                        //TODO
                        //wait 10 seconds 
                        //if not put into file, close connection (send fin)
                        send_packet(0, 1, server_sequence_num, server_ACK_number, curr_UDP_packet.connection_ID, 1, sockfd, myAddr); // 1 for setting ACK flag    
                        send_info(server_sequence_num,server_ACK_number,1,0,1,0,0); 
                    }
                    else {
                        send_packet(0, 0, server_sequence_num, server_ACK_number, curr_UDP_packet.connection_ID, 1, sockfd, myAddr); // 1 for setting ACK flag
                        send_info(server_sequence_num,server_ACK_number,1,0,0,0,0); 
                    }
                    time(&last_packet_received_time); // get the time 
                    return true;
                }
                else {
                    fprintf (stdout, "%s\n", "Error.\n"); 
                    debug_packet();
                    return false;        
                }
                expected_seq_num = server_ACK_number+num_bytes_read;
            return false;
    } 
 
    


void debug_packet(){
    fprintf(stderr,"DEBUGGING ------------\n");
    //memcpy(&curr_UDP_packet.ACK_number , (uint32_t*) (buffer + 4), sizeof(uint32_t*));
    fprintf (stderr, "packet received has: \nACK_number: %d\n", curr_UDP_packet.ACK_number);
    
   // memcpy(&curr_UDP_packet.connection_ID , (uint8_t*) (buffer + 8), sizeof(uint8_t*));
    fprintf (stderr, "connection_ID: %d\n", curr_UDP_packet.connection_ID);
    
    //memcpy(&curr_UDP_packet.ACK_flag , (uint8_t*) (buffer + 9), sizeof(uint8_t*));
    fprintf (stderr, "ACK_flag: %d\n", curr_UDP_packet.ACK_flag);

  //  memcpy(&curr_UDP_packet.SYN_flag , (uint8_t*) (buffer + 10), sizeof(uint8_t*));
    fprintf (stderr, "SYN_flag: %d\n", curr_UDP_packet.SYN_flag);

//    memcpy(&curr_UDP_packet.FIN_flag , (uint8_t*) (buffer + 11), sizeof(uint8_t*));
    fprintf (stderr, "FIN_flag: %d\n", curr_UDP_packet.FIN_flag);

    //memcpy(&curr_UDP_packet.payload , (uint8_t*) (buffer + 12), sizeof(uint8_t*));
    fprintf (stderr, "payload: %s\n", curr_UDP_packet.payload);

    fprintf(stderr,"END OF DEBUGGING ---------------\n");
}

void reset_packet() {
    curr_UDP_packet.sequence_number = 0; 
    curr_UDP_packet.ACK_number = 0; 
    curr_UDP_packet.ACK_flag = 0; 
    curr_UDP_packet.SYN_flag = 0;
    curr_UDP_packet.FIN_flag = 0;
    curr_UDP_packet.connection_ID = 0;

    num_bytes_read = 0; 

    memset(curr_UDP_packet.header,0, HEADER_LENGTH);
    memset(curr_UDP_packet.payload,0, MAX_PAYLOAD_LENGTH);

}
bool server_create_packet(){

    reset_packet();

    //fprintf (stdout, ": %x\n", curr_UDP_packet.sequence_number);
    memcpy(&curr_UDP_packet.sequence_number , &buffer[0], sizeof(uint32_t));
    //fprintf (stdout, "sequence number: %d\n", curr_UDP_packet.sequence_number);
    
    memcpy(&curr_UDP_packet.ACK_number , &buffer[4], sizeof(uint32_t));
    //fprintf (stdout, "ACK_number: %d\n", curr_UDP_packet.ACK_number);
    
    memcpy(&curr_UDP_packet.connection_ID , &buffer[8], sizeof(uint8_t));
    //fprintf (stdout, "connection_ID: %d\n", curr_UDP_packet.connection_ID);
    
    memcpy(&curr_UDP_packet.ACK_flag , &buffer[9], sizeof(uint8_t));
    //fprintf (stdout, "ACK_flag: %d\n", curr_UDP_packet.ACK_flag);

    memcpy(&curr_UDP_packet.SYN_flag , &buffer[10], sizeof(uint8_t));
    //fprintf (stdout, "SYN_flag: %d\n", curr_UDP_packet.SYN_flag);

    memcpy(&curr_UDP_packet.FIN_flag , &buffer[11], sizeof(uint8_t));
    //fprintf (stdout, "FIN_flag: %d\n", curr_UDP_packet.FIN_flag);

    memcpy(&curr_UDP_packet.payload , &buffer[12], sizeof(char)*512);
    //fprintf (stdout, "FIN_flag: %s\n", curr_UDP_packet.payload);

    //fprintf (stdout, "server side: received packet with SN %d, ACK_number %d \n", curr_UDP_packet.sequence_number, curr_UDP_packet.ACK_number); 
    return true;
}

void server_generate_starting_seq_num () {
    server_sequence_num = rand() % 25601;
    server_ACK_number = curr_UDP_packet.sequence_number + 1;
    assigned_connection_ID++;
} 

bool save_file(){
    char c;  
    char actual_number[3]; 
    char extension [8] = ".file\0";
    sprintf(actual_number, "%d", file_number);
    memset(save_filename,0,sizeof(save_filename));
    strcat (save_filename, actual_number);
    strcat(save_filename, extension); 

     
    writefile = fopen (save_filename, "a+"); 
    if (writefile){
        char c; 
        c = curr_UDP_packet.payload[0];
        //fprintf(stdout,"The first character of this payload is %c\n",c);
        int i = 0;
        while (c) {
            fputc(c,writefile);
            if (i+1 == 512)
                break;
            i++;
            c = curr_UDP_packet.payload[i];
        }
        //fprintf(stdout, "%d bytes read\n",i);
        fclose (writefile);
        return true;
    }
    else {
        fprintf(stdout,"Could not make/open a file.\n");
        return false; 
    }




}


