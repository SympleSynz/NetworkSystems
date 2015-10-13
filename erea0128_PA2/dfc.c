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
struct conf parseConfig(char *filename);
int hashDist(const char *filename);
int	errexit(const char *format, ...);
int	connectsock(const char *host, const char *portnum);
void GET(struct conf dfcConfig, const char *filename, const int dfs1, const int dfs2, const int dfs3, const int dfs4);
void PUT(struct conf dfcConfig, const char *filename, const int dfs1, const int dfs2, const int dfs3, const int dfs4);
void LIST(struct conf dfcConfig, const int dfs1, const int dfs2, const int dfs3, const int dfs4);

int main(int argc, char* argv[])
{
	struct conf dfcConfig;
	char *buf = calloc(BUFSIZE, 1);
	char *AuthBuf = calloc(BUFSIZE, 1);
	char *string, *line, *token, *request;
	int	dfs1, dfs2, dfs3, dfs4, sentAuth;
	char *filename = "\0";
	char *host = "localhost";
	int count = 0;

	dfcConfig = parseConfig(argv[1]);
	/*dfs1 = connectsock(host, dfcConfig.DFS1);
	dfs2 = connectsock(host, dfcConfig.DFS2);
	dfs3 = connectsock(host, dfcConfig.DFS3);
	dfs4 = connectsock(host, dfcConfig.DFS4);*/

	/*sentAuth = send(dfs1, dfcConfig.Username, strlen(dfcConfig.Username), 0);
	if (sentAuth == 0)
		printf("Failure to send Username authorization to Server 1\n");
	
	sentAuth = read(dfs1, AuthBuf, BUFSIZE);
	printf("%s\n", AuthBuf);

	if(!strstr(AuthBuf, "Invalid"))
	{
		sentAuth = send(dfs1, dfcConfig.Password, strlen(dfcConfig.Password), 0);
		if (sentAuth == 0)
			printf("Failure to send Password authorization to Server 1\n");
		
		sentAuth = read(dfs1, AuthBuf, BUFSIZE);
		printf("%s\n", AuthBuf);
	}*/
	
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
		{
			if(strcmp(filename, "\0") != 0)
				GET(dfcConfig, filename, dfs1, dfs2, dfs3, dfs4);
			else
				printf("No filename given\n");	
		}
		else if (strcmp(request, "PUT") == 0)
		{	
			if(strcmp(filename, "\0") != 0)
			{
				dfs1 = connectsock(host, dfcConfig.DFS1);
				PUT(dfcConfig, filename, dfs1, dfs2, dfs3, dfs4);
			}
			else
				printf("No filename given\n");
		}
		else if (strcmp(request, "LIST") == 0)
			LIST(dfcConfig, dfs1, dfs2, dfs3, dfs4);
		else
			printf("Invalid request\n");
		filename = "\0";
	}
	return 0;
}

struct conf parseConfig(char* filename)
{
	char *line;
	char *source = calloc(BUFSIZE, 1);
	int count = 0;
	struct conf configFile;
	FILE *dfcConfig = fopen(filename, "r"); //open the config file

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
	    //printf("%02x = %u\n", hash[MD5_DIGEST_LENGTH - 1], hash[MD5_DIGEST_LENGTH - 1]);
	    mdhash = hash[MD5_DIGEST_LENGTH - 1] % 4;
	    //printf("%d\n", mdhash);
	    fclose (fp);

	    return mdhash;
	}
	return -1;
}

void GET(struct conf dfcConfig, const char *filename, const int dfs1, const int dfs2, const int dfs3, const int dfs4)
{/*
	//char *line, *token;
	char *header = calloc(BUFSIZE, 1);
	char *S1Content1 = calloc(BUFSIZE, 1);
	char *S1Content2 = calloc(BUFSIZE, 1);
	char *S1Content3 = calloc(BUFSIZE, 1);
	char *S1Content4 = calloc(BUFSIZE, 1);
	char *S2Content1 = calloc(BUFSIZE, 1);
	char *S2Content2 = calloc(BUFSIZE, 1);
	char *S2Content3 = calloc(BUFSIZE, 1);
	char *S2Content4 = calloc(BUFSIZE, 1);
	char *S3Content1 = calloc(BUFSIZE, 1);
	char *S3Content2 = calloc(BUFSIZE, 1);
	char *S3Content3 = calloc(BUFSIZE, 1);
	char *S3Content4 = calloc(BUFSIZE, 1);
	char *S4Content1 = calloc(BUFSIZE, 1);
	char *S4Content2 = calloc(BUFSIZE, 1);
	char *S4Content3 = calloc(BUFSIZE, 1);
	char *S4Content4 = calloc(BUFSIZE, 1);
	char *AuthBuf = calloc(BUFSIZE, 1);
	char *TrackParts;
	char *FileContent;
	char *getRequest = "GET ";
	char *Root = "./ClientDir/";
	FILE *fp;
	int sentAuth, recBytes;
	int fail = 0;

	strcat(Root, filename);
	strcat(getRequest, filename);

/*----------------------Server 1--------------------------------
	sentAuth = send(dfs1, auth, strlen(auth), 0);
	
	if (sentAuth == 0)
		printf("Failure to send authorization to Server 1\n");
	
	sentAuth = read(dfs1, AuthBuf, BUFSIZE);
	printf("%s\n", AuthBuf);
	if(!strstr(AuthBuf, "Invalid"))
	{
		send(dfs1, getRequest, strlen(getRequest), 0);
		recBytes = read(dfs1, header, BUFSIZE);
		if (recBytes == 0)
			printf("Failure to read header from Server 1\n");
		else if (strstr(header, "1") || strstr(header, "2") || strstr(header, "3") || strstr(header, "4"))
		{
			printf("%s\n", header);
			fail++;
		}
		else
		{
			if (strstr(header, "1") && strstr(header, "2"))
			{
				TrackParts = "S1.1 S1.2";
				recBytes = read(dfs1, S1Content1, BUFSIZE);
				if (recBytes == 0)
					printf("Failure to read %s.1 from Server 1\n", filename);
				recBytes = read(dfs1, S1Content2, BUFSIZE);
				if (recBytes == 0)
					printf("Failure to read %s.2 from Server 1\n", filename);
			}
			else if (strstr(header, "1") && strstr(header, "4"))
			{
				TrackParts = "S1.1 S1.4";
				recBytes = read(dfs1, S1Content1, BUFSIZE);
				if (recBytes == 0)
					printf("Failure to read %s.1 from Server 1\n", filename);
				recBytes = read(dfs1, S1Content4, BUFSIZE);
				if (recBytes == 0)
					printf("Failure to read %s.4 from Server 1\n", filename);
			}
			else if (strstr(header, "2") && strstr(header, "3"))
			{
				TrackParts = "S1.2 S1.3";
				recBytes = read(dfs1, S1Content2, BUFSIZE);
				if (recBytes == 0)
					printf("Failure to read %s.2 from Server 1\n", filename);
				recBytes = read(dfs1, S1Content3, BUFSIZE);
				if (recBytes == 0)
					printf("Failure to read %s.3 from Server 1\n", filename);
			}
			else
			{
				TrackParts = "S1.3 S1.4";
				recBytes = read(dfs1, S1Content3, BUFSIZE);
				if (recBytes == 0)
					printf("Failure to read %s.3 from Server 1\n", filename);
				recBytes = read(dfs1, S1Content4, BUFSIZE);
				if (recBytes == 0)
					printf("Failure to read %s.4 from Server 1\n", filename);
			}
		}
	}
	else
		fail++;
/*----------------------Server 2--------------------------------
	sentAuth = send(dfs2, auth, strlen(auth), 0);
	
	if (sentAuth == 0)
		printf("Failure to send authorization to Server 2\n");
	
	sentAuth = read(dfs2, AuthBuf, BUFSIZE);
	printf("%s\n", AuthBuf);
	if(!strstr(AuthBuf, "Invalid"))
	{
		send(dfs2, getRequest, strlen(getRequest), 0);
		recBytes = read(dfs2, header, BUFSIZE);
		if (recBytes == 0)
			printf("Failure to read header from Server 2\n");
		else if (strstr(header, "1") || strstr(header, "2") || strstr(header, "3") || strstr(header, "4"))
		{
			printf("%s\n", header);
			fail++;
		}
		else
		{
			if (strstr(header, "1") && strstr(header, "2"))
			{
				recBytes = read(dfs2, S2Content1, BUFSIZE);
				if (recBytes == 0)
					printf("Failure to read %s.1 from Server 2\n", filename);
				recBytes = read(dfs2, S1Content2, BUFSIZE);
				if (recBytes == 0)
					printf("Failure to read %s.2 from Server 2\n", filename);
			}
			else if (strstr(header, "1") && strstr(header, "4"))
			{
				recBytes = read(dfs2, S2Content1, BUFSIZE);
				if (recBytes == 0)
					printf("Failure to read %s.1 from Server 2\n", filename);
				recBytes = read(dfs2, S2Content4, BUFSIZE);
				if (recBytes == 0)
					printf("Failure to read %s.4 from Server 2\n", filename);
			}
			else if (strstr(header, "2") && strstr(header, "3"))
			{
				recBytes = read(dfs2, S2Content2, BUFSIZE);
				if (recBytes == 0)
					printf("Failure to read %s.2 from Server 2\n", filename);
				recBytes = read(dfs2, S2Content3, BUFSIZE);
				if (recBytes == 0)
					printf("Failure to read %s.3 from Server 2\n", filename);
			}
			else
			{
				recBytes = read(dfs2, S2Content3, BUFSIZE);
				if (recBytes == 0)
					printf("Failure to read %s.3 from Server 2\n", filename);
				recBytes = read(dfs2, S2Content4, BUFSIZE);
				if (recBytes == 0)
					printf("Failure to read %s.4 from Server 2\n", filename);
			}
		}
	}
	else
		fail++;
/*----------------------Server 3--------------------------------
	sentAuth = send(dfs3, auth, strlen(auth), 0);
	
	if (sentAuth == 0)
		printf("Failure to send authorization to Server 3\n");
	
	sentAuth = read(dfs3, AuthBuf, BUFSIZE);
	printf("%s\n", AuthBuf);
	if(!strstr(AuthBuf, "Invalid"))
	{
		send(dfs3, getRequest, strlen(getRequest), 0);
		recBytes = read(dfs3, header, BUFSIZE);
		if (recBytes == 0)
			printf("Failure to read header from Server 3\n");
		else if (strstr(header, "1") || strstr(header, "2") || strstr(header, "3") || strstr(header, "4"))
		{
			printf("%s\n", header);
			fail++;
		}
		else
		{
			if (strstr(header, "1") && strstr(header, "2"))
			{
				recBytes = read(dfs3, S3Content1, BUFSIZE);
				if (recBytes == 0)
					printf("Failure to read %s.1 from Server 3\n", filename);
				recBytes = read(dfs3, S3Content2, BUFSIZE);
				if (recBytes == 0)
					printf("Failure to read %s.2 from Server 3\n", filename);
			}
			else if (strstr(header, "1") && strstr(header, "4"))
			{
				recBytes = read(dfs3, S3Content1, BUFSIZE);
				if (recBytes == 0)
					printf("Failure to read %s.1 from Server 3\n", filename);
				recBytes = read(dfs3, S3Content4, BUFSIZE);
				if (recBytes == 0)
					printf("Failure to read %s.4 from Server 3\n", filename);
			}
			else if (strstr(header, "2") && strstr(header, "3"))
			{
				recBytes = read(dfs3, S3Content2, BUFSIZE);
				if (recBytes == 0)
					printf("Failure to read %s.2 from Server 3\n", filename);
				recBytes = read(dfs3, S3Content3, BUFSIZE);
				if (recBytes == 0)
					printf("Failure to read %s.3 from Server 3\n", filename);
			}
			else
			{
				recBytes = read(dfs4, S4Content3, BUFSIZE);
				if (recBytes == 0)
					printf("Failure to read %s.3 from Server 4\n", filename);
				recBytes = read(dfs4, S4Content4, BUFSIZE);
				if (recBytes == 0)
					printf("Failure to read %s.4 from Server 4\n", filename);
			}
		}
	}
	else
		fail++;
/*----------------------Server 4--------------------------------
	sentAuth = send(dfs4, auth, strlen(auth), 0);
	
	if (sentAuth == 0)
		printf("Failure to send authorization to Server 4\n");
	
	sentAuth = read(dfs4, AuthBuf, BUFSIZE);
	printf("%s\n", AuthBuf);
	if(!strstr(AuthBuf, "Invalid"))
	{
		send(dfs4, getRequest, strlen(getRequest), 0);
		recBytes = read(dfs4, header, BUFSIZE);
		if (recBytes == 0)
			printf("Failure to read header from Server 4\n");
		else if (strstr(header, "1") || strstr(header, "2") || strstr(header, "3") || strstr(header, "4"))
		{
			printf("%s\n", header);
			fail++;
		}
		else
		{
			if (strstr(header, "1") && strstr(header, "2"))
			{
				recBytes = read(dfs4, S4Content1, BUFSIZE);
				if (recBytes == 0)
					printf("Failure to read %s.1 from Server 4\n", filename);
				recBytes = read(dfs4, S4Content2, BUFSIZE);
				if (recBytes == 0)
					printf("Failure to read %s.2 from Server 4\n", filename);
			}
			else if (strstr(header, "1") && strstr(header, "4"))
			{
				recBytes = read(dfs4, S4Content1, BUFSIZE);
				if (recBytes == 0)
					printf("Failure to read %s.1 from Server 4\n", filename);
				recBytes = read(dfs4, S4Content4, BUFSIZE);
				if (recBytes == 0)
					printf("Failure to read %s.4 from Server 4\n", filename);
			}
			else if (strstr(header, "2") && strstr(header, "3"))
			{
				recBytes = read(dfs4, S4Content2, BUFSIZE);
				if (recBytes == 0)
					printf("Failure to read %s.2 from Server 4\n", filename);
				recBytes = read(dfs4, S4Content3, BUFSIZE);
				if (recBytes == 0)
					printf("Failure to read %s.3 from Server 4\n", filename);
			}
			else
			{
				recBytes = read(dfs4, S4Content3, BUFSIZE);
				if (recBytes == 0)
					printf("Failure to read %s.3 from Server 4\n", filename);
				recBytes = read(dfs4, S4Content4, BUFSIZE);
				if (recBytes == 0)
					printf("Failure to read %s.4 from Server 4\n", filename);
			}
		}
	}
	else
		fail++;
/*---------Start putting the content back together--------------
	if (fail > 1)
	{
		printf("File is incomplete\n");
		free(header);
		free(S1Content1);
		free(S1Content2);
		free(S1Content3);
		free(S1Content4);
		free(S2Content1);
		free(S2Content2);
		free(S2Content3);
		free(S2Content4);
		free(S3Content1);
		free(S3Content2);
		free(S3Content3);
		free(S3Content4);
		free(S4Content1);
		free(S4Content2);
		free(S4Content3);
		free(S4Content4);
		free(AuthBuf);
		return;
	}
	else if (strstr(TrackParts, "S1.1 S1.2"))
	{
		if (strlen(S4Content1) <= strlen(S1Content1))
			FileContent = S1Content1;
		else
			FileContent = S4Content1;
		if (strlen(S1Content2) <= strlen(S2Content2))
			strcat(FileContent, S2Content2);
		else
			strcat(FileContent, S1Content2);
		if (strlen(S2Content3) <= strlen(S3Content3))
			strcat(FileContent, S3Content3);
		else
			strcat(FileContent, S2Content3);
		if (strlen(S3Content4) <= strlen(S4Content4))
			strcat(FileContent, S4Content4);
		else
			strcat(FileContent, S3Content4);
	}
	else if (strstr(TrackParts, "S1.1 S1.4"))
	{
		if (strlen(S1Content1) <= strlen(S2Content1))
			FileContent = S2Content1;
		else
			FileContent = S1Content1;
		if (strlen(S2Content2) <= strlen(S3Content2))
			strcat(FileContent, S3Content2);
		else
			strcat(FileContent, S2Content2);
		if (strlen(S3Content3) <= strlen(S4Content3))
			strcat(FileContent, S4Content3);
		else
			strcat(FileContent, S3Content3);
		if (strlen(S4Content4) <= strlen(S1Content4))
			strcat(FileContent, S1Content4);
		else
			strcat(FileContent, S4Content4);		
	}
	else if (strstr(TrackParts, "S1.2 S1.3"))
	{
		if (strlen(S2Content1) <= strlen(S3Content1))
			FileContent = S3Content1;
		else
			FileContent = S2Content1;
		if (strlen(S3Content2) <= strlen(S4Content2))
			strcat(FileContent, S4Content2);
		else
			strcat(FileContent, S3Content2);
		if (strlen(S4Content3) <= strlen(S1Content3))
			strcat(FileContent, S1Content3);
		else
			strcat(FileContent, S4Content3);
		if (strlen(S1Content4) <= strlen(S4Content4))
			strcat(FileContent, S2Content4);
		else
			strcat(FileContent, S1Content4);
	}
	else
		if (strlen(S3Content1) <= strlen(S4Content1))
			FileContent = S4Content1;
		else
			FileContent = S3Content1;
		if (strlen(S4Content2) <= strlen(S1Content2))
			strcat(FileContent, S1Content2);
		else
			strcat(FileContent, S4Content2);
		if (strlen(S1Content3) <= strlen(S2Content3))
			strcat(FileContent, S2Content3);
		else
			strcat(FileContent, S1Content3);
		if (strlen(S2Content4) <= strlen(S3Content4))
			strcat(FileContent, S3Content4);
		else
			strcat(FileContent, S2Content4);
/*-------------------Decrypt FileContent------------------------

/*------------Write file and content to directory---------------
	fp = fopen(Root, "w");
	fputs(FileContent, fp);
	fclose(fp);

	free(header);
	free(S1Content1);
	free(S1Content2);
	free(S1Content3);
	free(S1Content4);
	free(S2Content1);
	free(S2Content2);
	free(S2Content3);
	free(S2Content4);
	free(S3Content1);
	free(S3Content2);
	free(S3Content3);
	free(S3Content4);
	free(S4Content1);
	free(S4Content2);
	free(S4Content3);
	free(S4Content4);
	free(AuthBuf);
*/
}

void PUT(struct conf dfcConfig, const char *filename, const int dfs1, const int dfs2, const int dfs3, const int dfs4)
{
	struct stat st;
	char root[50];
	char FileNameToSend[50];
	char MsgSize[15];
	char LastMsgSize[15];
	char *AuthBuf = calloc(BUFSIZE, 1);
	int PieceSize, dist, LastSize, sentAuth;
	
	strcpy(root, "./");
	strcat(root, filename);

	FILE *fp = fopen(root, "rb");
	
	if (fp != NULL)
		dist = hashDist(filename);
	else
	{
		printf("%s does not exist\n", filename);
		return;
	}

	stat(root, &st);
	PieceSize = st.st_size/4 + 1;
	LastSize = PieceSize + 4;
    
    char *part1 = calloc(PieceSize, 1);
    char *part2 = calloc(PieceSize, 1);
    char *part3 = calloc(PieceSize, 1);
    char *part4 = calloc(LastSize, 1);
    
    fgets(part1, PieceSize, fp);
    fgets(part2, PieceSize, fp);
    fgets(part3, PieceSize, fp);
    fgets(part4, LastSize, fp);
    fclose(fp);
    int PartSize = strlen(part1);
   	sprintf(MsgSize, "%d", PartSize);
   	printf("MsgSize %s\n", MsgSize);
   	int Msg = strlen(MsgSize);

   	int LastPartSize = strlen(part4);
    sprintf(LastMsgSize, "%d", LastPartSize);

    strcpy(FileNameToSend, "/.");
	strcat(FileNameToSend, filename);
	strcat(FileNameToSend, ".1");
	int FileNameLength = strlen(FileNameToSend);


/*----------------------Server 1--------------------------------*/
	sentAuth = send(dfs1, dfcConfig.Username, strlen(dfcConfig.Username), 0);
	if (sentAuth == 0)
		printf("Failure to send Username authorization to Server 1\n");
	
	sentAuth = read(dfs1, AuthBuf, BUFSIZE);
	printf("%s\n", AuthBuf);

	if(!strstr(AuthBuf, "Invalid"))
	{
		sentAuth = send(dfs1, dfcConfig.Password, strlen(dfcConfig.Password), 0);
		if (sentAuth == 0)
			printf("Failure to send Password authorization to Server 1\n");
		
		sentAuth = read(dfs1, AuthBuf, BUFSIZE);
		printf("%s\n", AuthBuf);
		
		if(!strstr(AuthBuf, "Invalid"))
		{
			send(dfs1, "PUT", 3, 0);
			switch(dist)
			{
				case 0 ://dfs1 (1,2)
					send(dfs1, (char*)&Msg, sizeof(Msg), 0);
					send(dfs1, MsgSize, strlen(MsgSize), 0);
					send(dfs1, (char*)&FileNameLength, sizeof(FileNameLength), 0);
					send(dfs1, FileNameToSend, strlen(FileNameToSend), 0);
					send(dfs1, part1, strlen(part1), 0);
					
					FileNameToSend[strlen(FileNameToSend)-1] = '2';
					send(dfs1, (char*)&Msg, sizeof(Msg), 0);
					send(dfs1, MsgSize, strlen(MsgSize), 0);
					send(dfs1, (char*)&FileNameLength, sizeof(FileNameLength), 0);
					send(dfs1, FileNameToSend, strlen(FileNameToSend), 0);
					send(dfs1, part2, strlen(part2), 0);

				case 1 ://dfs1 (2,3)
					FileNameToSend[strlen(FileNameToSend)-1] = '2';					
					send(dfs1, MsgSize, strlen(MsgSize), 0);
					send(dfs1, FileNameToSend, strlen(FileNameToSend), 0);
					send(dfs1, part2, strlen(part2), 0);
					
					FileNameToSend[strlen(FileNameToSend)-1] = '3';
					send(dfs1, MsgSize, strlen(MsgSize), 0);
					send(dfs1, FileNameToSend, strlen(FileNameToSend), 0);
					send(dfs1, part3, strlen(part3), 0);

				case 2 ://dfs1 (3,4)
					FileNameToSend[strlen(FileNameToSend)-1] = '3';					
					send(dfs1, MsgSize, strlen(MsgSize), 0);
					send(dfs1, FileNameToSend, strlen(FileNameToSend), 0);
					send(dfs1, part3, strlen(part3), 0);
					
					FileNameToSend[strlen(FileNameToSend)-1] = '4';
					send(dfs1, LastMsgSize, strlen(LastMsgSize), 0);
					send(dfs1, FileNameToSend, strlen(FileNameToSend), 0);
					send(dfs1, part4, strlen(part4), 0);

				case 3 ://dfs1 (4,1)
					FileNameToSend[strlen(FileNameToSend)-1] = '4';
					send(dfs1, LastMsgSize, strlen(LastMsgSize), 0);
					send(dfs1, FileNameToSend, strlen(FileNameToSend), 0);
					send(dfs1, part4, strlen(part4), 0);

					FileNameToSend[strlen(FileNameToSend)-1] = '1';
					send(dfs1, MsgSize, strlen(MsgSize), 0);
					send(dfs1, FileNameToSend, strlen(FileNameToSend), 0);	
					send(dfs1, part1, strlen(part1), 0);
			}
		}
	}
/*----------------------Server 2--------------------------------*/
/*	sentAuth = send(dfs2, dfcConfig.Username, strlen(dfcConfig.Username), 0);
	if (sentAuth == 0)
		printf("Failure to send Username authorization to Server 2\n");
	
	sentAuth = read(dfs2, AuthBuf, BUFSIZE);
	printf("%s\n", AuthBuf);

	if(!strstr(AuthBuf, "Invalid"))
	{
		sentAuth = send(dfs2, dfcConfig.Password, strlen(dfcConfig.Password), 0);
		if (sentAuth == 0)
			printf("Failure to send Password authorization to Server 2\n");
		
		sentAuth = read(dfs2, AuthBuf, BUFSIZE);
		printf("%s\n", AuthBuf);

		if(!strstr(AuthBuf, "Invalid"))
		{
			send(dfs2, "PUT", 3, 0);
			switch(dist)
			{
				case 0 ://dfs2 (2,3)
					send(dfs2, MsgSize, PieceSize, 0);
					send(dfs2, FileNameToSend, strlen(FileNameToSend), 0);
					
					FileNameToSend[strlen(FileNameToSend)-1] = '3';
					send(dfs2, MsgSize, PieceSize, 0);
					send(dfs2, FileNameToSend, strlen(FileNameToSend), 0);

				case 1 ://dfs2 (3,4)
					send(dfs2, MsgSize, PieceSize, 0);
					send(dfs2, FileNameToSend, strlen(FileNameToSend), 0);
					
					LastSize = st.st_size - (3*PieceSize);
					sprintf(LastMsgSize, "%d", LastSize);
					FileNameToSend[strlen(FileNameToSend)-1] = '4';
					send(dfs2, LastMsgSize, PieceSize, 0);
					send(dfs2, FileNameToSend, strlen(FileNameToSend), 0);

				case 2 ://dfs2 (4,1)
					send(dfs2, LastMsgSize, PieceSize, 0);
					send(dfs2, FileNameToSend, strlen(FileNameToSend), 0);
					
					FileNameToSend[strlen(FileNameToSend)-1] = '1';
					send(dfs2, MsgSize, PieceSize, 0);
					send(dfs2, FileNameToSend, strlen(FileNameToSend), 0);

				case 3 ://dfs2 (1,2)
					send(dfs2, MsgSize, PieceSize, 0);
					send(dfs2, FileNameToSend, strlen(FileNameToSend), 0);
					
					FileNameToSend[strlen(FileNameToSend)-1] = '2';
					send(dfs2, MsgSize, PieceSize, 0);
					send(dfs2, FileNameToSend, strlen(FileNameToSend), 0);
			}
		}
	}*/
/*----------------------Server 3--------------------------------*/
/*	sentAuth = send(dfs3, dfcConfig.Username, strlen(dfcConfig.Username), 0);
	if (sentAuth == 0)
		printf("Failure to send Username authorization to Server 3\n");
	
	sentAuth = read(dfs3, AuthBuf, BUFSIZE);
	printf("%s\n", AuthBuf);
	if(!strstr(AuthBuf, "Invalid"))
	{
		sentAuth = send(dfs3, dfcConfig.Password, strlen(dfcConfig.Password), 0);
		if (sentAuth == 0)
			printf("Failure to send Password authorization to Server 3\n");
		
		sentAuth = read(dfs3, AuthBuf, BUFSIZE);
		printf("%s\n", AuthBuf);
		
		if(!strstr(AuthBuf, "Invalid"))
		{
			send(dfs3, "PUT", 3, 0);
			switch(dist)
			{
				case 0 ://dfs3 (3,4)
					send(dfs3, MsgSize, PieceSize, 0);
					send(dfs3, FileNameToSend, strlen(FileNameToSend), 0);
					
					LastSize = st.st_size - (3*PieceSize);
					sprintf(LastMsgSize, "%d", LastSize);
					FileNameToSend[strlen(FileNameToSend)-1] = '4';
					send(dfs3, LastMsgSize, PieceSize, 0);
					send(dfs3, FileNameToSend, strlen(FileNameToSend), 0);

				case 1 ://dfs3 (4,1)
					send(dfs3, LastMsgSize, PieceSize, 0);
					send(dfs3, FileNameToSend, strlen(FileNameToSend), 0);
					
					FileNameToSend[strlen(FileNameToSend)-1] = '1';
					send(dfs3, MsgSize, PieceSize, 0);
					send(dfs3, FileNameToSend, strlen(FileNameToSend), 0);

				case 2 ://dfs3 (1,2)
					send(dfs3, MsgSize, PieceSize, 0);
					send(dfs3, FileNameToSend, strlen(FileNameToSend), 0);
					
					FileNameToSend[strlen(FileNameToSend)-1] = '2';
					send(dfs3, MsgSize, PieceSize, 0);
					send(dfs3, FileNameToSend, strlen(FileNameToSend), 0);

				case 3 ://dfs3 (2,3)
					send(dfs3, MsgSize, PieceSize, 0);
					send(dfs3, FileNameToSend, strlen(FileNameToSend), 0);
					
					FileNameToSend[strlen(FileNameToSend)-1] = '3';
					send(dfs2, MsgSize, PieceSize, 0);
					send(dfs2, FileNameToSend, strlen(FileNameToSend), 0);
			}
		}
	}*/
/*----------------------Server 4--------------------------------*/
/*	sentAuth = send(dfs4, dfcConfig.Username, strlen(dfcConfig.Username), 0);
	if (sentAuth == 0)
		printf("Failure to send Username authorization to Server 4\n");
	
	sentAuth = read(dfs4, AuthBuf, BUFSIZE);
	printf("%s\n", AuthBuf);
	if(!strstr(AuthBuf, "Invalid"))
	{
		sentAuth = send(dfs4, dfcConfig.Password, strlen(dfcConfig.Password), 0);
		if (sentAuth == 0)
			printf("Failure to send Password authorization to Server 4\n");
		
		sentAuth = read(dfs4, AuthBuf, BUFSIZE);
		printf("%s\n", AuthBuf);
		
		if(!strstr(AuthBuf, "Invalid"))
		{
			send(dfs4, "PUT", 3, 0);
			switch(dist)
			{
				case 0 ://dfs4(4,1)
					send(dfs3, LastMsgSize, PieceSize, 0);
					send(dfs3, FileNameToSend, strlen(FileNameToSend), 0);
					
					FileNameToSend[strlen(FileNameToSend)-1] = '1';
					send(dfs3, MsgSize, PieceSize, 0);
					send(dfs3, FileNameToSend, strlen(FileNameToSend), 0);

				case 1 ://dfs4(1,2)
					send(dfs3, MsgSize, PieceSize, 0);
					send(dfs3, FileNameToSend, strlen(FileNameToSend), 0);
					
					FileNameToSend[strlen(FileNameToSend)-1] = '2';
					send(dfs3, MsgSize, PieceSize, 0);
					send(dfs3, FileNameToSend, strlen(FileNameToSend), 0);

				case 2 ://dfs4(2,3)
					send(dfs3, MsgSize, PieceSize, 0);
					send(dfs3, FileNameToSend, strlen(FileNameToSend), 0);
					
					FileNameToSend[strlen(FileNameToSend)-1] = '3';
					send(dfs2, MsgSize, PieceSize, 0);
					send(dfs2, FileNameToSend, strlen(FileNameToSend), 0);

				case 3 ://dfs4(3,4)
					send(dfs3, MsgSize, PieceSize, 0);
					send(dfs3, FileNameToSend, strlen(FileNameToSend), 0);

					FileNameToSend[strlen(FileNameToSend)-1] = '4';
					send(dfs3, LastMsgSize, PieceSize, 0);
					send(dfs3, FileNameToSend, strlen(FileNameToSend), 0);		
			}
		}
	}*/

	free(part1);
	free(part2);
	free(part3);
	free(part4);

}

void LIST(struct conf dfcConfig, const int dfs1, const int dfs2, const int dfs3, const int dfs4)
{
/*	char *AuthBuf = calloc(BUFSIZE, 1);
	char *S1List = calloc(BUFSIZE, 1);
	char *S2List = calloc(BUFSIZE, 1);
	char *S3List = calloc(BUFSIZE, 1);
	char *S4List = calloc(BUFSIZE, 1);
	char *FileList = "";
	char *line;
	int sentAuth, numFiles1, numFiles2, numFiles3, numFiles4;
	int fail = 0;

----------------------Server 1--------------------------------
	sentAuth = send(dfs1, auth, strlen(auth), 0);
	
	if (sentAuth == 0)
		printf("Failure to send authorization to Server 1\n");
	sentAuth = read(dfs1, AuthBuf, BUFSIZE);
	printf("%s\n", AuthBuf);
	if(!strstr(AuthBuf, "Invalid"))
	{
		send(dfs1, "LIST ./", 7, 0);
		read(dfs1, S1List, BUFSIZE);
		while((line = strsep(&S1List, "\n")) != NULL)
		{
			if(!strstr(FileList, line))
			{
				strcat(FileList, line);
				strcat(FileList, "\n");
			}
			else
				numFiles1++;
		}
	}
	else
		fail++;
/*----------------------Server 1--------------------------------
	sentAuth = send(dfs2, auth, strlen(auth), 0);
	
	if (sentAuth == 0)
		printf("Failure to send authorization to Server 2\n");
	sentAuth = read(dfs2, AuthBuf, BUFSIZE);
	printf("%s\n", AuthBuf);
	if(!strstr(AuthBuf, "Invalid"))
	{
		send(dfs2, "LIST ./", 7, 0);
		read(dfs2, S2List, BUFSIZE);
		while((line = strsep(&S2List, "\n")) != NULL)
		{
			if(!strstr(FileList, line))
			{
				strcat(FileList, line);
				strcat(FileList, "\n");
			}
			else
				numFiles2++;
		}
	}
	else
		fail++;	
/*----------------------Server 3--------------------------------
	sentAuth = send(dfs3, auth, strlen(auth), 0);
	
	if (sentAuth == 0)
		printf("Failure to send authorization to Server 3\n");
	sentAuth = read(dfs3, AuthBuf, BUFSIZE);
	printf("%s\n", AuthBuf);
	if(!strstr(AuthBuf, "Invalid"))
	{
		send(dfs3, "LIST ./", 7, 0);
		read(dfs3, S3List, BUFSIZE);
		while((line = strsep(&S3List, "\n")) != NULL)
		{
			if(!strstr(FileList, line))
			{
				strcat(FileList, line);
				strcat(FileList, "\n");
			}
			else
				numFiles3++;
		}
	}
	else
		fail++;
/*----------------------Server 4--------------------------------
	sentAuth = send(dfs4, auth, strlen(auth), 0);
	
	if (sentAuth == 0)
		printf("Failure to send authorization to Server 4\n");
	sentAuth = read(dfs4, AuthBuf, BUFSIZE);
	printf("%s\n", AuthBuf);
	if(!strstr(AuthBuf, "Invalid"))
	{
		send(dfs4, "LIST ./", 7, 0);
		read(dfs4, S4List, BUFSIZE);
		while((line = strsep(&S4List, "\n")) != NULL)
		{
			if(!strstr(FileList, line))
			{
				strcat(FileList, line);
				strcat(FileList, "\n");
			}
			else
				numFiles4++;
		}
	}
	else
		fail++;
/*------------Compare the number of files-----------------------
	if (numFiles1 == numFiles2 && numFiles2 == numFiles3 && numFiles3 == numFiles4)
		printf("%s", FileList);
	else if (fail > 1)
		while((line = strsep(&FileList, "\n")) != NULL)
			printf("%s [incomplete]\n", line);
	//else
		//figure out how to tell which specific file is incomplete
*/
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