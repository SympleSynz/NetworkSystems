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
#include <pthread.h>

#define QLEN    32 
#define BUFSIZE	4096

extern int  errno;
struct Cache
{
	char *path;
	int size;
	time_t timestamp;
	char *content;
};

void *get_in_addr(struct sockaddr *sa);
int connectsock(const char *URI);
void ErrorHandle(int d, int code, int fd, char* Method, char* URI, char* Version);
void *SendRequest(void *s);
struct Cache *CacheMoney;
/*pthread_mutex_t rlock;
pthread_mutex_t slock;
pthread_mutex_t clok;
pthread_mutex_t threadLock;*/
pthread_t ThreadCount;
int timeout;

int main(int argc, char* argv[])
{
	int client, new_fd, rv;  
    struct addrinfo hints, *servinfo, *p;
    struct sockaddr_storage their_addr;
    socklen_t sin_size;
    int yes = 1;
    char *portnum = NULL;
    char s[INET6_ADDRSTRLEN];

    CacheMoney = calloc(1000, sizeof(CacheMoney));

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

    printf("\nserver port %s: waiting for connections...\n", portnum);

	//Main accept() loop for webproxy
/*    if (pthread_mutex_init(&rlock, NULL) != 0)
        printf("\n Mutex init failed\n");
    if (pthread_mutex_init(&slock, NULL) != 0)
        printf("\n Mutex init failed\n");
    if (pthread_mutex_init(&clok, NULL) != 0)
        printf("\n Mutex init failed\n");*/
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
        printf("new_fd: %d\n", new_fd);
        if(pthread_create(&ThreadCount, NULL, &SendRequest,(void*) &new_fd)) 
        {
            fprintf(stderr, "Error creating thread\n");
            return 1;
        }
    }
    return 0;
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

void ErrorHandle(int d, int code, int fd, char* Method, char* URI, char* Version)
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
    {
        memcpy(error, "HTTP/1.1 500 Internal Server Error", 33);
        memcpy(errorMsg, error, strlen(error));
    }
    else
    {
        memcpy(error, "HTTP/1.1 501 Not Implemented: ", 30);
        memcpy(error + strlen(error), URI, strlen(URI));
    }
    memcpy(errorMsg, "<html><em> ", 11);
    memcpy(errorMsg + strlen(errorMsg), error, strlen(error));
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
    return;
}

void *SendRequest(void *s)
{ 
    int new_fd = *(int *)s;
    char Request[BUFSIZE];
    char *Method = NULL, *URI = NULL, *Version = NULL;
    char token1[300], token2[300], token3[300];
    int length, bytes;

    memset(Request, 0, BUFSIZE);

    //pthread_mutex_lock(&rlock);
    bytes = read(new_fd, Request, BUFSIZE); //read in the client header into buf
    //pthread_mutex_unlock(&rlock);
    if (bytes == 0)
    {
        (void) close(new_fd); //Close the file descriptor for the child process
        //pthread_mutex_unlock(&threadLock);
        pthread_exit(NULL);
    }
    if (bytes < 0)
    {
        //pthread_mutex_lock(&slock);
        ErrorHandle(0, 500, new_fd, Method, URI, Version);
        fputs("Error reading header\n", stderr);
        (void) close(new_fd); //Close the file descriptor for the child process
        //pthread_mutex_unlock(&slock);
        //pthread_mutex_unlock(&threadLock);
        pthread_exit(NULL);
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
        //pthread_mutex_lock(&slock);
        ErrorHandle(1, 400, new_fd, Method, URI, Version);
        //pthread_mutex_unlock(&slock);
        (void) close(new_fd); //Close the file descriptor for the child process
        //pthread_mutex_unlock(&threadLock);
        pthread_exit(NULL);
    }
    if (strcmp(Version, "HTTP/1.0\0") != 0 && strcmp(Version, "HTTP/1.1\0") != 0)
    {
        //pthread_mutex_lock(&slock);
        ErrorHandle(3, 400, new_fd, Method, URI, Version);
        //pthread_mutex_unlock(&slock);
        (void) close(new_fd); //Close the file descriptor for the child process
        //pthread_mutex_unlock(&threadLock);
        pthread_exit(NULL);
    }

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

    //Check the cache
    int index = 0;
    int found = 0;
    //pthread_mutex_lock(&clok);
    while (CacheMoney[index].path != NULL)
    {
        if (index >= 1000)
        {
            printf("\nNot found in Cache. Cache Full\n");
            break;
        }
        if (strcmp(CacheMoney[index].path, temp) == 0)
        {
            printf("\nFOUND A MATCH\n");
            double diff = 0.0;
            time_t time1;
            time(&time1);
            diff = difftime(CacheMoney[index].timestamp, time1);
            if (diff <= timeout)
            {
                CacheMoney[index].timestamp = time1;
                found = 1;
            }
            else
                break;
        }
        index++;
    }
    //pthread_mutex_unlock(&clok);
    
    if (found == 1)
    {
        //pthread_mutex_lock(&slock);
        send(new_fd, CacheMoney[index].content, CacheMoney[index].size, 0);
        //pthread_mutex_lock(&slock);
        printf("\nFound in Cache. Connection %d closed\n\n", new_fd);
        (void) close(new_fd); //Close the file descriptor for the child process
        //pthread_mutex_unlock(&threadLock);
        pthread_exit(NULL);
    }

    //If not in cache, connect to server and send request
    int fd = connectsock(URI);
    printf("fd: %d\n", fd);
    if (fd == -1)
    {
        //pthread_mutex_lock(&slock);
        ErrorHandle(0, 500, new_fd, Method, URI, Version);
        //pthread_mutex_lock(&slock);
        (void) close(new_fd); //Close the file descriptor for the child process
        //pthread_mutex_unlock(&threadLock);
        pthread_exit(NULL);;
    }

    memset(Request, 0, BUFSIZE);
    if (temp != NULL) //creating a new header to send
        sprintf(Request,"GET /%s %s\r\nHost: %s\r\nConnection: close\r\n\r\n", temp, token3, token2);
    else
        sprintf(Request,"GET / %s\r\nHost: %s\r\nConnection: close\r\n\r\n", token3, token2);

    //pthread_mutex_lock(&slock);
    bytes = send(fd, Request, strlen(Request), 0);
    //pthread_mutex_unlock(&slock);

    if (bytes < 0)
    {
        //pthread_mutex_lock(&slock);
        ErrorHandle(0, 500, new_fd, Method, URI, Version);
        //pthread_mutex_lock(&slock);
        fputs("Error Sending Request", stderr);
        (void) close(new_fd); //Close the file descriptor for the child process
        //pthread_mutex_unlock(&threadLock);
        pthread_exit(NULL);
    }
    else
    {
        do
        {
            memset(Request, 0, BUFSIZE);
            //pthread_mutex_lock(&rlock);
            bytes = read(fd, Request, BUFSIZE);
            //pthread_mutex_unlock(&rlock);
            if (index < 1000)
            {
                //pthread_mutex_lock(&clok);
                while (CacheMoney[index].path != NULL)
                    index++;
                CacheMoney[index].path = temp;
                CacheMoney[index].size = bytes;
                time(&CacheMoney[index].timestamp);
                CacheMoney[index].content = calloc(bytes, 1);
                memcpy(CacheMoney[index].content, Request, bytes);
                //pthread_mutex_unlock(&clok);
            }
            else
                printf("Cache is full index: %d\n", index);
            //pthread_mutex_lock(&slock);
            if (!(bytes <= 0))
                send(new_fd, Request, bytes, 0);
            //pthread_mutex_unlock(&slock);
        }while (bytes > 0);
    }
    printf("Connection %d closed\n", new_fd);
    (void) close(new_fd); //Close the file descriptor for the child process
    //pthread_mutex_unlock(&threadLock);
    pthread_exit(NULL);
}