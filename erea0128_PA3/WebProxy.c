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

int main(int argc, char* argv[])
{
	int client, new_fd, rv, bytes, AuthBool, pollRtn;  
    struct addrinfo hints, *servinfo, *p;
    struct sockaddr_storage their_addr; 
    socklen_t sin_size;
    struct sigaction sa;
    int yes = 1;
    char *portnum = "8080";
    char s[INET6_ADDRSTRLEN];
    char Request[BUFSIZE];

    if(argc < 3)
    {
    	fprintf(stderr, "\nIncorrect number of arguments\n");
        exit(1);
    }

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

    printf("\nserver port %s: waiting for connections...\n", argv[2]);
//Main accept() loop for dfs
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
        printf("server port %s: got connection from %s\n", argv[2], s);

		//Start listening for content
		if (!fork()) // We fork to allow the child process to complete the request
        { 
            (void) close(client); // child doesn't need the listener
            while(1)
            {   
            	memset(Request, 0, 4);
            	read(new_fd, Request, 4);

				/*struct pollfd ufds[1]; //This struct is used for the poll() for timing
                ufds[0].fd = new_fd;
                ufds[0].events = POLLIN;
                pollRtn = poll(ufds, 1, 1000);
                if (pollRtn == -1) //Error when getting a poll
                    printf("Error when creating poll for child fork\n");   
                else if (pollRtn == 0) //timed out
                {
                    printf("%d Timed out\n", getpid());
                    break;
                }
                else //Otherwise, continue with fulfilling the client request
                {
                	
                }*/
            }
            printf("Connection %d closed\n", new_fd);
            (void) close(new_fd); //Close the file descriptor for the child process
            exit(0);
       }
       (void) close(new_fd);  // parent doesn't need this
    }
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

int connectsock(const char *host, const char *portnum)
{
    struct hostent  *phe;   /* pointer to host information entry    */
    struct sockaddr_in sin; /* an Internet endpoint address         */
    int s;              	/* socket descriptor                    */


    memset(&sin, 0, sizeof(sin));
    sin.sin_family = AF_INET;

	/* Map port number (char string) to port number (int)*/
    if ((sin.sin_port=htons((unsigned short)atoi(portnum))) == 0)
            errexit("can't get \"%s\" port number\n", portnum);

	/* Map host name to IP address, allowing for dotted decimal */
    if ((phe = gethostbyname(host)))
            memcpy(&sin.sin_addr, phe->h_addr, phe->h_length);
    else if ( (sin.sin_addr.s_addr = inet_addr(host)) == INADDR_NONE )
            errexit("can't get \"%s\" host entry\n", host);

	/* Allocate a socket */
    s = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (s < 0)
            errexit("can't create socket: %s\n", strerror(errno));

	/* Connect the socket */
    if (connect(s, (struct sockaddr *)&sin, sizeof(sin)) < 0)
            errexit("can't connect to %s.%s: %s\n", host, portnum,
                    strerror(errno));
    return s;
}