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

#define QLEN    32 
#define BUFSIZE	4096

extern int  errno;

void sigchld_handler(int s);
void *get_in_addr(struct sockaddr *sa);
int connectsock(const char *URI);
int ErrorHandle(int d, int code, int fd, char* Method, char* URI, char* Version);

int main(int argc, char* argv[])
{
	int client, new_fd, rv, bytes, length;  
    struct addrinfo hints, *servinfo, *p;
    struct sockaddr_storage their_addr;
    socklen_t sin_size;
    struct sigaction sa;
    int yes = 1;
    char *Method = NULL, *URI = NULL, *Version = NULL, *portnum = NULL;
    char token1[300], token2[300], token3[300];
    char s[INET6_ADDRSTRLEN];
    char Request[BUFSIZE];

    if(argc < 2)
    	portnum = "8080";
    else
    	portnum = argv[1];

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
            (void) close(client); // child doesn't need the listener
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
		    
		    printf("Request before token: %s\n", Request);
		    sscanf(Request, "%s" "%s" "%s", token1, token2, token3);
		    Method = token1;
		    URI = token2;
		    Version = token3;
		    printf("Request: %s\n", Request);
            printf("Method: %s\n", Method);
            printf("URI: %s\n", URI);
            printf("Version: %s\n", Version);
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
				while (i < strlen(token2))
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
				printf("host = %s",token2);
		   
				if(flag==1)
					temp=strtok(NULL,"/");
		   
				strcat(token1,"^]");
				temp = strtok(token1,"//");
				temp = strtok(NULL,"/");
				if(temp!=NULL)
					temp=strtok(NULL,"^]");
				printf("\npath = %s\n", temp);

			    int fd = connectsock(URI);
			    if (fd == -1)
			    {
			    	ErrorHandle(0, 500, new_fd, Method, URI, Version);
			    	break;
			    }

			    memset(Request, 0, BUFSIZE);
			    if (temp != NULL)
			    	sprintf(Request,"GET /%s %s\r\nHost: %s\r\nConnection: close\r\n\r\n",temp,token3,token2);
				else
					sprintf(Request,"GET / %s\r\nHost: %s\r\nConnection: close\r\n\r\n",token3,token2);

				bytes = send(fd, Request, strlen(Request), 0);

				if (bytes < 0)
				{
					ErrorHandle(0, 500, new_fd, Method, URI, Version);
			        fputs("Error Sending Request", stderr);
			        break;
			    }
			    else
			    {
			    	do
					{
						memset(Request, 0, BUFSIZE);
						bytes = read(fd, Request, BUFSIZE);
						if(!(bytes <= 0))
							send(new_fd, Request, bytes, 0);
					}while (bytes > 0);
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

/*int connectsock(const char *host, const char *portnum)
{
    struct hostent  *phe;   // pointer to host information entry    
    struct sockaddr_in sin; // an Internet endpoint address         
    int s;              	// socket descriptor                    

    memset(&sin, 0, sizeof(sin));
    sin.sin_family = AF_INET;

	// Map port number (char string) to port number (int)
    if ((sin.sin_port=htons((unsigned short)atoi(portnum))) == 0)
    {
        printf("can't get \"%s\" port number\n", portnum);
    	return -1;
    }

	// Map host name to IP address, allowing for dotted decimal 
    if ((phe = gethostbyname(host)))
        memcpy(&sin.sin_addr, phe->h_addr, phe->h_length);
    else if ( (sin.sin_addr.s_addr = inet_addr(host)) == INADDR_NONE )
    {
        printf("can't get \"%s\" host entry\n", host);
    	return -1;
    }

	// Allocate a socket 
    s = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (s < 0)
    {
        printf("can't create socket: %s\n", strerror(errno));
    	return -1;
    }

	// Connect the socket 
    if (connect(s, (struct sockaddr *)&sin, sizeof(sin)) < 0)
    {
        printf("can't connect to %s.%s: %s\n", host, portnum,
            strerror(errno));
        return -1;
    }
    return s;
}*/

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
    return 0;
}