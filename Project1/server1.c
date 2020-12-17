#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/socket.h>
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

void retrieve_HTTP_version(int starting_index);
int retrieve_file();
void retrieve_file_extension();
void respond_to_client();  
bool traverse_directory();
void open_and_check_file(); 
void get_time(); 
bool validate_GET_request();
void generate_404_message(); 
void PART_B_HANDLER (); 

int sockfd; //for socket connection
int newfd; //for new connection
char buf[1000];
char filename[100];
int file_FD; 
struct stat stat_file_FD;
char file_type[30];
char HTTP_version[10];  
char time_buf[100];
char file_content_buffer [5000000];
size_t newBuffSize = sizeof file_content_buffer;

// useful for program
char NULL_char = '\0'; 
char newline = '\n'; 
char blank = ' '; 

int main(int argc, char * argv[]) {
	
    int port;
    if (argc != 2){
        fprintf(stderr, "Not the correct number of arguments.\n");
        exit(1);
    }
    port = atoi(argv[1]);
    struct sockaddr_in myAddr;
    struct sockaddr_in connAddr; //connecting address
    if ((sockfd = socket(AF_INET,SOCK_STREAM,0)) < 0){
        fprintf(stderr,"Could not create socket.\n");
    }
    //so we can reuse address (avoid 'Address Already in Use')
    int a = 1;
    if(setsockopt(sockfd,SOL_SOCKET,SO_REUSEADDR,&a,sizeof(a))< 0){
        fprintf(stderr,"%s\n",strerror(errno));
        exit(1);
    }
    //setting stuff for my address
    myAddr.sin_family = AF_INET;
    myAddr.sin_port = htons(port);
    myAddr.sin_addr.s_addr = htonl(INADDR_ANY);
    memset(myAddr.sin_zero,0,sizeof(myAddr.sin_zero));

    //Bind the socket 
    if ((bind(sockfd,(struct sockaddr*) &myAddr,sizeof(struct sockaddr))) < 0) {
        fprintf(stderr,"%s\n",strerror(errno));
        //fprintf ("closing socket\n"); 
        close(sockfd);
    }

    if ((listen(sockfd,BACKLOG)) < 0){
        fprintf(stderr,"Could not listen.\n");
    }

    socklen_t sinSize; 

    for(;;){ 
        sinSize = sizeof(struct sockaddr_in);
        if ((newfd = accept(sockfd,(struct sockaddr*) &connAddr, &sinSize)) < 0){
            fprintf(stderr,"Could not accept new connection.\n");
        }
        else {
            //printf("Connection from: %s\n",inet_ntoa(connAddr.sin_addr));
            //get header information
            if (read(newfd,&buf,sizeof(buf))<0){
                fprintf(stderr,"Could not read.\n");
            }
            //else let's send it to Part B handler
            else {
            	//print header 
            	printf("%s\n",buf);
            	PART_B_HANDLER ();
            }
        }
    }
    if ((close(sockfd)) < 0){
        fprintf(stderr,"Could not close socket.\n");
    }
    shutdown (sockfd, 0); 
    shutdown (newfd, 0); 
}

/* Part B */ 

void PART_B_HANDLER () {
	/*extract time */ 
    bool valid_request = true;
    if (validate_GET_request() == false){
    	fprintf (stderr, "%s", "This is not a valid GET request\n"); 
    }
	get_time(); 
    int HTTP_version_starting_index = retrieve_file(); 
    //retrieve_HTTP_version(HTTP_version_starting_index);
    retrieve_file_extension();
	if (traverse_directory() == false) { 
    	//meaning page not found
    	generate_404_message(); 
    }
    else {
    	respond_to_client(); 
    }
	//fprintf (stdout, "Sent a Message to Client Successfully\n"); 
	//shutdown ()

}

void generate_404_message() {
	//fprintf (stdout, "in 404\n"); 
	//fprintf (stdout, "client FD in 404 is: %d\n", newfd); 
	char response_message[1000]; 
	char acc [1000]; 
	memset(response_message, 0, 1000);

	
	char error_message_404[25] = "HTTP/1.1 404 Not Found\r\n"; 
	char connection_404 [25]= "Connection: close\r\n\r\n";
	char length_404 [25] = "Content-Length: 42\r\n";
	char content_type_404 [25] = "Content-Type: text/html\r\n";
	char content_404 [100] = "<html><body> 404 Not Found </body></html>";

	strcat(acc, error_message_404);
	strcat(acc, time_buf);
	strcat(acc, length_404);
	strcat(acc, content_type_404);
	strcat(acc, connection_404);
	strcat(acc, content_404);

	sprintf(response_message, "%s\n", acc);
	//fprintf(stdout, "%s\n", response_message);
	int x; 
	x = write (newfd, response_message, 1000);
	//fprintf (stdout, "output of write is %d\n", x);
	//fprintf (stdout, "exiting 404\n"); 
} 

void retrieve_file_extension() {
	//fprintf (stdout, "in file extension\n"); 
	char type_html[10] = ".html"; 
	char type_htm[10] = ".htm"; 
	char type_txt[10] = ".txt"; 
	char type_jpg [10] = ".jpg"; 
	char type_jpeg [10] = ".jpeg"; 
	char type_png[10] = ".png"; 
	char type_gif[10] = ".gif";

	char html_types [30] = "Content-Type: text/html\r\n";
	char plain_text_file_type [28] = "Content-Type: text/plain\r\n"; 
	char static_image_types [28] =  "Content-Type: image/jpeg\r\n"; 
	char animated_image_type [27] = "Content-Type: image/gif\r\n"; 

	//fprintf (stdout, "okay i'm here now \n"); 	  
	if (strstr(filename, type_txt) != NULL) {
		//fprintf (stdout, "this is .txt\n");
		strcpy (file_type, plain_text_file_type); 	 
	}
	else if ((strstr(filename, type_html) != NULL) ||(strstr(filename, type_htm) != NULL)) {
		strcpy (file_type, html_types); 	 
	} 
	else if ((strstr(filename, type_jpeg) != NULL) || (strstr(filename, type_jpg) != NULL) || (strstr(filename, type_png) != NULL)) {
		strcpy (file_type, static_image_types); 	 
	}
	else if(strstr(filename, type_gif) != NULL) {
		strcpy (file_type, animated_image_type); 	 
	}
	//fprintf (stdout, "finished getting the file extension\n"); 
}

int retrieve_file() {
	int str_iterator = 5;
	
	int x = 0; 
	int filename_iterator = 0; 

	while (buf[str_iterator] != newline){
		if (buf[str_iterator] == blank){
			break; 
		}
		else {
			if (str_iterator == 5){
				filename[filename_iterator] = buf[str_iterator]; 
			}
			else {
				filename[filename_iterator] = buf[str_iterator]; 				
			}
			str_iterator++; 
			filename_iterator++;
		}
	}
	filename[filename_iterator] = '\0';

	char dest1[50]; 
	char dest2[50]; 
	char space [2] = " "; 
	char *substring_check = strstr(filename, "%20");
	if (substring_check != NULL){
		//fprintf (stdout,"detected space\n");
		int position = substring_check - filename;		
		//printf (stdout,"position is: %d\n", position);
		strncpy(dest1, filename, position);
		//fprintf (stdout,"after first copy, dest1 is: %s\n", dest1);
		strcat(dest1, space); 
		position = position + 3; 
		while (filename[position] != '\0') {
			strcat (dest1, &filename[position]);
			//fprintf (stdout,"adding to dest1: %c\n", filename[position]);
			position++; 

		}
		memset(filename, 0, 100);
		strcat(filename,dest1); 
		filename[filename_iterator-2] = '\0'; 

	}
	//fprintf (stdout, "filename is: %s", filename);
	return str_iterator; 
}

void retrieve_HTTP_version(int starting_index) {
	bool first = true; 
	char newline = '\n'; 
	int HTTP_version_iterator = 0; 
	memset(HTTP_version, 0, 10);

	while (buf[starting_index] != newline && buf[starting_index] != '\r'){
		//fprintf (stdout, "got in here\n"); 
		if (buf[starting_index] == ' '){
			starting_index++;
			continue;
		}
		else if (first == true){
			HTTP_version[HTTP_version_iterator] = buf[starting_index]; 
			first = false; 
 		}
 		else {
 			HTTP_version[HTTP_version_iterator] = buf[starting_index]; 				
 		}
 		HTTP_version_iterator++;
 		starting_index++; 
	}
	HTTP_version[starting_index] = '\0'; 
}


bool validate_GET_request() {
	if (buf[0] != 'G' && buf[0] != 'E' && buf[0] != 'T') {
		return false; 
	} 
	return true;
}

/* https://www.tutorialspoint.com/How-can-I-get-the-list-of-files-in-a-directory-using-C-Cplusplus */
bool traverse_directory() {
	//fprintf (stdout, "in traverse directory \n"); 
	int b_flag;
	struct dirent *entry;
	DIR *file_directory = opendir (".");

	if (file_directory == NULL) {
	  fprintf (stderr, "directory not found when traversing \n"); 
      false;
   }
   //fprintf (stdout, "we are looking for file %s\n", filename);
   //fprintf (stdout, "found directory while traversing\n");
   char *content = malloc(sizeof file_content_buffer);
   size_t realSize = 0;
   
	while ((entry = readdir(file_directory)) != NULL) {
	 	//check if you can find the file 
	 	int x = strcasecmp (filename, entry->d_name);
	 	//fprintf (stdout, "looking at %s\n", entry->d_name); 
	 	if (x == 0) {
	 		//fprintf (stdout, "found your file\n"); 
	 		file_FD = open(entry->d_name, O_RDONLY);
	 		//fprintf (stdout, "file descriptor is: %d\n", file_FD);
	 		
	 		if (file_FD < 0){
	 			fprintf (stderr, "Invalid File Descriptor for File\n");
	 			return false;  
	 		}
	 		else {
	 			//retrieve status 
	 			memset(file_content_buffer, 0, sizeof file_content_buffer);
	 			while ((b_flag = read (file_FD, file_content_buffer, sizeof(file_content_buffer))) != 0){
					 fprintf(stdout,"The bflag is %d",b_flag);
					memcpy(content+realSize,file_content_buffer,b_flag);
					realSize = realSize + b_flag;
					fprintf(stdout, "Real size is %zu", realSize);
					fprintf(stdout, "Buff size is %zu", newBuffSize);
					if (realSize == newBuffSize) {
						newBuffSize = newBuffSize + sizeof file_content_buffer;
						char *inc = realloc(content,newBuffSize);
						content = inc;
					}
						
					if (b_flag < 0){ 
						fprintf (stderr, "File Can't be Read\n"); 
						close (file_FD); 
						break; 
					}
					
				}
				fstat (file_FD, &stat_file_FD);
	 			//fprintf (stdout, "content size is %lld\n", stat_file_FD.st_size);
				free(content);
	 			return true; 
	 		}
	 		break; 
	 	}
	 }
	//closedir(file_directory); 
	return false; 
}

/*use CTIME Library and change Date*/

void get_time() {
	//fprintf (stdout, "in getting time \n"); 

    time_t rawtime;
    struct tm * time_struct;

    time_struct = localtime(&rawtime);

	memset(time_buf, 0, 100);

	int hr = time_struct->tm_hour;
    int min = time_struct->tm_min;
    int sec = time_struct->tm_sec; 
    int tm_mday = time_struct->tm_mday;       
    int tm_mon = time_struct->tm_mon;         /* month */
    int tm_year= time_struct->tm_year;        /* year */
    int tm_wday= time_struct->tm_wday;        /* day of the week */

    sprintf (time_buf, "Date: %d, %d %d %d %02d:%02d:%02d GMT\r\n", tm_wday, tm_wday, tm_mon, tm_year, hr, min, sec);
    //fprintf (stdout, "TIME IS: %s\n", time_buf); 
    return; 
}

void respond_to_client() {
	char response_message[10000]; 
	memset(response_message, 0, 10000);

	char file_size_number[1000000];
	sprintf(file_size_number, "%lld\r\n", stat_file_FD.st_size);

 	char message_200[18] = "HTTP/1.1 200 OK\r\n";

	char connection_close [20]= "Connection: close\r\n";

	char file_length_str [1000000] = "Content-Length: ";

	char server_type [32]= "Server: Apache/2.2.3 (CentOS)\r\n";

	char ending [3] = "\r\n";
	strcat (file_length_str, file_size_number);

	char *acc = (char*) malloc(sizeof(char) * (50000));

	//strcat(acc, http_v2);
	strcat(acc, message_200);
	strcat(acc, connection_close);
	strcat(acc, time_buf);
	strcat(acc, server_type);
	strcat(acc, file_length_str);
	strcat(acc, file_type);
	strcat(acc, ending);


	sprintf(response_message, "%s%s%s%s%s%s%s", message_200, connection_close, time_buf, server_type, file_length_str, file_type, ending);
	
	//sprintf (response_message, "%s", acc);
	fprintf(stdout, "%s\n", response_message); 

	write (newfd, response_message, strlen(response_message));

	//TODO: write to client (see 4847k), should replace with content
	if (write(newfd, file_content_buffer, newBuffSize) <0) {
		fprintf (stderr, "Can't write to buffer \n");
	}
	//memset(response_message, 0, 1000);
	//memset(file_content_buffer, 0, 5000000);
}