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

struct conf
{
	char *Username;
	char *Password;
	char *Username2;
	char *Password2;
	char Root[50];
};
extern int  errno;
struct conf parseConfig();

void sigchld_handler(int s);
void *get_in_addr(struct sockaddr *sa);
/*void GET();
void PUT();
void LIST();*/

int main(int argc, char* argv[])
{
	struct conf authorized;
	authorized = parseConfig();

	int dfs, new_fd;  // listen on sock_fd, new connection on new_fd
    struct addrinfo hints, *servinfo, *p;
    struct sockaddr_storage their_addr; // connector's address information
    socklen_t sin_size;
    struct sigaction sa;
    int yes = 1;
    char s[INET6_ADDRSTRLEN];
    int rv;
    //int pollRtn;
    if(argc < 3)
    {
    	fprintf(stderr, "\nIncorrect number of arguments\n");
        exit(1);
    }
    strcpy(authorized.Root, ".");
    strcat(authorized.Root, argv[1]);
    char *portnum = argv[2];

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
        if ((dfs = socket(p->ai_family, p->ai_socktype,
                p->ai_protocol)) == -1) {
            perror("\nserver: socket");
            continue;
        }

        if (setsockopt(dfs, SOL_SOCKET, SO_REUSEADDR, &yes,
                sizeof(int)) == -1) {
            perror("\nsetsockopt");
            exit(1);
        }

        if (bind(dfs, p->ai_addr, p->ai_addrlen) == -1) {
            close(dfs);
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

    if (listen(dfs, QLEN) == -1) 
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

    while(1) 
    {  // main accept() loop for dfs
        sin_size = sizeof their_addr;
        new_fd = accept(dfs, (struct sockaddr *)&their_addr, &sin_size); //listening for a client
        if (new_fd == -1) 
        {
            perror("accept");
            continue;
        }

        inet_ntop(their_addr.ss_family, //tells us if we got a connection
            get_in_addr((struct sockaddr *)&their_addr),
            s, sizeof s);
        printf("server port %s: got connection from %s\n", argv[2], s);

/*	        if (!fork()) // We fork to allow the child process to complete the request
        { 
            (void) close(dfs); // child doesn't need the listener
            while(1)
            {   
                struct pollfd ufds[1]; //This struct is used for the poll() for timing
                ufds[0].fd = new_fd;
                ufds[0].events = POLLIN;
                pollRtn = poll(ufds, 1, 10000);
                if (pollRtn == -1) //Error when getting a poll
                {
                    printf("Error when creating poll for child fork\n");
                    ErrorHandle(0, 500, new_fd, " ", " ", " ");
                }
                else if (pollRtn == 0) //timed out
                {
                    printf("%d Timed out\n", getpid());
                    break;
                }
                else //Otherwise, continue with fulfilling the client request
                {
                    if (ClientInput(new_fd) == 0) 
                        perror("send");
                    if (strcmp(Connection, "close") == 0) //If the connection is close, then immediately break out
                        break;
                }
            }
            (void) close(new_fd); //Close the file descriptor for the child process
            exit(0);
       }*/
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

struct conf parseConfig()
{
	char *line, *token;
	char *source = calloc(BUFSIZE, 1);
	int count = 0;
	int counter = 0;
	struct conf authorized;
	FILE *dfsConfig = fopen("dfs.conf", "r"); //open the config file of authorized users

	if (dfsConfig != NULL)
    {
        size_t newLen = fread(source, 1, BUFSIZE, dfsConfig);
        if (newLen == 0)
            fputs("Error reading file", stderr);
        fclose(dfsConfig);
    }

	while ((line = strsep(&source, "\n")) != NULL)
	{
		while ((token = strsep(&line, " ")) != NULL)
		{
			if (count == 0)	
				if (counter == 0)
					authorized.Username = token;
				else
					authorized.Password = token;
			else
				if (counter == 0)
					authorized.Username2 = token;
				else
					authorized.Password2 = token;
			counter++;
		}
		count++;
		counter = 0;
	}
	free(source);
	return authorized;
}

/*void GET()
{

}

void LIST()
{

}

void PUT()
{
	
}*/