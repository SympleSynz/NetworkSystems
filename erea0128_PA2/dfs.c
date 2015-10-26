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
void GET(char Root[50], int fd);
void PUT(char Root[50], int fd);
void LIST(char Root[50], int fd);

int main(int argc, char* argv[])
{
	struct conf authorized;
	authorized = parseConfig();

	int dfs, new_fd, rv, bytes, AuthBool, pollRtn;  
    struct addrinfo hints, *servinfo, *p;
    struct sockaddr_storage their_addr; 
    socklen_t sin_size;
    struct sigaction sa;
    //struct stat sb;
    int yes = 1;
    char s[INET6_ADDRSTRLEN];
    char Root[50];
    char UserBuf[BUFSIZE];
    char PassBuf[BUFSIZE];
    char RequestType[4];

    if(argc < 3)
    {
    	fprintf(stderr, "\nIncorrect number of arguments\n");
        exit(1);
    }
    strcpy(authorized.Root, ".");
    strcat(authorized.Root, argv[1]);
    strcat(authorized.Root, "/");
    mkdir(authorized.Root, 0777);
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
// main accept() loop for dfs
    while(1) 
    {  
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
//Checking Authorization
        AuthBool = 0;
        memset(UserBuf, 0, BUFSIZE);
        memset(Root, 0, 50);
        strcpy(Root, authorized.Root);
        bytes = read(new_fd, UserBuf, BUFSIZE);
    	if (bytes < 0)
    		fputs("Error reading file", stderr);
    	//printf("%s\n", UserBuf);
    	if(strcmp(UserBuf, authorized.Username) == 0)
    	{
			send(new_fd, "Authorized User", 15, 0);
			memset(PassBuf, 0, BUFSIZE);
	        bytes = read(new_fd, PassBuf, BUFSIZE);
	        //printf("%s\n", PassBuf);
	    	if (bytes < 0)
	    		fputs("Error reading file", stderr);
	    	if(strcmp(PassBuf, authorized.Password) == 0)
	    	{
	    		strcat(Root, authorized.Username);
	    		mkdir(Root, 0777);	    		
    			send(new_fd, "Authorized Password", 19, 0);
    			AuthBool = 1;
    		}
	    	else
	    	{
    			send(new_fd, "Invalid Password", 16, 0);
    			printf("Connection %d closed\n", new_fd);
    			close(new_fd);
	    	}
	    }
    	else if(strcmp(UserBuf, authorized.Username2) == 0)
    	{
			send(new_fd, "Authorized User", 15, 0);
			memset(PassBuf, 0, BUFSIZE);
	        bytes = read(new_fd, PassBuf, BUFSIZE);
	    	if (bytes < 0)
	    		fputs("Error reading file", stderr);
	    	//printf("%s\n", PassBuf);
	    	if(strcmp(PassBuf, authorized.Password2) == 0)
	    	{
	    		strcat(Root, authorized.Username2);	
	    		mkdir(Root, 0777);    		
    			send(new_fd, "Authorized Password", 19, 0);
    			AuthBool = 1;
    		}
	    	else
	    	{
    			send(new_fd, "Invalid Password", 16, 0);
    			printf("Connection %d closed\n", new_fd);
    			close(new_fd);
	    	}
	    }
    	else
    	{
			send(new_fd, "Invalid Password", 16, 0);
			printf("Connection %d closed\n", new_fd);
			close(new_fd);
    	}

//Start listening for content
		if (!fork() && AuthBool) // We fork to allow the child process to complete the request
        { 
            (void) close(dfs); // child doesn't need the listener
            while(1)
            {   
                struct pollfd ufds[1]; //This struct is used for the poll() for timing
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
                	memset(RequestType, 0, 4);
            		read(new_fd, RequestType, 4);
                    if (strcmp(RequestType, "GET ") == 0)
                    {
                    	GET(Root, new_fd);
                    	break;
                    }
                    else if (strcmp(RequestType, "PUT ") == 0)
                    {
                    	PUT(Root, new_fd);
                    	break;
                    } 
                    else if (strcmp(RequestType, "LIST") == 0)
                    {
                     	LIST(Root, new_fd);
                     	break;   
                    }
                }
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

void GET(char Root[50], int fd)
{
	struct stat st;
	int PieceSize;
	int NameSize;
	char *FileRoot = calloc(50, 1);
	char MsgSize[15];
	char *filename, *Content;
	int bytes, Msg, folderNameSize;
	FILE *fp;
	
	bytes = read(fd, (char*)& folderNameSize, sizeof(folderNameSize));
	if (bytes < 0)
		printf("Error getting the subfolder name size\n");

	char *subfolder = calloc(folderNameSize, 1);
	bytes = read(fd, subfolder, folderNameSize);
	if (bytes < 0)
		printf("Error reading in subfolder\n");

	memcpy(FileRoot, Root, strlen(Root));
	if (folderNameSize != 1)
		memcpy(FileRoot+strlen(FileRoot), subfolder, folderNameSize);
	memcpy(FileRoot+strlen(FileRoot), "/.", 2);

	bytes = read(fd, (char*) &NameSize, sizeof(NameSize));
	if (bytes < 0)
		printf("Error reading in Filename Size\n");

	filename = calloc(NameSize, 1);
	bytes = read(fd, filename, NameSize);
	if (bytes < 0)
		printf("Error reading in file name\n");
	memcpy(FileRoot+strlen(FileRoot), filename, NameSize);
	memcpy(FileRoot+strlen(FileRoot), ".1", 2);
	fp = fopen(FileRoot, "rb");
	if (fp != NULL)
	{
		stat(FileRoot, &st);
		PieceSize = st.st_size;
		sprintf(MsgSize, "%d", PieceSize);
		Msg = strlen(MsgSize);

		send(fd, "1", 1, 0);
		send(fd, (char*)&Msg, sizeof(Msg), 0);
		send(fd, MsgSize, strlen(MsgSize), 0);
		Content = calloc(PieceSize, 1);
		fread(Content, 1, PieceSize, fp);
		send(fd, Content, strlen(Content), 0);
		fclose(fp);
		free(Content);

		FileRoot[strlen(FileRoot)-1] = '2';
		fp = fopen(FileRoot, "rb");
		if (fp != NULL)
		{
			stat(FileRoot, &st);
			PieceSize = st.st_size;
			sprintf(MsgSize, "%d", PieceSize);
			Msg = strlen(MsgSize);

			send(fd, "2", 1, 0);
			send(fd, (char*)&Msg, sizeof(Msg), 0);
			send(fd, MsgSize, strlen(MsgSize), 0);
			Content = calloc(PieceSize, 1);
			fread(Content, 1, PieceSize, fp);
			send(fd, Content, strlen(Content), 0);
			fclose(fp);
			free(Content);
		}
		else
		{
			FileRoot[strlen(FileRoot)-1] = '4';
			fp = fopen(FileRoot, "rb");
			if (fp != NULL)
			{
				stat(FileRoot, &st);
				PieceSize = st.st_size;
				sprintf(MsgSize, "%d", PieceSize);
				Msg = strlen(MsgSize);

				send(fd, "4", 1, 0);
				send(fd, (char*)&Msg, sizeof(Msg), 0);
				send(fd, MsgSize, strlen(MsgSize), 0);
				Content = calloc(PieceSize, 1);
				fread(Content, 1, PieceSize, fp);
				send(fd, Content, strlen(Content), 0);
				fclose(fp);
				free(Content);
			}
			else
				send(fd, "0", 1, 0);
		}
	}
	else
	{
		FileRoot[strlen(FileRoot)-1] = '2';
		fp = fopen(FileRoot, "rb");
		if (fp != NULL)
		{
			stat(FileRoot, &st);
			PieceSize = st.st_size;
			sprintf(MsgSize, "%d", PieceSize);
			Msg = strlen(MsgSize);

			send(fd, "2", 1, 0);
			send(fd, (char*)&Msg, sizeof(Msg), 0);
			send(fd, MsgSize, strlen(MsgSize), 0);
			Content = calloc(PieceSize, 1);
			fread(Content, 1, PieceSize, fp);
			send(fd, Content, strlen(Content), 0);
			fclose(fp);
			free(Content);

			FileRoot[strlen(FileRoot)-1] = '3';
			fp = fopen(FileRoot, "rb");
			if (fp != NULL)
			{
				stat(FileRoot, &st);
				PieceSize = st.st_size;
				sprintf(MsgSize, "%d", PieceSize);
				Msg = strlen(MsgSize);

				send(fd, "3", 1, 0);
				send(fd, (char*)&Msg, sizeof(Msg), 0);
				send(fd, MsgSize, strlen(MsgSize), 0);
				Content = calloc(PieceSize, 1);
				fread(Content, 1, PieceSize, fp);
				send(fd, Content, strlen(Content), 0);
				fclose(fp);
				free(Content);
			}
			else
				send(fd, "0", 1, 0);
		}
		else
		{
			FileRoot[strlen(FileRoot)-1] = '3';
			fp = fopen(FileRoot, "rb");
			if (fp != NULL)
			{
				stat(FileRoot, &st);
				PieceSize = st.st_size;
				sprintf(MsgSize, "%d", PieceSize);
				Msg = strlen(MsgSize);

				send(fd, "3", 1, 0);
				send(fd, (char*)&Msg, sizeof(Msg), 0);
				send(fd, MsgSize, strlen(MsgSize), 0);
				Content = calloc(PieceSize, 1);
				fread(Content, 1, PieceSize, fp);
				send(fd, Content, strlen(Content), 0);
				fclose(fp);
				free(Content);
			}
			else
				send(fd, "0", 1, 0);

			FileRoot[strlen(FileRoot)-1] = '4';
			fp = fopen(FileRoot, "rb");
			if (fp != NULL)
			{
				stat(FileRoot, &st);
				PieceSize = st.st_size;
				sprintf(MsgSize, "%d", PieceSize);
				Msg = strlen(MsgSize);

				send(fd, "4", 1, 0);
				send(fd, (char*)&Msg, sizeof(Msg), 0);
				send(fd, MsgSize, strlen(MsgSize), 0);
				Content = calloc(PieceSize, 1);
				fread(Content, 1, PieceSize, fp);
				send(fd, Content, strlen(Content), 0);
				fclose(fp);
				free(Content);
			}
			else
				send(fd, "0", 1, 0);
		}

	}
	
	free(filename);
	free(FileRoot);
	free(subfolder);
	
}

void PUT(char Root[50], int fd)
{
	int PieceSize;
	int NameSize;
	char *FileRoot = calloc(50, 1);
	int bytes, folderNameSize;
	
	memcpy(FileRoot, Root, strlen(Root));
	bytes = read(fd, (char*)& folderNameSize, sizeof(folderNameSize));
	if (bytes < 0)
		printf("Error getting the subfolder name size\n");

	char *subfolder = calloc(folderNameSize, 1);
	bytes = read(fd, subfolder, folderNameSize);
	if (bytes < 0)
		printf("Error reading in subfolder\n");

	if (folderNameSize != 1)
	{
		memcpy(FileRoot+strlen(FileRoot), subfolder, folderNameSize);
		mkdir(FileRoot, 0777);
	}

	bytes = read(fd, (char*) &PieceSize, sizeof(PieceSize));
	if (bytes < 0)
		printf("Error reading in Piece Size\n");

	char *MsgSize = calloc(PieceSize, 1);
	bytes = read(fd, MsgSize, PieceSize);
	if (bytes < 0)
		printf("Error reading in Msg Size\n");
	
	int ContentSize = atoi(MsgSize);
	bytes = read(fd, (char*) &NameSize, sizeof(NameSize));
	if (bytes < 0)
		printf("Error reading in Filename Size\n");

	char *filename = calloc(NameSize, 1);
	bytes = read(fd, filename, NameSize);
	if (bytes < 0)
		printf("Error reading in file name\n");

	char *ContentPiece = calloc(ContentSize, 1);
	bytes = read(fd, ContentPiece, ContentSize);
	if (bytes < 0)
		printf("Error reading in file\n");

	memcpy(FileRoot+strlen(FileRoot), filename, strlen(filename));
	FILE *fp = fopen(FileRoot, "w");
	if (fp == NULL)
		printf("Error opening %s\n", filename);
	fprintf(fp, "%s", ContentPiece);
	fclose(fp);
	free(ContentPiece);
	free(filename);
	free(MsgSize);
	free(FileRoot);

	//Receiving the 2nd piece
	FileRoot = calloc(50, 1);
	memcpy(FileRoot, Root, strlen(Root));
	if (folderNameSize != 1)
		memcpy(FileRoot+strlen(FileRoot), subfolder, folderNameSize);

	bytes = read(fd, (char*) &PieceSize, sizeof(PieceSize));
	if (bytes < 0)
		printf("Error reading in Piece Size\n");

	MsgSize = calloc(PieceSize, 1);
	bytes = read(fd, MsgSize, PieceSize);
	if (bytes < 0)
		printf("Error reading in Msg Size\n");

	ContentSize = atoi(MsgSize);
	printf("ContentSize %d\n", ContentSize);
	bytes = read(fd, (char*) &NameSize, sizeof(NameSize));
	if (bytes < 0)
		printf("Error reading in Filename Size\n");

	filename = calloc(NameSize, 1);
	bytes = read(fd, filename, NameSize);
	if (bytes < 0)
		printf("Error reading in file name\n");

	ContentPiece = calloc(ContentSize, 1);
	bytes = read(fd, ContentPiece, ContentSize);
	if (bytes < 0)
		printf("Error reading in file\n");

	memcpy(FileRoot+strlen(FileRoot), filename, strlen(filename));
	fp = fopen(FileRoot, "w");
	
	if (fp == NULL)
		printf("Error opening %s\n", filename);
	fprintf(fp, "%s", ContentPiece);
	fclose(fp);
	free(ContentPiece);
	free(filename);
	free(MsgSize);
	free(FileRoot);

}

void LIST(char Root[50], int fd)
{
	struct dirent *dir;
	DIR *directory;
	char *FileRoot = calloc(50, 1);
	int filenameSize, count, folderNameSize, bytes, cont;
	char filename[100];
	memcpy(FileRoot, Root, strlen(Root));

	bytes = read(fd, (char*)& folderNameSize, sizeof(folderNameSize));
	if (bytes < 0)
		printf("Error getting the subfolder name size\n");

	char *subfolder = calloc(folderNameSize, 1);
	bytes = read(fd, subfolder, folderNameSize);
	if (bytes < 0)
		printf("Error reading in subfolder\n");
	if (folderNameSize != 1)
		memcpy(FileRoot+strlen(FileRoot), subfolder, folderNameSize);
	if ((directory = opendir(FileRoot)) == NULL)
	{
		cont = 0;
		send(fd, (char*)&cont, sizeof(cont), 0);
		printf("Error opening directory to count\n");
		return;
	}
	cont = 1;
	send(fd, (char*)&cont, sizeof(cont), 0);

	count = 0;
	while ((dir = readdir(directory)) != NULL)
	{
		if (dir->d_type == DT_REG)
			count++;
	}
	closedir(directory);
	send(fd, (char*)&count, sizeof(count), 0);

	if ((directory = opendir(FileRoot)) == NULL)
		printf("Error opening directory\n");

	while ((dir = readdir(directory)) != NULL)
	{
		if (dir->d_type == DT_REG)
		{
			sprintf (filename, "%s", dir->d_name);
			filenameSize = strlen(filename);
			printf("%s\n", filename);
			send(fd, (char*)&filenameSize, sizeof(filenameSize), 0);
			send(fd, filename, filenameSize, 0);
		}
	}
	closedir(directory);
	free(FileRoot);
	free(subfolder);
}
