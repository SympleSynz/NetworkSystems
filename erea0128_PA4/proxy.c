#include <errno.h>
#include <arpa/inet.h>
#include <signal.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/resource.h>
#include <sys/wait.h>
#include <sys/errno.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/shm.h>
#include <dirent.h>
#include <netinet/in.h>
#include <netdb.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <time.h>

#include <stdarg.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define QLEN    32 
#define BUFSIZE	4096

extern int  errno;
void sigchld_handler(int s);
void *get_in_addr(struct sockaddr *sa);
int connectsock(const char *URI);

int main(int argc, char* argv[])
{
	int client, new_fd, bytes;  
    struct sockaddr_in c_hints;
    struct sockaddr_storage their_addr;
    socklen_t sin_size;
    struct sigaction sa;
    char *portnum = NULL;
    char s[INET6_ADDRSTRLEN];
    char DnatRules[BUFSIZE];

   	portnum = "8080";
   	//Open up a socket for listening
	if ((client = socket(AF_INET, SOCK_STREAM, 0)) == -1)
    {
    	perror("\nserver: socket");
    	exit(-1);
    }
    //Setting the default values for client's addr info
    c_hints.sin_family = AF_INET;
	c_hints.sin_port = htons(atoi(portnum));
	c_hints.sin_addr.s_addr = INADDR_ANY; 
	bzero(&c_hints.sin_zero, 8);

	//Bind to the socket indicated above
	sin_size = sizeof their_addr;
	if((bind(client, (struct sockaddr *) &c_hints, sin_size)) == -1)
	{
		perror("bind: ");
		exit(-1);
	}
	//Start listening on the port number
	if((listen(client, 10)) == -1)
	{
		perror("listen: ");
		exit(-1);
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

    int fd;
	struct ifreq ifr;
	//This grabs the information from ifconfig for eth1, in order to write the DNAT rule
	fd = socket(AF_INET, SOCK_DGRAM, 0);
	ifr.ifr_addr.sa_family = AF_INET;
	strncpy(ifr.ifr_name, "eth1", IFNAMSIZ-1);
	ioctl(fd, SIOCGIFADDR, &ifr);
	close(fd);

	//Writing out the DNAT rule
    sprintf(DnatRules, "iptables -t nat -A PREROUTING -p tcp -i eth1 -j DNAT --to %s:%d", inet_ntoa(((struct sockaddr_in *)&ifr.ifr_addr)->sin_addr), ntohs(c_hints.sin_port));
    printf("DNAT RULES: %s\n", DnatRules);
    system(DnatRules);
    printf("\nserver port %d: waiting for connections...\n", ntohs(c_hints.sin_port));

	//Main accept() loop for Proxy
    while(1) 
    {  	
        sin_size = sizeof their_addr;
        //listening for a client
        new_fd = accept(client, (struct sockaddr *)&their_addr, &sin_size); 
        if (new_fd == -1) 
        {
            perror("accept");
            continue;
        }
        //Tells if got a connection
        inet_ntop(their_addr.ss_family, 
            get_in_addr((struct sockaddr *)&their_addr),
            s, sizeof s);
        printf("server port %s: got connection from %s\n", portnum, s);

        //Fork to allow the child process to complete the request
		if (!fork()) 
        { 
        	//Child doesn't need to listen
            (void) close(client); 
		    int server;
		    //Need all these to bind to a dynamically chosen port as well as
		    //get the information necessary for SNAT rules
		    struct sockaddr_in s_hints;
		    struct sockaddr_in server_addr;
		    struct sockaddr_in p_addr;
		    struct sockaddr_in d_addr;
			socklen_t s_addrlen; 
			socklen_t p_addrlen; 
			socklen_t d_addrlen; 
			s_addrlen = sizeof(struct sockaddr_in);
			p_addrlen = sizeof(struct sockaddr_in);			
			d_addrlen = sizeof(struct sockaddr_in);

			//Same as above, open a socket for binding
		    if ((server = socket(AF_INET, SOCK_STREAM, 0)) == -1)
		    {
		    	perror("\nserver: socket");
		    	exit(-1);
		    }
		    //Set the default values for the server addr info
		    s_hints.sin_family = AF_INET;
			s_hints.sin_port = 0;
			s_hints.sin_addr.s_addr = INADDR_ANY; 
			bzero(&s_hints.sin_zero, 8);

			sin_size = sizeof their_addr;
			//Bind to a port (any open port since the port is set to 0)
			if((bind(server, (struct sockaddr *) &s_hints, sin_size)) == -1)
			{
				perror("bind: ");
				exit(-1);
			}
			//Need information for SNAT rules, getting the server info
			if((getsockname(server, (struct sockaddr *) &server_addr, &s_addrlen)) == -1){
				perror("getsockname: ");
				exit(-1);		
			}
			//Need information for SNAT rules, getting the client info
			if((getpeername(new_fd, (struct sockaddr *) &p_addr, &p_addrlen)) == -1){
				perror("getpeername: ");
				exit(-1);		
			}
			//Need information for SNAT rules, getting the original destination (in write up)
			if((getsockopt(new_fd, SOL_IP, 80, (struct sockaddr *) &d_addr, &d_addrlen)) == -1){
				perror("getsockopt: ");
				exit(-1);
			}
			//Write out the SNAT rule to the ip table
		    char SnatRules[BUFSIZE];
		    sprintf(SnatRules, "iptables -t nat -A POSTROUTING -p tcp -j SNAT --sport %d --to-source %s", ntohs(server_addr.sin_port), inet_ntoa(p_addr.sin_addr));
		    printf("%s\n", SnatRules);	
		    system(SnatRules);

		    int server_fd;
		    //Finally, connect to the server
		  	server_fd = connect(server, (struct sockaddr *) &d_addr, d_addrlen);
		    if (server_fd == -1)
		    	break;
		    //For the log file, start getting the information in to a single string
		    char *logfile;
		    char logs[BUFSIZE];
		    time_t currentTime;
		    currentTime = time(NULL);
		    //Getting the date and time
        	logfile = ctime(&currentTime);
        	//Replacing the new line character
        	logfile[strlen(logfile)-1] = ' ';
        	//Add the client's IP address to the logfile
        	strcat(logfile, inet_ntoa(p_addr.sin_addr));
        	//Convert the client's port number to a string
        	sprintf(logs, " %d ", ntohs(p_addr.sin_port));
        	strcat(logfile, logs);
        	//Add the server's IP address to the logfile
        	strcat(logfile, inet_ntoa(d_addr.sin_addr));
        	memset(logs, 0, BUFSIZE);
        	//Convert the server's port number to a string
        	sprintf(logs, " %d ", ntohs(d_addr.sin_port));
        	strcat(logfile, logs);

		    char Request[BUFSIZE];
			memset(Request, 0, BUFSIZE);
			//Read in from the client
			bytes = read(new_fd, Request, BUFSIZE);
			char byteSize[BUFSIZE];
			//Bytes sent from client
			if (!(bytes <= 0))
				//Send request to server
				bytes = send(server, Request, strlen(Request), 0);

			sprintf(byteSize, "%d ", bytes);
			strcat(logfile, byteSize);

			memset(byteSize, 0, BUFSIZE);
			int bytesSent;
			memset(Request, 0, BUFSIZE);
    		//Read in from the server
			bytes = recv(server, Request, BUFSIZE, 0);
			bytesSent += bytes;
			if (!(bytes <= 0))
				//Send response from server to client
				bytes = send(new_fd, Request, bytes, 0);
    		
			//Bytes received from the server
			sprintf(byteSize, "%d\n", bytesSent);
			strcat(logfile, byteSize);

        	printf("Connection %d closed for pid %d\n", new_fd, getpid());
        	memset(SnatRules, 0, BUFSIZE);
        	//Delete the SNAT rule since no longer needed
        	sprintf(SnatRules, "iptables -t nat -D POSTROUTING -p tcp -j SNAT --sport %d --to-source %s", ntohs(server_addr.sin_port), inet_ntoa(p_addr.sin_addr));
		    printf("%s\n", SnatRules);
		    system(SnatRules);
		    //Close the connection
            (void) close(new_fd); //Close the file descriptor for the child process
            //Write out to the logfile
            FILE * FileLog = fopen("logfile.txt", "a+");
            fprintf(FileLog, "%s", logfile);
            fclose(FileLog);
            //Child process exits
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

