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

#define QLEN    32    // how many pending connections queue will hold
#define BUFSIZE     4096


extern int  errno;
static char    *DocumentRoot, *port, *DocIndex, *ContentType, *Connection;

void sigchld_handler(int s);
void *get_in_addr(struct sockaddr *sa);
int     ClientInput(int fd);
int     ErrorHandle(int d, int code, int fd, char* Method, char* URI, char* Version);
int     GET(int fd, char* Method, char* URI, char* Version);
char *  wsConfig();

/*------------------------------------------------------------------------
 * sigchild_handler - for forking
 *------------------------------------------------------------------------
 */
void sigchld_handler(int s)
{
    // waitpid() might overwrite errno, so we save and restore it:
    int saved_errno = errno;
    while(waitpid(-1, NULL, WNOHANG) > 0);
    errno = saved_errno;
}

/*------------------------------------------------------------------------
 * get_in_addr - get sockaddr, IPv4 or IPv6
 *------------------------------------------------------------------------
 */
void *get_in_addr(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET)
        return &(((struct sockaddr_in*)sa)->sin_addr);

    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

/*------------------------------------------------------------------------
 * main - Concurrent TCP server for a GET service
 *------------------------------------------------------------------------
 */
int main(void)
{
    int sockfd, new_fd;  // listen on sock_fd, new connection on new_fd
    struct addrinfo hints, *servinfo, *p;
    struct sockaddr_storage their_addr; // connector's address information
    socklen_t sin_size;
    struct sigaction sa;
    int yes = 1;
    char s[INET6_ADDRSTRLEN];
    int rv;
    int pollRtn;
    char *portnum = wsConfig();


    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE; // use my IP

    if ((rv = getaddrinfo(NULL, portnum, &hints, &servinfo)) != 0) //Attempting to bind to the given portnum
    {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return 1;
    }

    // loop through all the results and bind to the first we can
    for(p = servinfo; p != NULL; p = p->ai_next) 
    {
        if ((sockfd = socket(p->ai_family, p->ai_socktype,
                p->ai_protocol)) == -1) {
            perror("server: socket");
            continue;
        }

        if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes,
                sizeof(int)) == -1) {
            perror("setsockopt");
            exit(1);
        }

        if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
            close(sockfd);
            perror("server: bind");
            continue;
        }

        break;
    }

    freeaddrinfo(servinfo); // all done with this structure

    if (p == NULL)  
    {
        fprintf(stderr, "server: failed to bind\n");
        exit(1);
    }

    if (listen(sockfd, QLEN) == -1) 
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

    printf("server: waiting for connections...\n");

    while(1) 
    {  // main accept() loop
        sin_size = sizeof their_addr;
        new_fd = accept(sockfd, (struct sockaddr *)&their_addr, &sin_size); //listening for a client
        if (new_fd == -1) {
            perror("accept");
            continue;
        }

        inet_ntop(their_addr.ss_family, //tells us if we got a connection
            get_in_addr((struct sockaddr *)&their_addr),
            s, sizeof s);
        printf("server: got connection from %s\n", s);

        if (!fork()) // We fork to allow the child process to complete the request
        { 
            (void) close(sockfd); // child doesn't need the listener
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
       }
       (void) close(new_fd);  // parent doesn't need this
    }

    return 0;
}

/*--------------------------------------------------------------------------------
 * wsConfig - Parsing the ws.conf file
 *--------------------------------------------------------------------------------
 */
 char *
 wsConfig()
 {
    char *line, *token, *string; 
    static char ws[BUFSIZE];
    FILE *fp = fopen("ws.conf", "r");
    int index = 0;
    int index2 = 0;
    int length;

    if (fp != NULL) 
    {
        size_t newLen = fread(ws, sizeof(char), BUFSIZE, fp); //read in the ws.conf file
        if (newLen == 0)
            fputs("Error reading file", stderr);
        fclose(fp);
    }
    string = ws;
    while((line = strsep(&string, "\n")) != NULL) //separate by newline characters
    {
        if (line[0] != '#') //if a comment, ignore, if not, then parse down
        {
            if (line[0] != '.') //This is if the start is the Connection Type
            {
                while((token = strsep(&line, " ")) != NULL) //Otherwise, we grab the specific stuff we are looking for
                	//because of the fact we know how the ws.conf file is written
                {   
                    if (index == 0 && index2 == 1)
                    {
                        port = token;
                        index2 = 0;
                    }
                    else if (index == 1 && index2 == 1)
                    { //we have quotes around the document root, so we need to remove those characters
                        DocumentRoot = token;
                        DocumentRoot++;
                        length = strlen(DocumentRoot);
                        DocumentRoot[length-1] = '\0';
                        index2 = 0;
                    }
                    if (index == 2 && index2 >= 1)
                        if (index2 == 1)
                            DocIndex = token;
                    index2++;
                }
                index2 = 0;
                index++;
            }
            else
            {
                while((token = strsep(&line, " ")) != NULL)
                {
                    if (index2 == 0)
                    {
                        ContentType = token;
                        index2 = 2;
                    }
                    else if (index2 == 1)
                    {   
                        strcat(ContentType, token);
                        index2++;
                    }
                }
                index2 = 1;
            }
        }
    }
    return port; //return the port number
 }
/*--------------------------------------------------------------------------------
 * ClientInput - ClientInput one buffer of data.  Call other functions accordingly
 *--------------------------------------------------------------------------------
 */
int
ClientInput(int fd)
{   
    static char    buf[BUFSIZE];
    int     index = 0;
    int     cc;
    int     length;
    int     results;
    char    *Method, *URI, *Version, *token, *line, *string;

    cc = read(fd, buf, BUFSIZE); //read in the client header into buf
    if (cc < 0)
    {
        return ErrorHandle(0, 500, fd, Method, URI, Version);
        fputs("Error reading file", stderr);
    }

    string = buf; //we need a pointer to parse through the buf

    while((line = strsep(&string, "\n")) != NULL) //separate by new line characters
    {
        if (index == 0) //if it is the first line, then we separate by the white space
        {
            while((token = strsep(&line, " ")) != NULL)
            {
                if (index == 0)
                    Method = token;
                else if (index == 1)
                    URI = token;
                else
                    Version = token;
                index++;
            }
            index = 0;
        }
        else //otherwise, we are just looking for the connection
            {
            	if(strstr(line, "keep-alive"))
			    	Connection = "keep-alive";
			    else if(strstr(line, "Keep-alive"))
			    	Connection = "keep-alive";
			    else if(strstr(line, "keep-Alive"))
			    	Connection = "keep-alive";
			    else if(strstr(line, "closed"))
			    	Connection = "close";
			    else if(strstr(line, "Closed"))
			    	Connection = "close";
			    else if(strstr(line, "close"))
			    	Connection = "close";
			    else if(strstr(line, "Close"))
			    	Connection = "close";
                else
                    Connection = "close";
            }
        index++;
    }

    if (Version != NULL) //Need to remove the return and newline characters to do a comparison for 400 error
    {   
        length = strlen(Version); 
        if (Version[length-1] == ' ' || Version[length-1] == '\n' || Version[length-1] == '\r')
            Version[length-1] = '\0';
    }
    //Checking my 400 errors
    if (strcmp(Method,"GET") != 0)
        results = ErrorHandle(1, 400, fd, Method, URI, Version);
    else if (strstr(URI, "//") || strstr(URI, "[") || strstr(URI, "]"))
        results = ErrorHandle(2, 400, fd, Method, URI, Version);
    else if (strcmp(Version, "HTTP/1.0\0") != 0 && strcmp(Version, "HTTP/1.1\0") != 0)
        results = ErrorHandle(3, 400, fd, Method, URI, Version);
    else
        results = GET(fd, Method, URI, Version); //If no 400 error, move on to the request

    return results;
}

/*--------------------------------------------------------------------------------
 * GET - GET function call, retreive the and return requested information
 *--------------------------------------------------------------------------------
 */
int
GET(int fd, char* Method, char* URI, char* Version)
{
    char *root = calloc(strlen(DocumentRoot)+strlen(URI)+10, 1);
    char *OK, *ext, *cTime; 
    static char Date[60] = "Date: ";
    static char ContType[60];
    static char len[BUFSIZE];
    static char Connect[60];
    long long length;
    long long cc;
    time_t currentTime;
    FILE *fp;
    wsConfig();
    memcpy(root, DocumentRoot, strlen(DocumentRoot));

    if (strcmp(URI, "/") == 0) //If no specific URL is passed in, return the index.html
    {   //We have to build the header first
        //Response code
        //OK = "HTTP/1.1 200 OK\r\n";

        //Date
        currentTime = time(NULL);
        cTime = ctime(&currentTime);
        memcpy(Date + (strlen(Date)), cTime, strlen(cTime));
        
        //Content-Type
        memcpy(root + strlen(root), "/index.html", 11);
        memcpy(ContType, "Content-Type: text/html\r\n", 25);

        fp = fopen(root, "rb");
        //Content-Length
        fseek(fp, 0, SEEK_END);
        length = ftell(fp); //gives us the length of the file being returned
        rewind(fp);

        struct stat st;
        stat(root, &st);

        char* html = calloc(st.st_size,1);      
        size_t bytes = fread(html, 1, st.st_size, fp);
        char *ContLength = calloc(bytes,1);
        snprintf(ContLength, sizeof ContLength, "%zu", st.st_size);
        fclose(fp);

        memcpy(len, "Content-Length: ", 16);
        memcpy(len + strlen(len), ContLength, strlen(ContLength));
        len[strlen(len)] = '\r';
        len[strlen(len)] = '\n';
        
        //Connection
        memcpy(Connect, "Connection: ", 12);
        cc = strlen(Connection);
        memcpy(Connect + strlen(Connect), Connection, strlen(Connection));

        //Sending the build header
        //int HeaderSize = strlen(OK) + strlen(Date) + strlen(ContType) + strlen(len) + strlen(Connect) + 4;
        int HeaderSize = strlen(Date) + strlen(ContType) + strlen(len) + strlen(Connect) + 4;
        char* header = calloc(HeaderSize,1);
        //memcpy(header, OK, strlen(OK));
        memcpy(header+strlen(header), Date, strlen(Date));
        memcpy(header+strlen(header), ContType, strlen(ContType));
        memcpy(header+strlen(header), len, strlen(len));
        memcpy(header+strlen(header), Connect, strlen(Connect));
        header[strlen(header)] = '\r';
        header[strlen(header)] = '\n';
        header[strlen(header)] = '\r';
        header[strlen(header)] = '\n';

        send(fd, header, HeaderSize, 0); //Send the header first
        cc = send(fd, html, length, 0); //Then send the content

        free(html);
        html = NULL;
        free(header);
        header = NULL;
        free(ContLength);
        ContLength = NULL;
    }
    else //This is grabbing the specific URL content
    {
        //Checking the content-type
        ext = strrchr(URI, '.');
        if (ext == NULL)
            return ErrorHandle(2, 400, fd, Method, URI, Version);
        if (strstr(ContentType, ext)) //Have to verify that the ext is actually implemented
        {
            if (strcmp(ext, ".html") == 0)
                memcpy(ContType, "Content-Type: text/html\r\n", 25);
            else if (strcmp(ext, ".htm") == 0)
                memcpy(ContType, "Content-Type: text/html\r\n", 25);
            else if (strcmp(ext, ".txt") == 0)
                memcpy(ContType, "Content-Type: text/plain\r\n", 26);
            else if (strcmp(ext, ".png") == 0)
                memcpy(ContType, "Content-Type: image/png\r\n", 25);
            else if (strcmp(ext, ".gif") == 0)
                memcpy(ContType, "Content-Type: image/gif\r\n", 25);
            else if (strcmp(ext, ".jpg") == 0)
                memcpy(ContType, "Content-Type: image/jpg\r\n", 25);
            else if (strcmp(ext, ".css") == 0)
                memcpy(ContType, "Content-Type: text/css\r\n", 24);
            else if (strcmp(ext, ".js") == 0)
                memcpy(ContType, "Content-Type: text/javascript\r\n", 31);
            else
                memcpy(ContType, "Content-Type: image/x-icon\r\n", 28);
        }
        else //If not, send a 501 error
            return ErrorHandle(0, 501, fd, Method, URI, Version);
        strcat(root, URI);

        fp = fopen(root, "rb"); //If we can't open the file, then it doesn't exist
        if (fp == NULL)
            return ErrorHandle(0, 404, fd, Method, URI, Version);
        fseek(fp, 0, SEEK_END);
        length = ftell(fp);

        rewind(fp);
        struct stat st;
        stat(root, &st);

        char* html = calloc(st.st_size,1);
        size_t bytes = fread(html, 1, st.st_size, fp);

        char *ContLength = calloc(bytes,1);    
        snprintf(ContLength, sizeof ContLength, "%zu", st.st_size);

        fclose(fp);

        //Response code
        OK = "HTTP/1.1 200 OK\r\n";

        //Date
        currentTime = time(NULL);
        cTime = ctime(&currentTime);
        memcpy(Date + (strlen(Date)), cTime, strlen(cTime));
        
        //Content-Length
        memcpy(len, "Content-Length: ", 16);
        memcpy(len + strlen(len), ContLength, strlen(ContLength));
        len[strlen(len)] = '\r';
        len[strlen(len)] = '\n';
        
        //Connection
        memcpy(Connect, "Connection: ", 12);
        memcpy(Connect + strlen(Connect), Connection, strlen(Connection));

        cc = strlen(Connection);

        //Sending the header
        int HeaderSize = strlen(OK) + strlen(Date) + strlen(ContType) + strlen(len) + strlen(Connect) + 4;
        char * header = calloc(HeaderSize,1);
        memcpy(header, OK, strlen(OK));
        memcpy(header+strlen(header), Date, strlen(Date));
        memcpy(header+strlen(header), ContType, strlen(ContType));
        memcpy(header+strlen(header), len, strlen(len));
        memcpy(header+strlen(header), Connect, strlen(Connect));
        header[strlen(header)] = '\r';
        header[strlen(header)] = '\n';
        header[strlen(header)] = '\r';
        header[strlen(header)] = '\n';

        //printf("Sending the header\n%s", header);
        send(fd, header, HeaderSize, 0);

        cc = send(fd, html, length, 0);
        free(html);
        html = NULL;
        free(header);
        header = NULL;
        free(ContLength);
        ContLength = NULL;
    }

    free(root);
    root = NULL;
    return cc;

}

/*--------------------------------------------------------------------------------
 * Handling error codes - 3xx,4xx, etc
 *--------------------------------------------------------------------------------
 */
int
ErrorHandle(int d, int code, int fd, char* Method, char* URI, char* Version)
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