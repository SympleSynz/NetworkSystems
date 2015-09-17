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
char    *DocumentRoot, *port, *DocIndex, *ContentType, *Connection;

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

    if ((rv = getaddrinfo(NULL, portnum, &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return 1;
    }

    // loop through all the results and bind to the first we can
    for(p = servinfo; p != NULL; p = p->ai_next) {
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

    if (p == NULL)  {
        fprintf(stderr, "server: failed to bind\n");
        exit(1);
    }

    if (listen(sockfd, QLEN) == -1) {
        perror("listen");
        exit(1);
    }

    sa.sa_handler = sigchld_handler; // reap all dead processes
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    if (sigaction(SIGCHLD, &sa, NULL) == -1) {
        perror("sigaction");
        exit(1);
    }

    printf("server: waiting for connections...\n");

    while(1) {  // main accept() loop
        sin_size = sizeof their_addr;
        new_fd = accept(sockfd, (struct sockaddr *)&their_addr, &sin_size);
        if (new_fd == -1) {
            perror("accept");
            continue;
        }

        inet_ntop(their_addr.ss_family,
            get_in_addr((struct sockaddr *)&their_addr),
            s, sizeof s);
        printf("server: got connection from %s\n", s);

        if (!fork()) // this is the child process
        { 
            close(sockfd); // child doesn't need the listener
            while(1)
            {   
                struct pollfd ufds[1];
                ufds[0].fd = new_fd;
                ufds[0].events = POLLIN;
                pollRtn = poll(ufds, 1, 10000);
                if (pollRtn == -1)
                {
                    printf("Error when creating poll for child fork\n");
                    ErrorHandle(0, 500, new_fd, " ", " ", " ");
                }
                else if (pollRtn == 0)
                {
                    printf("Timed out\n");
                    break;
                }
                else
                {
                    if (ClientInput(new_fd) == 0) 
                        perror("send");
                    if (strcmp(Connection, "close\r\n") == 0)
                        break;
                }
            }
            (void) close(new_fd);
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
    char *line, *string, *token; 
    char *ws = calloc(BUFSIZE, 1);
    FILE *fp = fopen("ws.conf", "r");
    int index = 0;
    int index2 = 0;
    int length;

    if (fp != NULL) 
    {
        size_t newLen = fread(ws, sizeof(char), BUFSIZE, fp);
        if (newLen == 0)
            fputs("Error reading file", stderr);
        else
            ws[++newLen] = '\0'; /* Just to be safe. */
        fclose(fp);
    }
    
    string = ws;
    while((line = strsep(&string, "\n")) != NULL)
    {
        if (line[0] != '#')
        {
            if (line[0] != '.')
            {
                while((token = strsep(&line, " ")) != NULL)
                {   
                    if (index == 0 && index2 == 1)
                    {
                        port = token;
                        index2 = 0;
                    }
                    else if (index == 1 && index2 == 1)
                    {
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
    free(ws);
    return port;
 }
/*--------------------------------------------------------------------------------
 * ClientInput - ClientInput one buffer of data.  Call other functions accordingly
 *--------------------------------------------------------------------------------
 */
int
ClientInput(int fd)
{   
    char    buf[BUFSIZE];
    //char    bufCpy[1];
    int     index = 0;
    int     cc;
    //int     i;
    //int     bufLength;
    int     length;
    int     results;
    //int     request;
    //bool    reading = true;
    //bool    sending = true;
    char    *Method, *URI, *Version, *string, *token, *line;//, *MultReq;


    /*while(sending == true)
    {
        bufLength = 0;
        while(reading == true)
        { 
            printf("inner while\n");
            cc = read(fd, bufCpy, 1);
            printf("How much got read in? %d\n", cc);
            printf("%d\n", bufLength);
            if (cc != 0)
            {
                buf[bufLength] = bufCpy[0];
                bufLength++;
            }
            if (cc < 0)
            {
                ErrorHandle(0, 500, fd, Method, URI, Version);
                fputs("Error reading file", stderr);
                return 0;
            }
            if (bufLength == 0)
                {printf("why aren't you going in here?\n");
                reading = false;
            }
            if (buf[bufLength-1] == '\r')
                request++;
            else if (buf[bufLength-1] == '\n')
                request = request + 2;
            else
                request = 0;
            if ((request >= 4) || cc == 0 || buf[bufLength - 1] == '\0')
                reading = false;
        }
        printf("%s\n", buf);
        if (bufLength == 0)
            sending = false;*/
        cc = read(fd, buf, BUFSIZE);
        string = buf;
        while((line = strsep(&string, "\n")) != NULL)
        {
            if (index == 0)
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
            else
                while((token = strsep(&line, " ")) != NULL)
                {
                    if(token[0] == 'k')
                        Connection = token;
                }
            index++;
        }

        if (Version != NULL);
        {   
            length = strlen(Version); 
            if (Version[length-1] == ' ' || Version[length-1] == '\n' || Version[length-1] == '\r')
                Version[length-1] = '\0';
        }
        
        if (strcmp(Method,"GET") != 0)
            results = ErrorHandle(1, 400, fd, Method, URI, Version);
        else if (strstr(URI, "//"))
            results = ErrorHandle(2, 400, fd, Method, URI, Version);
        else if (strcmp(Version, "HTTP/1.0\0") != 0 && strcmp(Version, "HTTP/1.1\0") != 0)
            results = ErrorHandle(3, 400, fd, Method, URI, Version);
        else
            results = GET(fd, Method, URI, Version);
        //free(buf);
        //reading = true;
    //}
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
    char Date[60] = "Date: ";
    char *ContType = calloc(60,1);
    char *len = calloc(BUFSIZE + 60,1);
    char *Connect = calloc(60,1);
    char *line = calloc(1024,1);
    long long length;
    long long cc;
    time_t currentTime;
    FILE *fp;
    wsConfig();
    memcpy(root, DocumentRoot, strlen(DocumentRoot));

    if (strcmp(URI, "/") == 0)
    {   
        //Response code
        OK = "HTTP/1.1 200 OK\r\n";

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
        length = ftell(fp);
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

        if (Connection != NULL)
            memcpy(Connect + strlen(Connect), Connection, strlen(Connection)-1);
        else
            memcpy(Connect + strlen(Connect), "closed", 6);

        //Sending the build header
        int HeaderSize = strlen(OK) + strlen(Date) + strlen(ContType) + strlen(len) + strlen(Connect) + 4;
        char* header = calloc(HeaderSize,1);
        memcpy(header, OK, strlen(OK));
        memcpy(header+strlen(header), Date, strlen(Date));
        memcpy(header+strlen(header), ContType, strlen(ContType));
        memcpy(header+strlen(header), len, strlen(len));
        memcpy(header+strlen(header), Connect, strlen(Connect));
        header[strlen(header)] = '\r';
        header[strlen(header)] = '\n';
        header[strlen(header)] = '\r';
        header[strlen(header)] = '\n';

        printf("Sending the header\n%s", header);
        send(fd, header, HeaderSize, 0);
        cc = send(fd, html, length, 0);
        free(html);
        free(header);
        free(ContLength);
    }
    else
    {
        //Checking the content-type
        ext = strrchr(URI, '.');
        if (strstr(ContentType, ext))
        {
            if (strcmp(ext, ".html") == 0)
                (char*)memcpy(ContType, "Content-Type: text/html\r\n", 25);
            else if (strcmp(ext, ".htm") == 0)
                (char*)memcpy(ContType, "Content-Type: text/html\r\n", 25);
            else if (strcmp(ext, ".txt") == 0)
                (char*)memcpy(ContType, "Content-Type: text/plain\r\n", 26);
            else if (strcmp(ext, ".png") == 0)
                (char*)memcpy(ContType, "Content-Type: image/png\r\n", 25);
            else if (strcmp(ext, ".gif") == 0)
                (char*)memcpy(ContType, "Content-Type: image/gif\r\n", 25);
            else if (strcmp(ext, ".jpg") == 0)
                (char*)memcpy(ContType, "Content-Type: image/jpg\r\n", 25);
            else if (strcmp(ext, ".css") == 0)
                (char*)memcpy(ContType, "Content-Type: text/css\r\n", 24);
            else if (strcmp(ext, ".js") == 0)
                (char*)memcpy(ContType, "Content-Type: text/javascript\r\n", 31);
            else
                (char*)memcpy(ContType, "Content-Type: image/x-icon\r\n", 28);
        }
        else
            return ErrorHandle(0, 501, fd, Method, URI, Version);
        strcat(root, URI);

        /*if (fopen(path, "rb") == NULL)
            return ErrorHandle(2, 400, fd, Method, URI, Version);*/
        fp = fopen(root, "rb");
        if (fp == NULL)
            return ErrorHandle(0, 404, fd, Method, URI, Version);
        fseek(fp, 0, SEEK_END);
        length = ftell(fp);
        /*if (length == 0)
            return ErrorHandle(0, 404, fd, Method, URI, Version);*/
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
        (char*)memcpy(Date + (strlen(Date)), cTime, strlen(cTime));
        
        //Content-Length
        (char*)memcpy(len, "Content-Length: ", 16);
        (char*)memcpy(len + strlen(len), ContLength, strlen(ContLength));
        len[strlen(len)] = '\r';
        len[strlen(len)] = '\n';
        
        //Connection
        (char*)memcpy(Connect, "Connection: ", 12);
        if (Connection != NULL)
            (char*)memcpy(Connect + strlen(Connect), Connection, strlen(Connection)-1);
        else
            (char*)memcpy(Connect + strlen(Connect), "closed", 6);

        cc = strlen(Connection);

        //Sending the header
        int HeaderSize = strlen(OK) + strlen(Date) + strlen(ContType) + strlen(len) + strlen(Connect) + 4;
        char * header = calloc(HeaderSize,1);
        (char*)memcpy(header, OK, strlen(OK));
        (char*)memcpy(header+strlen(header), Date, strlen(Date));
        (char*)memcpy(header+strlen(header), ContType, strlen(ContType));
        (char*)memcpy(header+strlen(header), len, strlen(len));
        (char*)memcpy(header+strlen(header), Connect, strlen(Connect));
        header[strlen(header)] = '\r';
        header[strlen(header)] = '\n';
        header[strlen(header)] = '\r';
        header[strlen(header)] = '\n';

        printf("Sending the header\n%s", header);
        send(fd, header, HeaderSize, 0);

        cc = send(fd, html, length, 0);

        free(html);
        free(header);
        free(ContLength);
    }
    free(root);
    free(ContType);
    free(len);
    free(Connect);
    free(line);
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

    error[strlen(error)] = '\r';
    error[strlen(error)] = '\n';
    error[strlen(error)] = '\r';
    error[strlen(error)] = '\n';
    send(fd, error, strlen(error), 0);
    printf("%s\n", error);
    return 0;
}