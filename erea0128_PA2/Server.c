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
	char *Root;
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
	int pid;
	struct conf authorized;
	authorized.Root = "./";
	authorized = parseConfig();

	int dfs1, dfs2, dfs3, dfs4, new_fd1, new_fd2, new_fd3, new_fd4;  // listen on sock_fd, new connection on new_fd
    struct addrinfo hints1, hints2, hints3, hints4, *servinfo, *p1, *p2, *p3, *p4;
    struct sockaddr_storage their_addr1, their_addr2, their_addr3, their_addr4; // connector's address information
    socklen_t sin_size1, sin_size2, sin_size3, sin_size4;
    struct sigaction sa;
    int yes = 1;
    char s1[INET6_ADDRSTRLEN];
    char s2[INET6_ADDRSTRLEN];
    char s3[INET6_ADDRSTRLEN];
    char s4[INET6_ADDRSTRLEN];
    int rv;
    //int pollRtn;
    
    char *portnum1 = "10001";
    char *portnum2 = "10002";
    char *portnum3 = "10003";
    char *portnum4 = "10004";

    memset(&hints1, 0, sizeof hints1);
    hints1.ai_family = AF_UNSPEC;
    hints1.ai_socktype = SOCK_STREAM;
    hints1.ai_flags = AI_PASSIVE; // use my IP
    
    memset(&hints2, 0, sizeof hints2);
    hints2.ai_family = AF_UNSPEC;
    hints2.ai_socktype = SOCK_STREAM;
    hints2.ai_flags = AI_PASSIVE; // use my IP
    
    memset(&hints3, 0, sizeof hints3);
    hints3.ai_family = AF_UNSPEC;
    hints3.ai_socktype = SOCK_STREAM;
    hints3.ai_flags = AI_PASSIVE; // use my IP
    
    memset(&hints4, 0, sizeof hints4);
    hints4.ai_family = AF_UNSPEC;
    hints4.ai_socktype = SOCK_STREAM;
    hints4.ai_flags = AI_PASSIVE; // use my IP

    //DFS 1 binding to socket
    if ((rv = getaddrinfo(NULL, portnum1, &hints1, &servinfo)) != 0) //Attempting to bind to portnum 10001
    {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return 1;
    }
    // loop through all the results and bind to the first we can
    for(p1 = servinfo; p1 != NULL; p1 = p1->ai_next) 
    {
        if ((dfs1 = socket(p1->ai_family, p1->ai_socktype,
                p1->ai_protocol)) == -1) {
            perror("server: socket");
            continue;
        }

        if (setsockopt(dfs1, SOL_SOCKET, SO_REUSEADDR, &yes,
                sizeof(int)) == -1) {
            perror("setsockopt");
            exit(1);
        }

        if (bind(dfs1, p1->ai_addr, p1->ai_addrlen) == -1) {
            close(dfs1);
            perror("server: bind");
            continue;
        }

        break;
    }

    freeaddrinfo(servinfo); // all done with this structure
    if (p1 == NULL)  
    {
        fprintf(stderr, "server: failed to bind\n");
        exit(1);
    }

    if (listen(dfs1, QLEN) == -1) 
    {
        perror("listen");
        exit(1);
    }

  	//DFS 2 binding to socket
    if ((rv = getaddrinfo(NULL, portnum2, &hints2, &servinfo)) != 0) //Attempting to bind to portnum 10002
    {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return 1;
    }
    // loop through all the results and bind to the first we can
    for(p2 = servinfo; p2 != NULL; p2 = p2->ai_next) 
    {
        if ((dfs2 = socket(p2->ai_family, p2->ai_socktype,
                p2->ai_protocol)) == -1) {
            perror("server: socket");
            continue;
        }

        if (setsockopt(dfs2, SOL_SOCKET, SO_REUSEADDR, &yes,
                sizeof(int)) == -1) {
            perror("setsockopt");
            exit(1);
        }

        if (bind(dfs2, p2->ai_addr, p2->ai_addrlen) == -1) {
            close(dfs2);
            perror("server: bind");
            continue;
        }

        break;
    }

    freeaddrinfo(servinfo); // all done with this structure
    if (p2 == NULL)  
    {
        fprintf(stderr, "server: failed to bind\n");
        exit(1);
    }

    if (listen(dfs2, QLEN) == -1) 
    {
        perror("listen");
        exit(1);
    }

    //DFS 3 binding to socket
    if ((rv = getaddrinfo(NULL, portnum3, &hints3, &servinfo)) != 0) //Attempting to bind to portnum 10003
    {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return 1;
    }
    // loop through all the results and bind to the first we can
    for(p3 = servinfo; p3 != NULL; p3 = p3->ai_next) 
    {
        if ((dfs3 = socket(p3->ai_family, p3->ai_socktype,
                p3->ai_protocol)) == -1) {
            perror("server: socket");
            continue;
        }

        if (setsockopt(dfs3, SOL_SOCKET, SO_REUSEADDR, &yes,
                sizeof(int)) == -1) {
            perror("setsockopt");
            exit(1);
        }

        if (bind(dfs3, p3->ai_addr, p3->ai_addrlen) == -1) {
            close(dfs3);
            perror("server: bind");
            continue;
        }

        break;
    }

    freeaddrinfo(servinfo); // all done with this structure
    if (p3 == NULL)  
    {
        fprintf(stderr, "server: failed to bind\n");
        exit(1);
    }

    if (listen(dfs3, QLEN) == -1) 
    {
        perror("listen");
        exit(1);
    }

    //DFS 4 binding to socket
    if ((rv = getaddrinfo(NULL, portnum4, &hints4, &servinfo)) != 0) //Attempting to bind to portnum 10004
    {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return 1;
    }
    // loop through all the results and bind to the first we can
    for(p4 = servinfo; p4 != NULL; p4 = p4->ai_next) 
    {
        if ((dfs4 = socket(p4->ai_family, p4->ai_socktype,
                p4->ai_protocol)) == -1) {
            perror("server: socket");
            continue;
        }

        if (setsockopt(dfs4, SOL_SOCKET, SO_REUSEADDR, &yes,
                sizeof(int)) == -1) {
            perror("setsockopt");
            exit(1);
        }

        if (bind(dfs4, p4->ai_addr, p4->ai_addrlen) == -1) {
            close(dfs4);
            perror("server: bind");
            continue;
        }

        break;
    }

    freeaddrinfo(servinfo); // all done with this structure
    if (p4 == NULL)  
    {
        fprintf(stderr, "server: failed to bind\n");
        exit(1);
    }

    if (listen(dfs4, QLEN) == -1) 
    {
        perror("listen");
        exit(1);
    }

    //Handling all the child processes
    sa.sa_handler = sigchld_handler; // reap all dead processes
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    if (sigaction(SIGCHLD, &sa, NULL) == -1) 
    {
        perror("sigaction");
        exit(1);
    }

    printf("servers: waiting for connections...\n");

    if (!fork())
    {
	    while(1) 
	    {  // main accept() loop for dfs1
	        sin_size1 = sizeof their_addr1;
	        new_fd1 = accept(dfs1, (struct sockaddr *)&their_addr1, &sin_size1); //listening for a client
	        if (new_fd1 == -1) {
	            perror("accept");
	            continue;
	        }

	        inet_ntop(their_addr1.ss_family, //tells us if we got a connection
	            get_in_addr((struct sockaddr *)&their_addr1),
	            s1, sizeof s1);
	        printf("server: got connection from %s\n", s1);

/*	        if (!fork()) // We fork to allow the child process to complete the request
	        { 
	            (void) close(dfs1); // child doesn't need the listener
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
	       (void) close(new_fd1);  // parent doesn't need this
	    }
	}
	if (!fork())
    {
	    while(1) 
	    {  // main accept() loop for dfs2
	        sin_size2 = sizeof their_addr2;
	        new_fd2 = accept(dfs2, (struct sockaddr *)&their_addr2, &sin_size2); //listening for a client
	        if (new_fd2 == -1) {
	            perror("accept");
	            continue;
	        }

	        inet_ntop(their_addr2.ss_family, //tells us if we got a connection
	            get_in_addr((struct sockaddr *)&their_addr2),
	            s2, sizeof s2);
	        printf("server: got connection from %s\n", s2);
	       
	       (void) close(new_fd2);  // parent doesn't need this
	    }
	}
	if (!fork())
    {
	    while(1) 
	    {  // main accept() loop for dfs3
	        sin_size3 = sizeof their_addr3;
	        new_fd3 = accept(dfs3, (struct sockaddr *)&their_addr3, &sin_size3); //listening for a client
	        if (new_fd3 == -1) {
	            perror("accept");
	            continue;
	        }

	        inet_ntop(their_addr3.ss_family, //tells us if we got a connection
	            get_in_addr((struct sockaddr *)&their_addr3),
	            s3, sizeof s3);
	        printf("server: got connection from %s\n", s3);
	       
	       (void) close(new_fd3);  // parent doesn't need this
	    }
	}
	if (!fork())
    {
	    while(1) 
	    {  // main accept() loop for dfs4
	        sin_size4 = sizeof their_addr4;
	        new_fd4 = accept(dfs4, (struct sockaddr *)&their_addr4, &sin_size4); //listening for a client
	        if (new_fd4 == -1) {
	            perror("accept");
	            continue;
	        }

	        inet_ntop(their_addr4.ss_family, //tells us if we got a connection
	            get_in_addr((struct sockaddr *)&their_addr4),
	            s4, sizeof s4);
	        printf("server: got connection from %s\n", s4);
	       
	       (void) close(new_fd4);  // parent doesn't need this
	    }
	}
	do
	{
		pid = wait(NULL);
	}while(pid != -1);
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