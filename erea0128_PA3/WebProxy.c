#define __USE_XOPEN
#define _GNU_SOURCE
#include <errno.h>
#include <arpa/inet.h>
#include <signal.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/wait.h>
#include <sys/errno.h>
#include <sys/stat.h>
#include <sys/poll.h>
#include <sys/mman.h>
#include <sys/shm.h>
#include <dirent.h>
#include <netinet/in.h>
#include <netdb.h>

#include <stdarg.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include <time.h>
#include <openssl/md5.h>
#include <fcntl.h>
#include <pthread.h>

#define QLEN    32 
#define BUFSIZE	4096

extern int  errno;
void sigchld_handler(int s);
void *get_in_addr(struct sockaddr *sa);
int connectsock(const char *URI);
int ErrorHandle(int d, int code, int fd, char* Method, char* URI, char* Version);
unsigned char *HashIt(const char Request[BUFSIZE], int bytes);
//struct Cache *CacheMoney;

int main(int argc, char* argv[])
{
	int client, new_fd, rv, bytes, length;
	float timeout;  
    struct addrinfo hints, *servinfo, *p;
    struct sockaddr_storage their_addr;
    socklen_t sin_size;
    struct sigaction sa;
    int yes = 1;
    char *Method = NULL, *URI = NULL, *Version = NULL, *portnum = NULL;
    char token1[300], token2[300], token3[300];
    char s[INET6_ADDRSTRLEN];
    char Request[BUFSIZE];
    /*pthread_mutex_t* threadLock;
    int des_mutex;

    des_mutex = shm_open("/mutex_lock", O_CREAT | O_RDWR | O_TRUNC, S_IRWXU | S_IRWXG);

    if (des_mutex < 0) 
    	perror("failure on shm_open on des_mutex");
    
    if(ftruncate(des_mutex, sizeof(pthread_mutex_t)) == -1)
    	fprintf(stderr, "ftruncate: %s\n", strerror(errno));

    threadLock = (pthread_mutex_t*) mmap(NULL, sizeof(pthread_mutex_t), PROT_READ | PROT_WRITE, MAP_SHARED, des_mutex, 0);

    if (threadLock == MAP_FAILED ) 
    	perror("Error on mmap on mutex\n");

    pthread_mutexattr_t mutexAttr;
	pthread_mutexattr_setpshared(&mutexAttr, PTHREAD_PROCESS_SHARED);
	pthread_mutex_init(threadLock, &mutexAttr);

    CacheMoney = calloc(50, sizeof(CacheMoney));
    int check = 0;
    while (check < 50)
    {
    	CacheMoney[check].path = calloc(BUFSIZE, 1);
    	check++;
    }

    fd = shm_open("/CacheMoney", O_CREAT | O_RDWR, S_IRUSR | S_IWUSR);

    if (fd < 0) 
    	perror("failure on shm_open on des_mutex");

    if(ftruncate(fd, sizeof *CacheMoney) == -1)
        fprintf(stderr, "ftruncate: %s\n", strerror(errno));

    CacheMoney = mmap(NULL, sizeof *CacheMoney, PROT_READ | PROT_WRITE, 
                    MAP_SHARED | MAP_ANONYMOUS, fd, 0);

    if (CacheMoney == MAP_FAILED ) 
    	perror("Error on mmap on mutex\n");*/

    if (argc < 2)
    	portnum = "8080";
    else
    	portnum = argv[1];
    if (argc == 3)
    	timeout = atof(argv[2]);
    else
    	timeout = 5.0;

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE; // use my IP

	//DFS binding to socket
    if ((rv = getaddrinfo(NULL, portnum, &hints, &servinfo)) != 0) //Attempting to bind to portnum
    {
        fprintf(stderr, "\ngetaddrinfo: %s\n", gai_strerror(rv));
        return 1;
    }
	// loop through all the results and bind to the first we can
    for(p = servinfo; p != NULL; p = p->ai_next) 
    {
        if ((client = socket(p->ai_family, p->ai_socktype,
                p->ai_protocol)) == -1) {
            perror("\nserver: socket");
            continue;
        }

        if (setsockopt(client, SOL_SOCKET, SO_REUSEADDR, &yes,
                sizeof(int)) == -1) {
            perror("\nsetsockopt");
            exit(1);
        }

        if (bind(client, p->ai_addr, p->ai_addrlen) == -1) {
            close(client);
            perror("\nserver: bind");
            continue;
        }

        break;
    }

    freeaddrinfo(servinfo); // all done with this structure
    if (p == NULL)  
    {
        fprintf(stderr, "\nserver: failed to bind\n");
        exit(1);
    }

    if (listen(client, QLEN) == -1) 
    {
        perror("\nlisten");
        exit(1);
    }

	//Handling all the child processes
    sa.sa_handler = sigchld_handler; // reap all dead processes
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    if (sigaction(SIGCHLD, &sa, NULL) == -1) 
    {
        perror("\nsigaction");
        exit(1);
    }
    printf("\nserver port %s: waiting for connections...\n", portnum);

	//Main accept() loop for webproxy
    while(1) 
    {  	
        sin_size = sizeof their_addr;
        new_fd = accept(client, (struct sockaddr *)&their_addr, &sin_size); //listening for a client
        if (new_fd == -1) 
        {
            perror("accept");
            continue;
        }

        inet_ntop(their_addr.ss_family, //tells us if we got a connection
            get_in_addr((struct sockaddr *)&their_addr),
            s, sizeof s);
        printf("server port %s: got connection from %s\n", portnum, s);

		//Start listening for content

		if (!fork()) // We fork to allow the child process to complete the request
        { 
            (void) close(client); // child doesn't need to listen
        	memset(Request, 0, BUFSIZE);
        	bytes = read(new_fd, Request, BUFSIZE); //read in the client header into buf
        	if (bytes == 0)
        		break;
		    if (bytes < 0)
		    {
		        ErrorHandle(0, 500, new_fd, Method, URI, Version);
		        fputs("Error reading file", stderr);
		        break;
		    }
		    
		    sscanf(Request, "%s" "%s" "%s", token1, token2, token3); //Parse out the first line
		    Method = token1;
		    URI = token2;
		    Version = token3;

            if (Version != NULL) //Need to remove the return and newline characters to do a comparison for 400 error
		    {   
		        length = strlen(Version); 
		        if (Version[length-1] == ' ' || Version[length-1] == '\n' || Version[length-1] == '\r')
		            Version[length-1] = '\0';
		    }
			if (strcmp(Method,"GET") != 0)
			{
			 	ErrorHandle(1, 400, new_fd, Method, URI, Version);
			 	break;
			}
			else if (strstr(URI, "[") || strstr(URI, "]"))
			{
			    ErrorHandle(2, 400, new_fd, Method, URI, Version);
			    break;
			}
			else if (strcmp(Version, "HTTP/1.0\0") != 0 && strcmp(Version, "HTTP/1.1\0") != 0)
			{
			    ErrorHandle(3, 400, new_fd, Method, URI, Version);
			    break;
			}
			else
			{
				char *temp;
				strcpy(token1,token2);
				int flag = 0;
			   	int i = 7;
				while (i < strlen(token2)) //Getting the proper host URL
				{
					if (token2[i] == ':')
					{
						flag = 1;
						break;
					}
					i++;
				}
		   
				temp = strtok(token2,"//");
				if (flag == 0)
					temp = strtok(NULL,"/");
				else
					temp = strtok(NULL,":");
		   
				sprintf(token2,"%s",temp);
				printf("host = %s",token2); //Removal of the http://, to only have the host name
		   
				if(flag == 1)
					temp = strtok(NULL,"/");
		   
				strcat(token1, "^]"); //Setting up the rest of the path after the host name
				temp = strtok(token1, "//");
				temp = strtok(NULL, "/");
				if(temp!=NULL)
					temp=strtok(NULL, "^]");
				printf("\npath = %s\n", temp);
				
				char PathName[BUFSIZE];
				char *temp2;
				if(strstr(temp, "."))
				{
					
					memcpy(PathName, temp, strlen(temp));
					temp2 = strtok(PathName, "/");
					while (temp2 != NULL)
					{
						if (strstr(temp2, "."))
							break;
						temp2 = strtok(NULL, "/");
					}
				}

				//Check the cache
				char* HashName;
				if (strstr(temp, "."))
					HashName = temp2;
					//HashName = HashIt(temp, strlen(temp));
				else
					HashName = "index";

				FILE *CacheMoney = fopen((char*)HashName, "rb");
				if (CacheMoney != NULL)
				{
					struct stat st;
					stat((char*)HashName, &st);
					char* content = calloc(st.st_size, 1);
					fread(content, 1, st.st_size, CacheMoney);
					char timestamp[25];
					memcpy(timestamp, content, 25);
					fclose(CacheMoney);
					struct tm timeInfo;
					strptime(timestamp, "%a %b %d %H:%M:%S %Y", &timeInfo);
					time_t TimeIsMoney = mktime(&timeInfo);
					time_t currentTime = time(NULL);
					
					float diff = difftime(currentTime, TimeIsMoney);
					if (diff <= timeout)
					{
						int otherSize = st.st_size - 25;
						char* OtherContent = calloc(otherSize, 1);
						content += 25;
						OtherContent = content;
						send(new_fd, OtherContent, otherSize, 0);
						printf("Connection %d closed for pid %d\n", new_fd, getpid());
			            (void) close(new_fd); //Close the file descriptor for the child process
			            exit(0);
					}
				}

				//If not in cache, connect to server and send request
			    int fd = connectsock(URI);
			    if (fd == -1)
			    {
			    	ErrorHandle(0, 500, new_fd, Method, URI, Version);
			    	break;
			    }

			    memset(Request, 0, BUFSIZE);

			    if (temp != NULL) //creating a new header to send
			    	sprintf(Request,"GET /%s %s\r\nHost: %s\r\nConnection: close\r\n\r\n", temp, token3, token2);
				else
					sprintf(Request,"GET / %s\r\nHost: %s\r\nConnection: close\r\n\r\n", token3, token2);
				bytes = send(fd, Request, strlen(Request), 0);

				if (bytes < 0)
				{
					ErrorHandle(0, 500, new_fd, Method, URI, Version);
			        fputs("Error Sending Request", stderr);
			        break;
			    }
			    else
			    {
					time_t timestamp = time(NULL);
					char *currentTime = ctime(&timestamp);
					FILE *fp = fopen(HashName, "w+");
					if (fp != NULL)
						fprintf(fp, "%s", currentTime);
			    	do
					{
						memset(Request, 0, BUFSIZE);
						bytes = recv(fd, Request, BUFSIZE, 0);
						if (!(bytes <= 0))
						{
							send(new_fd, Request, bytes, 0);
							fwrite(Request, 1, bytes, fp);
						}
					}while (bytes > 0);
					fclose(fp);
			    }
			}
            printf("Connection %d closed for pid %d\n", new_fd, getpid());
            (void) close(new_fd); //Close the file descriptor for the child process
            exit(0);
        }
        (void) close(new_fd);  // parent doesn't need this
    }
    return 0;
}

void sigchld_handler(int s)
{
    // waitpid() might overwrite errno, so we save and restore it:
    int saved_errno = errno;
    while(waitpid(-1, NULL, WNOHANG) > 0);
    errno = saved_errno;
}

void *get_in_addr(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET)
        return &(((struct sockaddr_in*)sa)->sin_addr);

    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

int connectsock(const char *URI)
{

    int sock;
    int connectError; 

    // ESTABLISH SOCKET
    if((sock = socket(AF_INET, SOCK_STREAM, 0)) == -1){
        perror("socket");
        return -1;
    }
    // 
    int status;
	struct addrinfo hints;
	struct addrinfo *servinfo;  // will point to the result

	// INPUT STRUCT getadderinfo()
	memset(&hints, 0, sizeof hints); // make sure the struct is empty
	hints.ai_family = AF_INET;  	 // don't care IPv4 or IPv6
	hints.ai_socktype = SOCK_STREAM; // TCP stream sockets
	hints.ai_flags = AI_PASSIVE;     // fill in my IP for me

	if ((status = getaddrinfo(URI, "80", &hints, &servinfo)) != 0) 
	{
	    fprintf(stderr, "getaddrinfo error: %s\n", gai_strerror(status));
	    return -1;
	}

	// CONNECT
    connectError = connect(sock, servinfo->ai_addr, servinfo->ai_addrlen);
    if (connectError == -1)
    	return -1;

    freeaddrinfo(servinfo); // free the linked-list

    return sock; 
}

int ErrorHandle(int d, int code, int fd, char* Method, char* URI, char* Version)
{
    char error[BUFSIZE];
    char errorMsg[BUFSIZE];
    char length[256]; 
    //have to build the header to send back to the client even for the error
    if (code == 400)
    {   
        if (d == 1)
        {
            memcpy(error, "HTTP/1.1 400 Bad Request: Invalid Method: ", 42);
            memcpy(error + strlen(error), Method, strlen(Method));
        }
        else if (d == 2)
        {
            memcpy(error, "HTTP/1.1 400 Bad Request: Invalid URI: ", 39);
            memcpy(error + strlen(error), URI, strlen(URI));
        }
        else
        {
            memcpy(error, "HTTP/1.1 400 Bad Request: Invalid HTTP-Version: ", 47);
            memcpy(error + strlen(error), Version, strlen(Version));
        }
    }

    else if (code == 404)
    {
        memcpy(error, "HTTP/1.1 404 Not Found: ", 24);
        memcpy(error + strlen(error), URI, strlen(URI));
    }
    else if (code == 500)
        memcpy(error, "HTTP/1.1 500 Internal Server Error", 34);
    else
    {
        memcpy(error, "HTTP/1.1 501 Not Implemented: ", 30);
        memcpy(error + strlen(error), URI, strlen(URI));
    }
    memcpy(errorMsg, "<html><em> ", 11);
    memcpy(errorMsg + 11, error, strlen(error));
    memcpy(errorMsg + strlen(errorMsg), " </em></html>", 13);
    sprintf(length, "%ld", strlen(errorMsg));
    
    error[strlen(error)] = '\n';    
    memcpy(error + strlen(error), "Content-type: text/html\n", 24);
    memcpy(error + strlen(error), "Content-length: ", 16);
    memcpy(error + strlen(error), length, strlen(length));

    error[strlen(error)] = '\n';
    error[strlen(error)] = '\n';
    send(fd, error, strlen(error), 0);
    send(fd, errorMsg, strlen(errorMsg), 0);
    return 0;
}

unsigned char *HashIt(const char Request[BUFSIZE], int bytes)
{
	unsigned char *hash = calloc(MD5_DIGEST_LENGTH, 1);
	MD5_CTX mdContext;
	MD5_Init (&mdContext);
    MD5_Update (&mdContext, Request, bytes);    
    MD5_Final (hash,&mdContext);
	return hash;
}
