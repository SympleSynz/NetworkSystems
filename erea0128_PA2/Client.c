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

#define BUFSIZE	4096
#ifndef INADDR_NONE
#define INADDR_NONE	0xffffffff
#endif  /* INADDR_NONE */

struct conf
{
	char *DFS1;
	char *DFS2;
	char *DFS3;
	char *DFS4;
	char *Username;
	char *Password;
};
struct conf parseConfig();
int hashDist(const char *filename);
int	errexit(const char *format, ...);
int	connectsock(const char *host, const char *portnum);
void GET(const char *auth, const char *filename);
void PUT(const char *auth, const char *filename);
void LIST(const char *auth);

int main(int argc, char* argv[])
{
	struct conf dfcConfig;
	char *buf = calloc(BUFSIZE, 1);
	char *string, *line, *token, *request, *filename, *auth;
	int	dfs1, dfs2, dfs3, dfs4;
	char *host = "localhost";
	int count = 0;
	
	dfcConfig = parseConfig();
	auth = dfcConfig.Username;
	strcat(auth, " ");
	strcat(auth, dfcConfig.Password);

	dfs1 = connectsock(host, dfcConfig.DFS1);
	dfs2 = connectsock(host, dfcConfig.DFS2);
	dfs3 = connectsock(host, dfcConfig.DFS3);
	dfs4 = connectsock(host, dfcConfig.DFS4);

	while (fgets(buf, BUFSIZE, stdin)) 
	{
		buf[strlen(buf)-1] = '\0';	/* make sure line null-terminated	*/
		string = buf;
		while ((line = strsep(&string, "\0")) != NULL)
		{
			while((token = strsep(&line, " ")) != NULL)
			{
				if (count == 0)
					request = token;
				else
					filename = token;
				count++;
			}
			count = 0;
		}

		if (strcmp(request, "GET") == 0)
			GET(auth, filename);
		else if (strcmp(request, "PUT") == 0)
		{	
			if(strcmp(filename, "\0") != 0)
				PUT(auth, filename);
			else
				printf("No filename given\n");
		}
		else if (strcmp(request, "LIST") == 0)
			LIST(auth);
		else
			printf("Invalid request\n");
		filename = "\0";
		//outchars = strlen(buf);
		//(void) write(s, buf, outchars);

		/* read it back */
		/*for (inchars = 0; inchars < outchars; inchars+=n ) {
			n = read(s, &buf[inchars], outchars - inchars);
			if (n < 0)
				errexit("socket read failed: %s\n",
					strerror(errno));
		}*/
		//fputs(buf, stdout);
	}
	return 0;
}

struct conf parseConfig()
{
	char *line;
	char *source = calloc(BUFSIZE, 1);
	int count = 0;
	struct conf configFile;
	FILE *dfcConfig = fopen("dfc.conf", "r"); //open the config file

	if (dfcConfig != NULL)
    {
        size_t newLen = fread(source, 1, BUFSIZE, dfcConfig);
        if (newLen == 0)
            fputs("Error reading file", stderr);
        fclose(dfcConfig);
    }

	while ((line = strsep(&source, "\n")) != NULL)
	{
		if (count == 0)
		{	
			configFile.DFS1 = strrchr(line, ':');
			configFile.DFS1++;
		}
		else if (count == 1)
		{	
			configFile.DFS2 = strrchr(line, ':');
			configFile.DFS2++;
		}
		else if (count == 2)
		{	
			configFile.DFS3 = strrchr(line, ':');
			configFile.DFS3++;
		}
		else if (count == 3)
		{	
			configFile.DFS4 = strrchr(line, ':');
			configFile.DFS4++;
		}
		else if (count == 4)
		{	
			configFile.Username = strrchr(line, ':');
			configFile.Username++;
		}
		else
		{	
			configFile.Password = strrchr(line, ':');
			configFile.Password++;
		}
		count++;
	}
	free(source);
	return configFile;
}

int hashDist(const char *filename)
{
	static char fileContent[BUFSIZE];
	unsigned char hash[MD5_DIGEST_LENGTH];
	FILE *fp = fopen(filename, "rb");
	MD5_CTX mdContext;
	int bytes;
	//int i = 0;
	int mdhash = 0; 

	if (fp != NULL)
	{
		MD5_Init (&mdContext);
	    while ((bytes = fread (fileContent, 1, 1024, fp)) != 0)
	        MD5_Update (&mdContext, fileContent, bytes);
	    
	    MD5_Final (hash,&mdContext);
	    /*while(i < MD5_DIGEST_LENGTH) 
		{
			printf("%02x", hash[i]);
			i++;
		}
	    printf (" %s\n", filename);*/
	    printf("%02x = %u\n", hash[MD5_DIGEST_LENGTH - 1], hash[MD5_DIGEST_LENGTH - 1]);
	    mdhash = hash[MD5_DIGEST_LENGTH - 1];
	    printf("%d\n", mdhash);
	    fclose (fp);

	    return mdhash;
	}
	return -1;
}

void GET(const char *auth, const char *filename)
{
	printf("%s\n", auth);
	printf("%s\n", filename);
}

void PUT(const char *auth, const char *filename)
{
	int dist = hashDist(filename);

	if(dist == -1)
	{
		printf("%s does not exist\n", filename);
		return;
	}

}

void LIST(const char *auth)
{

}

/*------------------------------------------------------------------------
 * errexit - print an error message and exit
 *------------------------------------------------------------------------
 */
int
errexit(const char *format, ...)
{
        va_list args;

        va_start(args, format);
        vfprintf(stderr, format, args);
        va_end(args);
        exit(1);
}

int
connectsock(const char *host, const char *portnum)
/*-----------------------------------------------------------------------
 * Arguments:
 *      host      - name of host to which connection is desired
 *      portnum   - server port number
 *-----------------------------------------------------------------------
 */
{
        struct hostent  *phe;   /* pointer to host information entry    */
        struct sockaddr_in sin; /* an Internet endpoint address         */
        int s;              /* socket descriptor                    */


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