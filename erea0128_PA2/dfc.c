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
char* XOR(char *string, char *key);
void GET(struct conf dfcConfig, const char *filename);
void PUT(struct conf dfcConfig, const char *filename);
void LIST(struct conf dfcConfig);

int main(int argc, char* argv[])
{
	struct conf dfcConfig;
	char *buf = calloc(BUFSIZE, 1);
	char *string, *line, *token, *request;
	char *filename = "\0";
	int count = 0;

	dfcConfig = parseConfig(argv[1]);
	
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
				GET(dfcConfig, filename);
			else
				printf("No filename given\n");	
		}
		else if (strcmp(request, "PUT") == 0)
		{	
			if(strcmp(filename, "\0") != 0)
				PUT(dfcConfig, filename);
			else
				printf("No filename given\n");
		}
		else if (strcmp(request, "LIST") == 0)
			LIST(dfcConfig);
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
	int mdhash = 0; 

	if (fp != NULL)
	{
		MD5_Init (&mdContext);
	    while ((bytes = fread (fileContent, 1, 1024, fp)) != 0)
	        MD5_Update (&mdContext, fileContent, bytes);
	    
	    MD5_Final (hash,&mdContext);
	    mdhash = hash[MD5_DIGEST_LENGTH - 1] % 4;
	    fclose (fp);

	    return mdhash;
	}
	return -1;
}

void GET(struct conf dfcConfig, const char *filename)
{	
	char *AuthBuf = calloc(BUFSIZE, 1);
	char *S1Content1, *S1Content2, *S2Content1, *S2Content2, *S3Content1, *S3Content2, *S4Content1, *S4Content2, *MsgSize;
	char ServerPieces[1];
	char *Hashing = calloc(2, 1);
	char Root[50];
	char *host = "localhost";
	FILE *fp;
	int sentAuth, filenameSize, hashType, PieceSize, bytes, ContentSize, dfs1, dfs2, dfs3, dfs4;

	filenameSize = strlen(filename);			
/*----------------------Server 1--------------------------------*/
	dfs1 = connectsock(host, dfcConfig.DFS1);
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
			send(dfs1, "GET ", 4, 0);
			send(dfs1, (char*)&filenameSize, sizeof(filenameSize), 0);
			send(dfs1, filename, strlen(filename), 0);

			bytes = read(dfs1, ServerPieces, 1);
			if (bytes < 0)
				printf("Error reading in first Piece number\n");

			Hashing[0] = ServerPieces[0];
			bytes = read(dfs1, (char*) &PieceSize, sizeof(PieceSize));
			if (bytes < 0)
				printf("Error reading in Piece Size\n");

			MsgSize = calloc(PieceSize, 1);
			bytes = read(dfs1, MsgSize, PieceSize);
			if (bytes < 0)
				printf("Error reading in Msg Size\n");

			ContentSize = atoi(MsgSize);
			S1Content1 = calloc(ContentSize, 1);
			bytes = read(dfs1, S1Content1, ContentSize);
			if (bytes < 0)
				printf("Error reading in file\n");
			free(MsgSize);

			bytes = read(dfs1, ServerPieces, 1);
			if (bytes < 0)
				printf("Error reading in second Piece number\n");

			Hashing[1] = ServerPieces[0];
			bytes = read(dfs1, (char*) &PieceSize, sizeof(PieceSize));
			if (bytes < 0)
				printf("Error reading in Piece Size\n");

			MsgSize = calloc(PieceSize, 1);
			bytes = read(dfs1, MsgSize, PieceSize);
			if (bytes < 0)
				printf("Error reading in Msg Size\n");

			ContentSize = atoi(MsgSize);
			S1Content2 = calloc(ContentSize, 1);
			bytes = read(dfs1, S1Content2, ContentSize);
			if (bytes < 0)
				printf("Error reading in file\n");
			free(MsgSize);

			if ((Hashing[0] == '1' && Hashing[1] == '2') || (Hashing[1] == '1' && Hashing[0] == '2'))
				hashType = 0;
			else if ((Hashing[0] == '2' && Hashing[1] == '3') || (Hashing[1] == '2' && Hashing[0] == '3'))
				hashType = 1;
			else if ((Hashing[0] == '3' && Hashing[1] == '4') || (Hashing[1] == '3' && Hashing[0] == '4'))
				hashType = 2;
			else
				hashType = 3;	
		}
	}
/*----------------------Server 2--------------------------------*/
	dfs2 = connectsock(host, dfcConfig.DFS2);
	sentAuth = send(dfs2, dfcConfig.Username, strlen(dfcConfig.Username), 0);
	if (sentAuth == 0)
		printf("Failure to send Username authorization to Server 1\n");
	
	sentAuth = read(dfs2, AuthBuf, BUFSIZE);
	printf("%s\n", AuthBuf);

	if(!strstr(AuthBuf, "Invalid"))
	{
		sentAuth = send(dfs2, dfcConfig.Password, strlen(dfcConfig.Password), 0);
		if (sentAuth == 0)
			printf("Failure to send Password authorization to Server 1\n");
		
		sentAuth = read(dfs2, AuthBuf, BUFSIZE);
		printf("%s\n", AuthBuf);
		
		if(!strstr(AuthBuf, "Invalid"))
		{
			send(dfs2, "GET ", 4, 0);
			send(dfs2, (char*)&filenameSize, sizeof(filenameSize), 0);
			send(dfs2, filename, strlen(filename), 0);

			bytes = read(dfs2, ServerPieces, 1);
			if (bytes < 0)
				printf("Error reading in first Piece number\n");

			bytes = read(dfs2, (char*) &PieceSize, sizeof(PieceSize));
			if (bytes < 0)
				printf("Error reading in Piece Size\n");

			MsgSize = calloc(PieceSize, 1);
			bytes = read(dfs2, MsgSize, PieceSize);
			if (bytes < 0)
				printf("Error reading in Msg Size\n");

			ContentSize = atoi(MsgSize);
			S2Content1 = calloc(ContentSize, 1);
			bytes = read(dfs2, S2Content1, ContentSize);
			if (bytes < 0)
				printf("Error reading in file\n");
			free(MsgSize);

			bytes = read(dfs2, ServerPieces, 1);
			if (bytes < 0)
				printf("Error reading in first Piece number\n");

			bytes = read(dfs2, (char*) &PieceSize, sizeof(PieceSize));
			if (bytes < 0)
				printf("Error reading in Piece Size\n");

			MsgSize = calloc(PieceSize, 1);
			bytes = read(dfs2, MsgSize, PieceSize);
			if (bytes < 0)
				printf("Error reading in Msg Size\n");

			ContentSize = atoi(MsgSize);
			S2Content2 = calloc(ContentSize, 1);
			bytes = read(dfs2, S2Content2, ContentSize);
			if (bytes < 0)
				printf("Error reading in file\n");	
		}
	}			
/*----------------------Server 3--------------------------------*/
	dfs3 = connectsock(host, dfcConfig.DFS3);
	sentAuth = send(dfs3, dfcConfig.Username, strlen(dfcConfig.Username), 0);
	if (sentAuth == 0)
		printf("Failure to send Username authorization to Server 1\n");
	
	sentAuth = read(dfs3, AuthBuf, BUFSIZE);
	printf("%s\n", AuthBuf);

	if(!strstr(AuthBuf, "Invalid"))
	{
		sentAuth = send(dfs3, dfcConfig.Password, strlen(dfcConfig.Password), 0);
		if (sentAuth == 0)
			printf("Failure to send Password authorization to Server 1\n");
		
		sentAuth = read(dfs3, AuthBuf, BUFSIZE);
		printf("%s\n", AuthBuf);
		
		if(!strstr(AuthBuf, "Invalid"))
		{
			send(dfs3, "GET ", 4, 0);
			send(dfs3, (char*)&filenameSize, sizeof(filenameSize), 0);
			send(dfs3, filename, strlen(filename), 0);

			bytes = read(dfs3, ServerPieces, 1);
			if (bytes < 0)
				printf("Error reading in first Piece number\n");

			bytes = read(dfs3, (char*) &PieceSize, sizeof(PieceSize));
			if (bytes < 0)
				printf("Error reading in Piece Size\n");

			MsgSize = calloc(PieceSize, 1);
			bytes = read(dfs3, MsgSize, PieceSize);
			if (bytes < 0)
				printf("Error reading in Msg Size\n");

			ContentSize = atoi(MsgSize);
			S3Content1 = calloc(ContentSize, 1);
			bytes = read(dfs3, S3Content1, ContentSize);
			if (bytes < 0)
				printf("Error reading in file\n");
			free(MsgSize);

			bytes = read(dfs3, ServerPieces, 1);
			if (bytes < 0)
				printf("Error reading in first Piece number\n");

			bytes = read(dfs3, (char*) &PieceSize, sizeof(PieceSize));
			if (bytes < 0)
				printf("Error reading in Piece Size\n");

			MsgSize = calloc(PieceSize, 1);
			bytes = read(dfs3, MsgSize, PieceSize);
			if (bytes < 0)
				printf("Error reading in Msg Size\n");

			ContentSize = atoi(MsgSize);
			S3Content2 = calloc(ContentSize, 1);
			bytes = read(dfs3, S3Content2, ContentSize);
			if (bytes < 0)
				printf("Error reading in file\n");
		}
	}			
/*----------------------Server 4--------------------------------*/
	dfs4 = connectsock(host, dfcConfig.DFS4);
	sentAuth = send(dfs4, dfcConfig.Username, strlen(dfcConfig.Username), 0);
	if (sentAuth == 0)
		printf("Failure to send Username authorization to Server 1\n");
	
	sentAuth = read(dfs4, AuthBuf, BUFSIZE);
	printf("%s\n", AuthBuf);

	if(!strstr(AuthBuf, "Invalid"))
	{
		sentAuth = send(dfs4, dfcConfig.Password, strlen(dfcConfig.Password), 0);
		if (sentAuth == 0)
			printf("Failure to send Password authorization to Server 1\n");
		
		sentAuth = read(dfs4, AuthBuf, BUFSIZE);
		printf("%s\n", AuthBuf);
		
		if(!strstr(AuthBuf, "Invalid"))
		{
			send(dfs4, "GET ", 4, 0);
			send(dfs4, (char*)&filenameSize, sizeof(filenameSize), 0);
			send(dfs4, filename, strlen(filename), 0);

			bytes = read(dfs4, ServerPieces, 1);
			if (bytes < 0)
				printf("Error reading in first Piece number\n");

			bytes = read(dfs4, (char*) &PieceSize, sizeof(PieceSize));
			if (bytes < 0)
				printf("Error reading in Piece Size\n");

			MsgSize = calloc(PieceSize, 1);
			bytes = read(dfs4, MsgSize, PieceSize);
			if (bytes < 0)
				printf("Error reading in Msg Size\n");

			ContentSize = atoi(MsgSize);
			S4Content1 = calloc(ContentSize, 1);
			bytes = read(dfs4, S4Content1, ContentSize);
			if (bytes < 0)
				printf("Error reading in file\n");
			free(MsgSize);

			bytes = read(dfs4, ServerPieces, 1);
			if (bytes < 0)
				printf("Error reading in first Piece number\n");

			bytes = read(dfs4, (char*) &PieceSize, sizeof(PieceSize));
			if (bytes < 0)
				printf("Error reading in Piece Size\n");

			MsgSize = calloc(PieceSize, 1);
			bytes = read(dfs4, MsgSize, PieceSize);
			if (bytes < 0)
				printf("Error reading in Msg Size\n");

			ContentSize = atoi(MsgSize);
			S4Content2 = calloc(ContentSize, 1);
			bytes = read(dfs4, S4Content2, ContentSize);
			if (bytes < 0)
				printf("Error reading in file\n");
		}
	}
/*-------------------Decrypt FileContent------------------------*/
	S1Content1 = XOR(S1Content1, dfcConfig.Password);
	S1Content2 = XOR(S1Content2, dfcConfig.Password);
	S2Content1 = XOR(S2Content1, dfcConfig.Password);
	S2Content2 = XOR(S2Content2, dfcConfig.Password);
	S3Content1 = XOR(S3Content1, dfcConfig.Password);
	S3Content2 = XOR(S3Content2, dfcConfig.Password);
	S4Content1 = XOR(S4Content1, dfcConfig.Password);
	S4Content2 = XOR(S4Content2, dfcConfig.Password);
/*------------Put files back together and write to file---------*/
	strcpy(Root, "./Client/");
	strcat(Root, filename);
	fp = fopen(Root, "w");
	switch(hashType)
	{
		case 0 : //(1,2)(2,3)(3,4)(1,4)
			if (strlen(S1Content1) <= strlen(S4Content1))
				fprintf(fp, "%s", S4Content1);
			else
				fprintf(fp, "%s", S1Content1);
			if (strlen(S1Content2) <= strlen(S2Content1))
				fprintf(fp, "%s", S2Content1);
			else
				fprintf(fp, "%s", S1Content2);
			if (strlen(S2Content2) <= strlen(S3Content1))
				fprintf(fp, "%s", S3Content1);
			else
				fprintf(fp, "%s", S2Content2);
			if (strlen(S3Content2) <= strlen(S4Content2))
				fprintf(fp, "%s", S4Content2);
			else
				fprintf(fp, "%s", S3Content2);
		case 1 : //(2,3)(3,4)(1,4)(1,2)
			if (strlen(S3Content1) <= strlen(S4Content1))
				fprintf(fp, "%s", S4Content1);
			else
				fprintf(fp, "%s", S3Content1);
			if (strlen(S4Content2) <= strlen(S1Content1))
				fprintf(fp, "%s", S1Content1);
			else
				fprintf(fp, "%s", S4Content2);
			if (strlen(S1Content2) <= strlen(S2Content1))
				fprintf(fp, "%s", S2Content1);
			else
				fprintf(fp, "%s", S1Content2);
			if (strlen(S2Content2) <= strlen(S3Content2))
				fprintf(fp, "%s", S3Content2);
			else
				fprintf(fp, "%s", S2Content2);
		case 2 : //(3,4)(1,4)(1,2)(2,3)
			if (strlen(S2Content1) <= strlen(S3Content1))
				fprintf(fp, "%s", S3Content1);
			else
				fprintf(fp, "%s", S2Content1);
			if (strlen(S3Content2) <= strlen(S4Content1))
				fprintf(fp, "%s", S4Content1);
			else
				fprintf(fp, "%s", S3Content2);
			if (strlen(S4Content2) <= strlen(S1Content1))
				fprintf(fp, "%s", S1Content1);
			else
				fprintf(fp, "%s", S4Content2);
			if (strlen(S1Content2) <= strlen(S2Content2))
				fprintf(fp, "%s", S2Content2);
			else
				fprintf(fp, "%s", S1Content2);
		case 3 : //(1,4)(1,2)(2,3)(3,4)
			if (strlen(S1Content1) <= strlen(S2Content1))
				fprintf(fp, "%s", S2Content1);
			else
				fprintf(fp, "%s", S1Content1);
			if (strlen(S2Content2) <= strlen(S3Content1))
				fprintf(fp, "%s", S3Content1);
			else
				fprintf(fp, "%s", S2Content2);
			if (strlen(S3Content2) <= strlen(S4Content1))
				fprintf(fp, "%s", S4Content1);
			else
				fprintf(fp, "%s", S3Content2);
			if (strlen(S1Content2) <= strlen(S4Content2))
				fprintf(fp, "%s", S4Content2);
			else
				fprintf(fp, "%s", S1Content2);
	}
	free(S1Content1);
	free(S1Content2);
	free(S2Content1);
	free(S2Content2);
	free(S3Content1);
	free(S3Content2);
	free(S4Content1);
	free(S4Content2);
	free(AuthBuf);
	free(MsgSize);
	free(Hashing);
}

void PUT(struct conf dfcConfig, const char *filename)
{
	struct stat st;
	char root[50];
	char FileNameToSend[50];
	char MsgSize[15];
	char LastMsgSize[15];
	char *AuthBuf = calloc(BUFSIZE, 1);
	int PieceSize, dist, LastSize, sentAuth, dfs1, dfs2, dfs3, dfs4;
	char *host = "localhost";
	
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

    part1 = XOR(part1, dfcConfig.Password);
    part2 = XOR(part2, dfcConfig.Password);
    part3 = XOR(part3, dfcConfig.Password);
    part4 = XOR(part4, dfcConfig.Password);

    int PartSize = strlen(part1);
   	sprintf(MsgSize, "%d", PartSize);
   	int Msg = strlen(MsgSize);

   	int LastPartSize = strlen(part4);
    sprintf(LastMsgSize, "%d", LastPartSize);
    int LastMsg = strlen(LastMsgSize);

    strcpy(FileNameToSend, "/.");
	strcat(FileNameToSend, filename);
	strcat(FileNameToSend, ".1");
	int FileNameLength = strlen(FileNameToSend);

/*----------------------Server 1--------------------------------*/
	dfs1 = connectsock(host, dfcConfig.DFS1);
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
		memset(AuthBuf, 0, BUFSIZE);
		sentAuth = read(dfs1, AuthBuf, BUFSIZE);
		printf("%s\n", AuthBuf);
		
		if(!strstr(AuthBuf, "Invalid"))
		{
			send(dfs1, "PUT ", 4, 0);
			switch(dist)
			{
				case 0 ://dfs1 (1,2)
					printf("%s\n", FileNameToSend);	
					send(dfs1, (char*)&Msg, sizeof(Msg), 0);
					send(dfs1, MsgSize, strlen(MsgSize), 0);
					send(dfs1, (char*)&FileNameLength, sizeof(FileNameLength), 0);
					send(dfs1, FileNameToSend, strlen(FileNameToSend), 0);
					send(dfs1, part1, strlen(part1), 0);
					
					FileNameToSend[strlen(FileNameToSend)-1] = '2';
					printf("%s\n", FileNameToSend);	
					send(dfs1, (char*)&Msg, sizeof(Msg), 0);
					send(dfs1, MsgSize, strlen(MsgSize), 0);
					send(dfs1, (char*)&FileNameLength, sizeof(FileNameLength), 0);
					send(dfs1, FileNameToSend, strlen(FileNameToSend), 0);
					send(dfs1, part2, strlen(part2), 0);

				case 1 ://dfs1 (2,3)
					FileNameToSend[strlen(FileNameToSend)-1] = '2';
					send(dfs1, (char*)&Msg, sizeof(Msg), 0);
					send(dfs1, MsgSize, strlen(MsgSize), 0);
					send(dfs1, (char*)&FileNameLength, sizeof(FileNameLength), 0);
					send(dfs1, FileNameToSend, strlen(FileNameToSend), 0);
					send(dfs1, part2, strlen(part2), 0);
					
					FileNameToSend[strlen(FileNameToSend)-1] = '3';
					send(dfs1, (char*)&Msg, sizeof(Msg), 0);
					send(dfs1, MsgSize, strlen(MsgSize), 0);
					send(dfs1, (char*)&FileNameLength, sizeof(FileNameLength), 0);
					send(dfs1, FileNameToSend, strlen(FileNameToSend), 0);
					send(dfs1, part3, strlen(part3), 0);

				case 2 ://dfs1 (3,4)
					FileNameToSend[strlen(FileNameToSend)-1] = '3';
					send(dfs1, (char*)&Msg, sizeof(Msg), 0);
					send(dfs1, MsgSize, strlen(MsgSize), 0);
					send(dfs1, (char*)&FileNameLength, sizeof(FileNameLength), 0);
					send(dfs1, FileNameToSend, strlen(FileNameToSend), 0);
					send(dfs1, part3, strlen(part3), 0);
					
					FileNameToSend[strlen(FileNameToSend)-1] = '4';
					send(dfs1, (char*)&LastMsg, sizeof(LastMsg), 0);
					send(dfs1, LastMsgSize, strlen(LastMsgSize), 0);
					send(dfs1, (char*)&FileNameLength, sizeof(FileNameLength), 0);
					send(dfs1, FileNameToSend, strlen(FileNameToSend), 0);
					send(dfs1, part4, strlen(part4), 0);

				case 3 ://dfs1 (4,1)
					FileNameToSend[strlen(FileNameToSend)-1] = '4';
					send(dfs1, (char*)&LastMsg, sizeof(LastMsg), 0);
					send(dfs1, LastMsgSize, strlen(LastMsgSize), 0);
					send(dfs1, (char*)&FileNameLength, sizeof(FileNameLength), 0);
					send(dfs1, FileNameToSend, strlen(FileNameToSend), 0);
					send(dfs1, part4, strlen(part4), 0);

					FileNameToSend[strlen(FileNameToSend)-1] = '1';
					send(dfs1, (char*)&Msg, sizeof(Msg), 0);
					send(dfs1, MsgSize, strlen(MsgSize), 0);
					send(dfs1, (char*)&FileNameLength, sizeof(FileNameLength), 0);
					send(dfs1, FileNameToSend, strlen(FileNameToSend), 0);
					send(dfs1, part1, strlen(part1), 0);
			}
		}
		close(dfs1);
	}
/*----------------------Server 2--------------------------------*/
	dfs2 = connectsock(host, dfcConfig.DFS2);
	sentAuth = send(dfs2, dfcConfig.Username, strlen(dfcConfig.Username), 0);
	if (sentAuth == 0)
		printf("Failure to send Username authorization to Server 2\n");
	
	memset(AuthBuf, 0, BUFSIZE);
	sentAuth = read(dfs2, AuthBuf, BUFSIZE);
	printf("%s\n", AuthBuf);

	if(!strstr(AuthBuf, "Invalid"))
	{
		sentAuth = send(dfs2, dfcConfig.Password, strlen(dfcConfig.Password), 0);
		if (sentAuth == 0)
			printf("Failure to send Password authorization to Server 2\n");
		
		memset(AuthBuf, 0, BUFSIZE);
		sentAuth = read(dfs2, AuthBuf, BUFSIZE);
		printf("%s\n", AuthBuf);

		if(!strstr(AuthBuf, "Invalid"))
		{
			send(dfs2, "PUT ", 4, 0);
			printf("%d\n", dist);
			switch(dist)
			{
				case 0 ://dfs2 (2,3)
					printf("Server 2\n");
					FileNameToSend[strlen(FileNameToSend)-1] = '2';
					printf("%s\n", FileNameToSend);				
					send(dfs2, (char*)&Msg, sizeof(Msg), 0);
					send(dfs2, MsgSize, strlen(MsgSize), 0);
					send(dfs2, (char*)&FileNameLength, sizeof(FileNameLength), 0);
					send(dfs2, FileNameToSend, strlen(FileNameToSend), 0);
					send(dfs2, part2, strlen(part2), 0);
					
					FileNameToSend[strlen(FileNameToSend)-1] = '3';
					printf("%s\n", FileNameToSend);	
					send(dfs2, (char*)&Msg, sizeof(Msg), 0);
					send(dfs2, MsgSize, strlen(MsgSize), 0);
					send(dfs2, (char*)&FileNameLength, sizeof(FileNameLength), 0);
					send(dfs2, FileNameToSend, strlen(FileNameToSend), 0);
					send(dfs2, part3, strlen(part3), 0);

				case 1 ://dfs2 (3,4)
					FileNameToSend[strlen(FileNameToSend)-1] = '3';				
					send(dfs2, (char*)&Msg, sizeof(Msg), 0);
					send(dfs2, MsgSize, strlen(MsgSize), 0);
					send(dfs2, (char*)&FileNameLength, sizeof(FileNameLength), 0);
					send(dfs2, FileNameToSend, strlen(FileNameToSend), 0);
					send(dfs2, part3, strlen(part3), 0);
					
					FileNameToSend[strlen(FileNameToSend)-1] = '4';
					send(dfs2, (char*)&LastMsg, sizeof(LastMsg), 0);
					send(dfs2, LastMsgSize, strlen(LastMsgSize), 0);
					send(dfs2, (char*)&FileNameLength, sizeof(FileNameLength), 0);
					send(dfs2, FileNameToSend, strlen(FileNameToSend), 0);
					send(dfs2, part4, strlen(part4), 0);

				case 2 ://dfs2 (4,1)
					FileNameToSend[strlen(FileNameToSend)-1] = '4';
					send(dfs2, (char*)&LastMsg, sizeof(LastMsg), 0);
					send(dfs2, LastMsgSize, strlen(LastMsgSize), 0);
					send(dfs2, (char*)&FileNameLength, sizeof(FileNameLength), 0);
					send(dfs2, FileNameToSend, strlen(FileNameToSend), 0);
					send(dfs2, part4, strlen(part4), 0);

					FileNameToSend[strlen(FileNameToSend)-1] = '1';
					send(dfs2, (char*)&Msg, sizeof(Msg), 0);
					send(dfs2, MsgSize, strlen(MsgSize), 0);
					send(dfs2, (char*)&FileNameLength, sizeof(FileNameLength), 0);
					send(dfs2, FileNameToSend, strlen(FileNameToSend), 0);
					send(dfs2, part1, strlen(part1), 0);

				case 3 ://dfs2 (1,2)
					FileNameToSend[strlen(FileNameToSend)-1] = '1';
					send(dfs2, (char*)&Msg, sizeof(Msg), 0);
					send(dfs2, MsgSize, strlen(MsgSize), 0);
					send(dfs2, (char*)&FileNameLength, sizeof(FileNameLength), 0);
					send(dfs2, FileNameToSend, strlen(FileNameToSend), 0);
					send(dfs2, part1, strlen(part1), 0);
					
					FileNameToSend[strlen(FileNameToSend)-1] = '2';
					send(dfs2, (char*)&Msg, sizeof(Msg), 0);
					send(dfs2, MsgSize, strlen(MsgSize), 0);
					send(dfs2, (char*)&FileNameLength, sizeof(FileNameLength), 0);
					send(dfs2, FileNameToSend, strlen(FileNameToSend), 0);
					send(dfs2, part2, strlen(part2), 0);
			}
		}
		close(dfs2);
	}
/*----------------------Server 3--------------------------------*/
	dfs3 = connectsock(host, dfcConfig.DFS3);
	sentAuth = send(dfs3, dfcConfig.Username, strlen(dfcConfig.Username), 0);
	if (sentAuth == 0)
		printf("Failure to send Username authorization to Server 3\n");
	
	memset(AuthBuf, 0, BUFSIZE);
	sentAuth = read(dfs3, AuthBuf, BUFSIZE);
	printf("%s\n", AuthBuf);
	if(!strstr(AuthBuf, "Invalid"))
	{
		sentAuth = send(dfs3, dfcConfig.Password, strlen(dfcConfig.Password), 0);
		if (sentAuth == 0)
			printf("Failure to send Password authorization to Server 3\n");
		
		memset(AuthBuf, 0, BUFSIZE);
		sentAuth = read(dfs3, AuthBuf, BUFSIZE);
		printf("%s\n", AuthBuf);
		
		if(!strstr(AuthBuf, "Invalid"))
		{
			send(dfs3, "PUT ", 4, 0);
			switch(dist)
			{
				case 0 ://dfs3 (3,4)
					printf("Server 3\n");
					FileNameToSend[strlen(FileNameToSend)-1] = '3';
					printf("%s\n", FileNameToSend);	
					send(dfs3, (char*)&Msg, sizeof(Msg), 0);
					send(dfs3, MsgSize, strlen(MsgSize), 0);
					send(dfs3, (char*)&FileNameLength, sizeof(FileNameLength), 0);
					send(dfs3, FileNameToSend, strlen(FileNameToSend), 0);
					send(dfs3, part3, strlen(part3), 0);
					
					FileNameToSend[strlen(FileNameToSend)-1] = '4';
					printf("%s\n", FileNameToSend);	
					send(dfs3, (char*)&LastMsg, sizeof(LastMsg), 0);
					send(dfs3, LastMsgSize, strlen(LastMsgSize), 0);
					send(dfs3, (char*)&FileNameLength, sizeof(FileNameLength), 0);
					send(dfs3, FileNameToSend, strlen(FileNameToSend), 0);
					send(dfs3, part4, strlen(part4), 0);

				case 1 ://dfs3 (4,1)
					FileNameToSend[strlen(FileNameToSend)-1] = '4';
					send(dfs3, (char*)&LastMsg, sizeof(LastMsg), 0);
					send(dfs3, LastMsgSize, strlen(LastMsgSize), 0);
					send(dfs3, (char*)&FileNameLength, sizeof(FileNameLength), 0);
					send(dfs3, FileNameToSend, strlen(FileNameToSend), 0);
					send(dfs3, part4, strlen(part4), 0);

					FileNameToSend[strlen(FileNameToSend)-1] = '1';
					send(dfs3, (char*)&Msg, sizeof(Msg), 0);
					send(dfs3, MsgSize, strlen(MsgSize), 0);
					send(dfs3, (char*)&FileNameLength, sizeof(FileNameLength), 0);
					send(dfs3, FileNameToSend, strlen(FileNameToSend), 0);
					send(dfs3, part1, strlen(part1), 0);

				case 2 ://dfs3 (1,2)
					FileNameToSend[strlen(FileNameToSend)-1] = '1';
					send(dfs3, (char*)&Msg, sizeof(Msg), 0);
					send(dfs3, MsgSize, strlen(MsgSize), 0);
					send(dfs3, (char*)&FileNameLength, sizeof(FileNameLength), 0);
					send(dfs3, FileNameToSend, strlen(FileNameToSend), 0);
					send(dfs3, part1, strlen(part1), 0);
					
					FileNameToSend[strlen(FileNameToSend)-1] = '2';
					send(dfs3, (char*)&Msg, sizeof(Msg), 0);
					send(dfs3, MsgSize, strlen(MsgSize), 0);
					send(dfs3, (char*)&FileNameLength, sizeof(FileNameLength), 0);
					send(dfs3, FileNameToSend, strlen(FileNameToSend), 0);
					send(dfs3, part2, strlen(part2), 0);

				case 3 ://dfs3 (2,3)
					FileNameToSend[strlen(FileNameToSend)-1] = '2';
					send(dfs3, (char*)&Msg, sizeof(Msg), 0);
					send(dfs3, MsgSize, strlen(MsgSize), 0);
					send(dfs3, (char*)&FileNameLength, sizeof(FileNameLength), 0);
					send(dfs3, FileNameToSend, strlen(FileNameToSend), 0);
					send(dfs3, part2, strlen(part2), 0);
					
					FileNameToSend[strlen(FileNameToSend)-1] = '3';
					send(dfs3, (char*)&Msg, sizeof(Msg), 0);
					send(dfs3, MsgSize, strlen(MsgSize), 0);
					send(dfs3, (char*)&FileNameLength, sizeof(FileNameLength), 0);
					send(dfs3, FileNameToSend, strlen(FileNameToSend), 0);
					send(dfs3, part3, strlen(part3), 0);
			}
		}
		close(dfs3);
	}
/*----------------------Server 4--------------------------------*/
	dfs4 = connectsock(host, dfcConfig.DFS4);
	sentAuth = send(dfs4, dfcConfig.Username, strlen(dfcConfig.Username), 0);
	if (sentAuth == 0)
		printf("Failure to send Username authorization to Server 4\n");
	
	memset(AuthBuf, 0, BUFSIZE);
	sentAuth = read(dfs4, AuthBuf, BUFSIZE);
	printf("%s\n", AuthBuf);
	if(!strstr(AuthBuf, "Invalid"))
	{
		sentAuth = send(dfs4, dfcConfig.Password, strlen(dfcConfig.Password), 0);
		if (sentAuth == 0)
			printf("Failure to send Password authorization to Server 4\n");
		
		memset(AuthBuf, 0, BUFSIZE);
		sentAuth = read(dfs4, AuthBuf, BUFSIZE);
		printf("%s\n", AuthBuf);
		
		if(!strstr(AuthBuf, "Invalid"))
		{
			send(dfs4, "PUT ", 4, 0);
			switch(dist)
			{
				case 0 ://dfs4(4,1)
					printf("Server 4\n");
					FileNameToSend[strlen(FileNameToSend)-1] = '4';
					printf("%s\n", FileNameToSend);	
					send(dfs4, (char*)&LastMsg, sizeof(LastMsg), 0);
					send(dfs4, LastMsgSize, strlen(LastMsgSize), 0);
					send(dfs4, (char*)&FileNameLength, sizeof(FileNameLength), 0);
					send(dfs4, FileNameToSend, strlen(FileNameToSend), 0);
					send(dfs4, part4, strlen(part4), 0);

					FileNameToSend[strlen(FileNameToSend)-1] = '1';
					printf("%s\n", FileNameToSend);	
					send(dfs4, (char*)&Msg, sizeof(Msg), 0);
					send(dfs4, MsgSize, strlen(MsgSize), 0);
					send(dfs4, (char*)&FileNameLength, sizeof(FileNameLength), 0);
					send(dfs4, FileNameToSend, strlen(FileNameToSend), 0);
					send(dfs4, part1, strlen(part1), 0);

				case 1 ://dfs4(1,2)
					FileNameToSend[strlen(FileNameToSend)-1] = '1';
					send(dfs4, (char*)&Msg, sizeof(Msg), 0);
					send(dfs4, MsgSize, strlen(MsgSize), 0);
					send(dfs4, (char*)&FileNameLength, sizeof(FileNameLength), 0);
					send(dfs4, FileNameToSend, strlen(FileNameToSend), 0);
					send(dfs4, part1, strlen(part1), 0);
					
					FileNameToSend[strlen(FileNameToSend)-1] = '2';
					send(dfs4, (char*)&Msg, sizeof(Msg), 0);
					send(dfs4, MsgSize, strlen(MsgSize), 0);
					send(dfs4, (char*)&FileNameLength, sizeof(FileNameLength), 0);
					send(dfs4, FileNameToSend, strlen(FileNameToSend), 0);
					send(dfs4, part2, strlen(part2), 0);

				case 2 ://dfs4(2,3)
					FileNameToSend[strlen(FileNameToSend)-1] = '2';
					send(dfs4, (char*)&Msg, sizeof(Msg), 0);
					send(dfs4, MsgSize, strlen(MsgSize), 0);
					send(dfs4, (char*)&FileNameLength, sizeof(FileNameLength), 0);
					send(dfs4, FileNameToSend, strlen(FileNameToSend), 0);
					send(dfs4, part2, strlen(part2), 0);
					
					FileNameToSend[strlen(FileNameToSend)-1] = '3';
					send(dfs4, (char*)&Msg, sizeof(Msg), 0);
					send(dfs4, MsgSize, strlen(MsgSize), 0);
					send(dfs4, (char*)&FileNameLength, sizeof(FileNameLength), 0);
					send(dfs4, FileNameToSend, strlen(FileNameToSend), 0);
					send(dfs4, part3, strlen(part3), 0);

				case 3 ://dfs4(3,4)
					FileNameToSend[strlen(FileNameToSend)-1] = '3';
					send(dfs4, (char*)&Msg, sizeof(Msg), 0);
					send(dfs4, MsgSize, strlen(MsgSize), 0);
					send(dfs4, (char*)&FileNameLength, sizeof(FileNameLength), 0);
					send(dfs4, FileNameToSend, strlen(FileNameToSend), 0);
					send(dfs4, part3, strlen(part3), 0);
					
					FileNameToSend[strlen(FileNameToSend)-1] = '4';
					send(dfs4, (char*)&LastMsg, sizeof(LastMsg), 0);
					send(dfs4, LastMsgSize, strlen(LastMsgSize), 0);
					send(dfs4, (char*)&FileNameLength, sizeof(FileNameLength), 0);
					send(dfs4, FileNameToSend, strlen(FileNameToSend), 0);
					send(dfs4, part4, strlen(part4), 0);		
			}
		}
		close(dfs4);
	}
	printf("DFS1 %d\n", dfs1);
	printf("DFS2 %d\n", dfs2);
	printf("DFS3 %d\n", dfs3);
	printf("DFS4 %d\n", dfs4);

	free(part1);
	free(part2);
	free(part3);
	free(part4);

}

void LIST(struct conf dfcConfig)
{

/*----------------------Server 1--------------------------------*/

/*----------------------Server 1--------------------------------*/
	
/*----------------------Server 3--------------------------------*/
	
/*----------------------Server 4--------------------------------*/

/*------------Compare the number of files-----------------------*/

}

char* XOR(char *string, char *key)
{
	int length = strlen(key);
	int i = 0;
	while(i < strlen(string))
	{
		string[i] = string[i] ^ key[i % length];
		i++;
	}
	return string;
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