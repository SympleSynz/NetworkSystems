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
struct listFiles
{
	char filename[BUFSIZE];
	int Parts[5];
};
struct conf parseConfig(char *filename);
int hashDist(const char *filename);
int	errexit(const char *format, ...);
int	connectsock(const char *host, const char *portnum);
char* XOR(char *string, char *key);
void GET(struct conf dfcConfig, const char *filename, const char *subfolder);
void PUT(struct conf dfcConfig, const char *filename, const char *subfolder);
void LIST(struct conf dfcConfig, const char *subfolder);

int main(int argc, char* argv[])
{
	struct conf dfcConfig;
	char *buf = calloc(BUFSIZE, 1);
	char *string, *line, *token, *request, *subfolder;
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
				else if (count == 1)
					filename = token;
				else
					subfolder = token;
				count++;
			}
			count = 0;
		}
		if (!strstr(subfolder, "/"))
			subfolder = "/";
		if (strcmp(request, "GET") == 0)
		{
			if(strcmp(filename, "\0") != 0 || !strstr(filename, "/"))
				GET(dfcConfig, filename, subfolder);
			else
				printf("No filename given\n");	
		}
		else if (strcmp(request, "PUT") == 0)
		{	
			if(strcmp(filename, "\0") != 0 || !strstr(filename, "/"))
				PUT(dfcConfig, filename, subfolder);
			else
				printf("No filename given\n");
		}
		else if (strcmp(request, "LIST") == 0)
		{
			if (strstr(filename, "/"))
				subfolder = filename;
			LIST(dfcConfig, subfolder);
		}
		else
			printf("Invalid request\n");
		filename = "\0";
		subfolder = "\0";
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

void GET(struct conf dfcConfig, const char *filename, const char *subfolder)
{	
	char *AuthBuf = calloc(BUFSIZE, 1);
	char *S1Content1, *S1Content2, *S2Content1, *S2Content2, *S3Content1, *S3Content2, *S4Content1, *S4Content2, *MsgSize;
	char ServerPieces[1];
	int *Hashing = calloc(5, sizeof(int));
	char *Hash = calloc(8, 1);
	char Root[50];
	char *host = "localhost";
	FILE *fp;
	int sentAuth, filenameSize, hashType, PieceSize, bytes, ContentSize, dfs1, dfs2, dfs3, dfs4;
	int folderNameSize = strlen(subfolder);

	filenameSize = strlen(filename);
	Hashing[0] = 0;
	Hashing[1] = 0;
	Hashing[2] = 0;
	Hashing[3] = 0;
	Hashing[4] = 0;			
/*----------------------Server 1--------------------------------*/
	dfs1 = connectsock(host, dfcConfig.DFS1);
	sentAuth = send(dfs1, dfcConfig.Username, strlen(dfcConfig.Username), 0);
	if (sentAuth == 0)
		printf("Failure to send Username authorization to Server 1\n");
	
	sentAuth = read(dfs1, AuthBuf, BUFSIZE);
	printf("Server 1 %s\n", AuthBuf);

	if(!strstr(AuthBuf, "Invalid"))
	{
		sentAuth = send(dfs1, dfcConfig.Password, strlen(dfcConfig.Password), 0);
		if (sentAuth == 0)
			printf("Failure to send Password authorization to Server 1\n");
		
		memset(AuthBuf, 0, BUFSIZE);
		sentAuth = read(dfs1, AuthBuf, BUFSIZE);
		printf("Server 1 %s\n", AuthBuf);
		
		if(!strstr(AuthBuf, "Invalid"))
		{
			send(dfs1, "GET ", 4, 0);
			send(dfs1, (char*)& folderNameSize, sizeof(folderNameSize), 0);
			send(dfs1, subfolder, folderNameSize, 0);
			send(dfs1, (char*)&filenameSize, sizeof(filenameSize), 0);
			send(dfs1, filename, strlen(filename), 0);

			bytes = read(dfs1, ServerPieces, 1);
			if (bytes < 0)
				printf("Error reading in first Piece number\n");
			Hash[0] = ServerPieces[0];
			if (strcmp(ServerPieces, "0") != 0)
			{
				Hashing[atoi(ServerPieces)] = 1;
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
			}
			bytes = read(dfs1, ServerPieces, 1);
			if (bytes < 0)
				printf("Error reading in second Piece number\n");
			Hash[1] = ServerPieces[0];
			if (strcmp(ServerPieces, "0") != 0)
			{
				Hashing[atoi(ServerPieces)] = 1;
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
			}
		}
	}
/*----------------------Server 2--------------------------------*/
	dfs2 = connectsock(host, dfcConfig.DFS2);
	sentAuth = send(dfs2, dfcConfig.Username, strlen(dfcConfig.Username), 0);
	if (sentAuth == 0)
		printf("Failure to send Username authorization to Server 1\n");
	
	memset(AuthBuf, 0, BUFSIZE);
	sentAuth = read(dfs2, AuthBuf, BUFSIZE);
	printf("Server 2 %s\n", AuthBuf);

	if(!strstr(AuthBuf, "Invalid"))
	{
		sentAuth = send(dfs2, dfcConfig.Password, strlen(dfcConfig.Password), 0);
		if (sentAuth == 0)
			printf("Failure to send Password authorization to Server 1\n");
		
		memset(AuthBuf, 0, BUFSIZE);
		sentAuth = read(dfs2, AuthBuf, BUFSIZE);
		printf("Server 2 %s\n", AuthBuf);
		
		if(!strstr(AuthBuf, "Invalid"))
		{
			send(dfs2, "GET ", 4, 0);
			send(dfs2, (char*)& folderNameSize, sizeof(folderNameSize), 0);
			send(dfs2, subfolder, folderNameSize, 0);
			send(dfs2, (char*)&filenameSize, sizeof(filenameSize), 0);
			send(dfs2, filename, strlen(filename), 0);

			bytes = read(dfs2, ServerPieces, 1);
			if (bytes < 0)
				printf("Error reading in first Piece number\n");
			Hash[2] = ServerPieces[0];
			if (strcmp(ServerPieces, "0") != 0)
			{
				Hashing[atoi(ServerPieces)] = 1;
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
			}

			bytes = read(dfs2, ServerPieces, 1);
			if (bytes < 0)
				printf("Error reading in second Piece number\n");
			Hash[3] = ServerPieces[0];
			if (strcmp(ServerPieces, "0") != 0)
			{
				Hashing[atoi(ServerPieces)] = 1;
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
	}			
/*----------------------Server 3--------------------------------*/
	dfs3 = connectsock(host, dfcConfig.DFS3);
	sentAuth = send(dfs3, dfcConfig.Username, strlen(dfcConfig.Username), 0);
	if (sentAuth == 0)
		printf("Failure to send Username authorization to Server 1\n");
	
	memset(AuthBuf, 0, BUFSIZE);
	sentAuth = read(dfs3, AuthBuf, BUFSIZE);
	printf("Server 3 %s\n", AuthBuf);

	if(!strstr(AuthBuf, "Invalid"))
	{
		sentAuth = send(dfs3, dfcConfig.Password, strlen(dfcConfig.Password), 0);
		if (sentAuth == 0)
			printf("Failure to send Password authorization to Server 1\n");
		
		memset(AuthBuf, 0, BUFSIZE);
		sentAuth = read(dfs3, AuthBuf, BUFSIZE);
		printf("Server 3 %s\n", AuthBuf);
		
		if(!strstr(AuthBuf, "Invalid"))
		{
			send(dfs3, "GET ", 4, 0);
			send(dfs3, (char*)& folderNameSize, sizeof(folderNameSize), 0);
			send(dfs3, subfolder, folderNameSize, 0);
			send(dfs3, (char*)&filenameSize, sizeof(filenameSize), 0);
			send(dfs3, filename, strlen(filename), 0);

			bytes = read(dfs3, ServerPieces, 1);
			if (bytes < 0)
				printf("Error reading in first Piece number\n");
			Hash[4] = ServerPieces[0];
			if (strcmp(ServerPieces, "0") != 0)
			{
				Hashing[atoi(ServerPieces)] = 1;
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
			}
			bytes = read(dfs3, ServerPieces, 1);
			if (bytes < 0)
				printf("Error reading in second Piece number\n");
			Hash[5] = ServerPieces[0];
			if (strcmp(ServerPieces, "0") != 0)
			{
				Hashing[atoi(ServerPieces)] = 1;
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
	}			
/*----------------------Server 4--------------------------------*/
	dfs4 = connectsock(host, dfcConfig.DFS4);
	sentAuth = send(dfs4, dfcConfig.Username, strlen(dfcConfig.Username), 0);
	if (sentAuth == 0)
		printf("Failure to send Username authorization to Server 1\n");
	
	memset(AuthBuf, 0, BUFSIZE);	
	sentAuth = read(dfs4, AuthBuf, BUFSIZE);
	printf("Server 4 %s\n", AuthBuf);

	if(!strstr(AuthBuf, "Invalid"))
	{
		sentAuth = send(dfs4, dfcConfig.Password, strlen(dfcConfig.Password), 0);
		if (sentAuth == 0)
			printf("Failure to send Password authorization to Server 1\n");
		
		memset(AuthBuf, 0, BUFSIZE);
		sentAuth = read(dfs4, AuthBuf, BUFSIZE);
		printf("Server 4 %s\n", AuthBuf);
		
		if(!strstr(AuthBuf, "Invalid"))
		{
			send(dfs4, "GET ", 4, 0);
			send(dfs4, (char*)& folderNameSize, sizeof(folderNameSize), 0);
			send(dfs4, subfolder, folderNameSize, 0);
			send(dfs4, (char*)&filenameSize, sizeof(filenameSize), 0);
			send(dfs4, filename, strlen(filename), 0);

			bytes = read(dfs4, ServerPieces, 1);
			if (bytes < 0)
				printf("Error reading in first Piece number\n");
			Hash[6] = ServerPieces[0];
			if (strcmp(ServerPieces, "0") != 0)
			{
				Hashing[atoi(ServerPieces)] = 1;
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
			}

			bytes = read(dfs4, ServerPieces, 1);
			if (bytes < 0)
				printf("Error reading in second Piece number\n");
			Hash[7] = ServerPieces[0];
			if (strcmp(ServerPieces, "0") != 0)
			{
				Hashing[atoi(ServerPieces)] = 1;
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
	}

	if(Hashing[1] == 0 || Hashing[2] == 0 || Hashing[3] == 0 || Hashing[4] == 0)
	{
		printf("%s File is incomplete\n", filename);
		free(AuthBuf);
		free(Hashing);
		free(Hash);
		return;
	}
/*------------Put files back together and write to file---------*/	
	strcpy(Root, "./Client/");
	strcat(Root, filename);
	fp = fopen(Root, "w");
	
	if ((Hash[0] == '1' && Hash[1] == '2') || (Hash[2] == '2' && Hash[3] == '3') || (Hash[4] == '3' && Hash[5] == '4') || (Hash[6] == '1' && Hash[7] == '4'))
		hashType = 0;
	else if ((Hash[0] == '2' && Hash[1] == '3') || (Hash[2] == '3' && Hash[3] == '4') || (Hash[4] == '1' && Hash[5] == '4') || (Hash[6] == '1' && Hash[7] == '2'))
		hashType = 1;
	else if ((Hash[0] == '3' && Hash[1] == '4') || (Hash[2] == '1' && Hash[3] == '4') || (Hash[4] == '1' && Hash[5] == '2') || (Hash[6] == '2' && Hash[7] == '3'))
		hashType = 2;
	else
		hashType = 3;

	char *TotalContent = calloc(strlen(S1Content1)+strlen(S1Content2)+strlen(S2Content1)+strlen(S2Content2)+strlen(S3Content1)+strlen(S3Content2)+strlen(S4Content1)+strlen(S4Content2), 1);
	switch(hashType)
	{
		case 0 : //(1,2)(2,3)(3,4)(1,4)
			if (strlen(S1Content1) <= strlen(S4Content1))
				strcpy(TotalContent, S4Content1);
			else
				strcpy(TotalContent, S1Content1);
			if (strlen(S1Content2) <= strlen(S2Content1))
				strcpy(TotalContent+strlen(TotalContent), S2Content1);
			else
				strcpy(TotalContent+strlen(TotalContent), S1Content2);
			if (strlen(S2Content2) <= strlen(S3Content1))
				strcpy(TotalContent+strlen(TotalContent), S3Content1);
			else
				strcpy(TotalContent+strlen(TotalContent), S2Content2);
			if (strlen(S3Content2) <= strlen(S4Content2))
				strcpy(TotalContent+strlen(TotalContent), S4Content2);
			else
				strcpy(TotalContent+strlen(TotalContent), S3Content2);
			break;
		case 1 : //(2,3)(3,4)(1,4)(1,2)
			if (strlen(S3Content1) <= strlen(S4Content1))
				strcpy(TotalContent, S4Content1);
			else
				strcpy(TotalContent, S3Content1);
			if (strlen(S4Content2) <= strlen(S1Content1))
				strcpy(TotalContent+strlen(TotalContent), S1Content1);
			else
				strcpy(TotalContent+strlen(TotalContent), S4Content2);
			if (strlen(S1Content2) <= strlen(S2Content1))
				strcpy(TotalContent+strlen(TotalContent), S2Content1);
			else
				strcpy(TotalContent+strlen(TotalContent), S3Content2);
			if (strlen(S2Content2) <= strlen(S3Content2))
				strcpy(TotalContent+strlen(TotalContent), S3Content2);
			else
				strcpy(TotalContent+strlen(TotalContent), S2Content2);
			break;
		case 2 : //(3,4)(1,4)(1,2)(2,3)
			if (strlen(S2Content1) <= strlen(S3Content1))
				strcpy(TotalContent, S3Content1);
			else
				strcpy(TotalContent, S2Content1);
			if (strlen(S3Content2) <= strlen(S4Content1))
				strcpy(TotalContent+strlen(TotalContent), S4Content1);
			else
				strcpy(TotalContent+strlen(TotalContent), S3Content2);
			if (strlen(S4Content2) <= strlen(S1Content1))
				strcpy(TotalContent+strlen(TotalContent), S1Content1);
			else
				strcpy(TotalContent+strlen(TotalContent), S4Content2);
			if (strlen(S1Content2) <= strlen(S2Content2))
				strcpy(TotalContent+strlen(TotalContent), S2Content2);
			else
				strcpy(TotalContent+strlen(TotalContent), S1Content2);
			break;
		case 3 : //(1,4)(1,2)(2,3)(3,4)
			if (strlen(S1Content1) <= strlen(S2Content1))
				strcpy(TotalContent, S2Content1);
			else
				strcpy(TotalContent, S1Content1);
			if (strlen(S2Content2) <= strlen(S3Content1))
				strcpy(TotalContent+strlen(TotalContent), S3Content1);
			else
				strcpy(TotalContent+strlen(TotalContent), S2Content2);
			if (strlen(S3Content2) <= strlen(S4Content1))
				strcpy(TotalContent+strlen(TotalContent), S4Content1);
			else
				strcpy(TotalContent+strlen(TotalContent), S3Content2);
			if (strlen(S1Content2) <= strlen(S4Content2))
				strcpy(TotalContent+strlen(TotalContent), S4Content2);
			else
				strcpy(TotalContent+strlen(TotalContent), S1Content2);
			break;
	}
	TotalContent = XOR(TotalContent, dfcConfig.Password);
	fprintf(fp, "%s", TotalContent);
	fclose(fp);
	free(S1Content1);
	free(S1Content2);
	free(S2Content1);
	free(S2Content2);
	free(S3Content1);
	free(S3Content2);
	free(S4Content1);
	free(S4Content2);
	free(TotalContent);
	free(AuthBuf);
	free(Hashing);
	free(Hash);
}

void PUT(struct conf dfcConfig, const char *filename, const char *subfolder)
{
	struct stat st;
	char root[50];
	char FileNameToSend[50];
	char MsgSize[15];
	char LastMsgSize[15];
	char *AuthBuf = calloc(BUFSIZE, 1);
	int PieceSize, dist, LastSize, sentAuth, dfs1, dfs2, dfs3, dfs4;
	int folderNameSize = strlen(subfolder);
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
	int BigSize = st.st_size*2;
	char *OrigFile = calloc(BigSize, 1);
	fread(OrigFile, 1, st.st_size, fp);
	OrigFile = XOR(OrigFile, dfcConfig.Password);

	PieceSize = strlen(OrigFile)/4 + 1;
	LastSize = PieceSize + 4;
    
    char *part1 = calloc(PieceSize, 1);
    char *part2 = calloc(PieceSize, 1);
    char *part3 = calloc(PieceSize, 1);
    char *part4 = calloc(LastSize, 1);
    
    strncpy(part1, OrigFile, PieceSize);
    strncpy(part2, OrigFile+PieceSize, PieceSize);
    strncpy(part3, OrigFile+(PieceSize*2), PieceSize);
    strncpy(part4, OrigFile+(PieceSize*3), PieceSize);

    fclose(fp);

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
	printf("Server 1 %s\n", AuthBuf);

	if(!strstr(AuthBuf, "Invalid"))
	{
		sentAuth = send(dfs1, dfcConfig.Password, strlen(dfcConfig.Password), 0);
		if (sentAuth == 0)
			printf("Failure to send Password authorization to Server 1\n");
		memset(AuthBuf, 0, BUFSIZE);
		sentAuth = read(dfs1, AuthBuf, BUFSIZE);
		printf("Server 1 %s\n", AuthBuf);
		
		if(!strstr(AuthBuf, "Invalid"))
		{
			send(dfs1, "PUT ", 4, 0);
			send(dfs1, (char*)& folderNameSize, sizeof(folderNameSize), 0);
			send(dfs1, subfolder, folderNameSize, 0);
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
					break;

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
					break;

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
					break;

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
					break;
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
	printf("Server 2 %s\n", AuthBuf);

	if(!strstr(AuthBuf, "Invalid"))
	{
		sentAuth = send(dfs2, dfcConfig.Password, strlen(dfcConfig.Password), 0);
		if (sentAuth == 0)
			printf("Failure to send Password authorization to Server 2\n");
		
		memset(AuthBuf, 0, BUFSIZE);
		sentAuth = read(dfs2, AuthBuf, BUFSIZE);
		printf("Server 2 %s\n", AuthBuf);

		if(!strstr(AuthBuf, "Invalid"))
		{
			send(dfs2, "PUT ", 4, 0);
			send(dfs2, (char*)& folderNameSize, sizeof(folderNameSize), 0);
			send(dfs2, subfolder, folderNameSize, 0);
			switch(dist)
			{
				case 0 ://dfs2 (2,3)
					FileNameToSend[strlen(FileNameToSend)-1] = '2';		
					send(dfs2, (char*)&Msg, sizeof(Msg), 0);
					send(dfs2, MsgSize, strlen(MsgSize), 0);
					send(dfs2, (char*)&FileNameLength, sizeof(FileNameLength), 0);
					send(dfs2, FileNameToSend, strlen(FileNameToSend), 0);
					send(dfs2, part2, strlen(part2), 0);
					
					FileNameToSend[strlen(FileNameToSend)-1] = '3';	
					send(dfs2, (char*)&Msg, sizeof(Msg), 0);
					send(dfs2, MsgSize, strlen(MsgSize), 0);
					send(dfs2, (char*)&FileNameLength, sizeof(FileNameLength), 0);
					send(dfs2, FileNameToSend, strlen(FileNameToSend), 0);
					send(dfs2, part3, strlen(part3), 0);
					break;

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
					break;

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
					break;

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
					break;
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
	printf("Server 3 %s\n", AuthBuf);
	if(!strstr(AuthBuf, "Invalid"))
	{
		sentAuth = send(dfs3, dfcConfig.Password, strlen(dfcConfig.Password), 0);
		if (sentAuth == 0)
			printf("Failure to send Password authorization to Server 3\n");
		
		memset(AuthBuf, 0, BUFSIZE);
		sentAuth = read(dfs3, AuthBuf, BUFSIZE);
		printf("Server 3 %s\n", AuthBuf);
		
		if(!strstr(AuthBuf, "Invalid"))
		{
			send(dfs3, "PUT ", 4, 0);
			send(dfs3, (char*)& folderNameSize, sizeof(folderNameSize), 0);
			send(dfs3, subfolder, folderNameSize, 0);
			switch(dist)
			{
				case 0 ://dfs3 (3,4)
					FileNameToSend[strlen(FileNameToSend)-1] = '3';	
					send(dfs3, (char*)&Msg, sizeof(Msg), 0);
					send(dfs3, MsgSize, strlen(MsgSize), 0);
					send(dfs3, (char*)&FileNameLength, sizeof(FileNameLength), 0);
					send(dfs3, FileNameToSend, strlen(FileNameToSend), 0);
					send(dfs3, part3, strlen(part3), 0);
					
					FileNameToSend[strlen(FileNameToSend)-1] = '4';
					send(dfs3, (char*)&LastMsg, sizeof(LastMsg), 0);
					send(dfs3, LastMsgSize, strlen(LastMsgSize), 0);
					send(dfs3, (char*)&FileNameLength, sizeof(FileNameLength), 0);
					send(dfs3, FileNameToSend, strlen(FileNameToSend), 0);
					send(dfs3, part4, strlen(part4), 0);
					break;

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
					break;

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
					break;

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
					break;
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
	printf("Server 4 %s\n", AuthBuf);
	if(!strstr(AuthBuf, "Invalid"))
	{
		sentAuth = send(dfs4, dfcConfig.Password, strlen(dfcConfig.Password), 0);
		if (sentAuth == 0)
			printf("Failure to send Password authorization to Server 4\n");
		
		memset(AuthBuf, 0, BUFSIZE);
		sentAuth = read(dfs4, AuthBuf, BUFSIZE);
		printf("Server 4 %s\n", AuthBuf);
		
		if(!strstr(AuthBuf, "Invalid"))
		{
			send(dfs4, "PUT ", 4, 0);
			send(dfs4, (char*)& folderNameSize, sizeof(folderNameSize), 0);
			send(dfs4, subfolder, folderNameSize, 0);
			switch(dist)
			{
				case 0 ://dfs4(4,1)
					FileNameToSend[strlen(FileNameToSend)-1] = '4';
					send(dfs4, (char*)&LastMsg, sizeof(LastMsg), 0);
					send(dfs4, LastMsgSize, strlen(LastMsgSize), 0);
					send(dfs4, (char*)&FileNameLength, sizeof(FileNameLength), 0);
					send(dfs4, FileNameToSend, strlen(FileNameToSend), 0);
					send(dfs4, part4, strlen(part4), 0);

					FileNameToSend[strlen(FileNameToSend)-1] = '1';
					send(dfs4, (char*)&Msg, sizeof(Msg), 0);
					send(dfs4, MsgSize, strlen(MsgSize), 0);
					send(dfs4, (char*)&FileNameLength, sizeof(FileNameLength), 0);
					send(dfs4, FileNameToSend, strlen(FileNameToSend), 0);
					send(dfs4, part1, strlen(part1), 0);
					break;

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
					break;

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
					break;

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
					break;		
			}
		}
		close(dfs4);
	}

	free(part1);
	free(part2);
	free(part3);
	free(part4);
	free(AuthBuf);
	free(OrigFile);
}

void LIST(struct conf dfcConfig, const char *subfolder)
{
	struct listFiles *Server1;
	struct listFiles *Server2;
	struct listFiles *Server3;
	struct listFiles *Server4;
	char *AuthBuf = calloc(BUFSIZE, 1);
	int bytes, sentAuth, dfs1, dfs2, dfs3, dfs4, index, filenameSize;
	int files1 = 0;
	int files2 = 0;
	int files3 = 0;
	int files4 = 0;
	int cont = 0;
	char *host = "localhost";
	int folderNameSize = strlen(subfolder);

/*----------------------Server 1--------------------------------*/
	dfs1 = connectsock(host, dfcConfig.DFS1);
	sentAuth = send(dfs1, dfcConfig.Username, strlen(dfcConfig.Username), 0);
	if (sentAuth == 0)
		printf("Failure to send Username authorization to Server 1\n");
	
	sentAuth = read(dfs1, AuthBuf, BUFSIZE);
	printf("Server 1 %s\n", AuthBuf);

	if(!strstr(AuthBuf, "Invalid"))
	{
		sentAuth = send(dfs1, dfcConfig.Password, strlen(dfcConfig.Password), 0);
		if (sentAuth == 0)
			printf("Failure to send Password authorization to Server 1\n");
		memset(AuthBuf, 0, BUFSIZE);
		sentAuth = read(dfs1, AuthBuf, BUFSIZE);
		printf("Server 1%s\n", AuthBuf);
		
		if(!strstr(AuthBuf, "Invalid"))
		{
			send(dfs1, "LIST", 4, 0);
			send(dfs1, (char*)& folderNameSize, sizeof(folderNameSize), 0);
			send(dfs1, subfolder, folderNameSize, 0);
			bytes = read(dfs1, (char*)&cont, sizeof(cont));
			if (bytes < 0)
				printf("Error reading if valid directory on Server 1\n");
			if (cont == 1)
			{
				bytes = read(dfs1, (char*)&files1, sizeof(files1));
				if (bytes < 0)
					printf("Error reading in the number of files on Server 1\n");
				Server1 = calloc(files1, sizeof(Server1));
				int i = 0;
				while (i < files1)
				{
					strcpy(Server1[i].filename, "");
					Server1[i].Parts[0] = 0;
					Server1[i].Parts[1] = 0;
					Server1[i].Parts[2] = 0;
					Server1[i].Parts[3] = 0;
					Server1[i].Parts[4] = 0;
					i++;
				}

				index = 0;
				while (index < files1)
				{
					bytes = read(dfs1, (char*)&filenameSize, sizeof(filenameSize));
					if (bytes < 0)
						printf("Error reading in the size of the file name on Server 1\n");
					
					char filename[filenameSize];
					bytes = read(dfs1, filename, filenameSize);
					if (bytes < 0)
						printf("Error reading in the file name on Server 1\n");
					
					int pt;
					if (filename[filenameSize - 1] == '1')
						pt = 1;
					else if (filename[filenameSize - 1] == '2')
						pt = 2;
					else if (filename[filenameSize - 1] == '3')
						pt = 3;
					else
						pt = 4;

					filename[filenameSize - 2] = '\0';
					char* name = filename;
					name++;

					i = 0;
					while (i <= index)
					{
						if (strcmp(Server1[i].filename, "") == 0)
						{
							strcpy(Server1[i].filename, name);
							Server1[i].Parts[pt] = 1;
							break;
						}
						else if (strcmp(Server1[i].filename, name) == 0)
						{
							Server1[i].Parts[pt] = 1;
							break;
						}
						else
							i++;
					}
					index++;
				}
			}
			else
				printf("Invalid directory on Server 1\n");
		}
	}
/*----------------------Server 2--------------------------------*/
	dfs2 = connectsock(host, dfcConfig.DFS2);
	sentAuth = send(dfs2, dfcConfig.Username, strlen(dfcConfig.Username), 0);
	if (sentAuth == 0)
		printf("Failure to send Username authorization to Server 2\n");
	
	sentAuth = read(dfs2, AuthBuf, BUFSIZE);
	printf("Server 2 %s\n", AuthBuf);

	if(!strstr(AuthBuf, "Invalid"))
	{
		sentAuth = send(dfs2, dfcConfig.Password, strlen(dfcConfig.Password), 0);
		if (sentAuth == 0)
			printf("Failure to send Password authorization to Server 2\n");
		memset(AuthBuf, 0, BUFSIZE);
		sentAuth = read(dfs2, AuthBuf, BUFSIZE);
		printf("Server 2 %s\n", AuthBuf);
		
		if(!strstr(AuthBuf, "Invalid"))
		{
			send(dfs2, "LIST", 4, 0);
			send(dfs2, (char*)& folderNameSize, sizeof(folderNameSize), 0);
			send(dfs2, subfolder, folderNameSize, 0);
			bytes = read(dfs2, (char*)&cont, sizeof(cont));
			if (bytes < 0)
				printf("Error reading if valid directory on Server 2\n");
			if (cont == 1)
			{
				bytes = read(dfs2, (char*)&files2, sizeof(files2));
				if (bytes < 0)
					printf("Error reading in the number of files on Server 2\n");
				Server2 = calloc(files2, sizeof(Server2));
				int i = 0;
				while (i < files2)
				{
					strcpy(Server2[i].filename, "");
					Server2[i].Parts[0] = 0;
					Server2[i].Parts[1] = 0;
					Server2[i].Parts[2] = 0;
					Server2[i].Parts[3] = 0;
					Server2[i].Parts[4] = 0;
					i++;
				}

				index = 0;
				while (index < files2)
				{
					bytes = read(dfs2, (char*)&filenameSize, sizeof(filenameSize));
					if (bytes < 0)
						printf("Error reading in the size of the file name on Server 2\n");
					
					char filename[filenameSize];
					bytes = read(dfs2, filename, filenameSize);
					if (bytes < 0)
						printf("Error reading in the file name on Server 2\n");
					
					int pt;
					if (filename[filenameSize - 1] == '1')
						pt = 1;
					else if (filename[filenameSize - 1] == '2')
						pt = 2;
					else if (filename[filenameSize - 1] == '3')
						pt = 3;
					else
						pt = 4;

					filename[filenameSize - 2] = '\0';
					char* name = filename;
					name++;

					i = 0;
					while (i <= index)
					{
						if (strcmp(Server2[i].filename, "") == 0)
						{
							strcpy(Server2[i].filename, name);
							Server2[i].Parts[pt] = 1;
							break;
						}
						else if (strcmp(Server2[i].filename, name) == 0)
						{
							Server2[i].Parts[pt] = 1;
							break;
						}
						else
							i++;
					}
					index++;
				}	
			}
			else
				printf("Invalid directory on Server 2\n");
		}
	}
/*----------------------Server 3--------------------------------*/
	dfs3 = connectsock(host, dfcConfig.DFS3);
	sentAuth = send(dfs3, dfcConfig.Username, strlen(dfcConfig.Username), 0);
	if (sentAuth == 0)
		printf("Failure to send Username authorization to Server 3\n");
	
	sentAuth = read(dfs3, AuthBuf, BUFSIZE);
	printf("Server 3 %s\n", AuthBuf);

	if(!strstr(AuthBuf, "Invalid"))
	{
		sentAuth = send(dfs3, dfcConfig.Password, strlen(dfcConfig.Password), 0);
		if (sentAuth == 0)
			printf("Failure to send Password authorization to Server 3\n");
		memset(AuthBuf, 0, BUFSIZE);
		sentAuth = read(dfs3, AuthBuf, BUFSIZE);
		printf("Server 3 %s\n", AuthBuf);
		
		if(!strstr(AuthBuf, "Invalid"))
		{
			send(dfs3, "LIST", 4, 0);
			send(dfs3, (char*)& folderNameSize, sizeof(folderNameSize), 0);
			send(dfs3, subfolder, folderNameSize, 0);
			bytes = read(dfs3, (char*)&cont, sizeof(cont));
			if (bytes < 0)
				printf("Error reading if valid directory on Server 3\n");
			if (cont == 1)
			{
				bytes = read(dfs3, (char*)&files3, sizeof(files3));
				if (bytes < 0)
					printf("Error reading in the number of files on Server 3\n");
				Server3 = calloc(files3, sizeof(Server3));
				int i = 0;
				while (i < files3)
				{
					strcpy(Server3[i].filename, "");
					Server3[i].Parts[0] = 0;
					Server3[i].Parts[1] = 0;
					Server3[i].Parts[2] = 0;
					Server3[i].Parts[3] = 0;
					Server3[i].Parts[4] = 0;
					i++;
				}

				index = 0;
				while (index < files3)
				{
					bytes = read(dfs3, (char*)&filenameSize, sizeof(filenameSize));
					if (bytes < 0)
						printf("Error reading in the size of the file name on Server 3\n");
					
					char filename[filenameSize];
					bytes = read(dfs3, filename, filenameSize);
					if (bytes < 0)
						printf("Error reading in the file name on Server 3\n");
					
					int pt;
					if (filename[filenameSize - 1] == '1')
						pt = 1;
					else if (filename[filenameSize - 1] == '2')
						pt = 2;
					else if (filename[filenameSize - 1] == '3')
						pt = 3;
					else
						pt = 4;

					filename[filenameSize - 2] = '\0';
					char* name = filename;
					name++;

					i = 0;
					while (i <= index)
					{
						if (strcmp(Server3[i].filename, "") == 0)
						{
							strcpy(Server3[i].filename, name);
							Server3[i].Parts[pt] = 1;
							break;
						}
						else if (strcmp(Server3[i].filename, name) == 0)
						{
							Server3[i].Parts[pt] = 1;
							break;
						}
						else
							i++;
					}
					index++;
				}
			}
			else
				printf("Invalid directory on Server 3\n");
		}	
	}
/*----------------------Server 4--------------------------------*/
	dfs4 = connectsock(host, dfcConfig.DFS4);
	sentAuth = send(dfs4, dfcConfig.Username, strlen(dfcConfig.Username), 0);
	if (sentAuth == 0)
		printf("Failure to send Username authorization to Server 4\n");
	
	sentAuth = read(dfs4, AuthBuf, BUFSIZE);
	printf("Server 4 %s\n", AuthBuf);

	if(!strstr(AuthBuf, "Invalid"))
	{
		sentAuth = send(dfs4, dfcConfig.Password, strlen(dfcConfig.Password), 0);
		if (sentAuth == 0)
			printf("Failure to send Password authorization to Server 4\n");
		memset(AuthBuf, 0, BUFSIZE);
		sentAuth = read(dfs4, AuthBuf, BUFSIZE);
		printf("Server 4 %s\n", AuthBuf);
		
		if(!strstr(AuthBuf, "Invalid"))
		{
			send(dfs4, "LIST", 4, 0);
			send(dfs4, (char*)& folderNameSize, sizeof(folderNameSize), 0);
			send(dfs4, subfolder, folderNameSize, 0);
			bytes = read(dfs4, (char*)&cont, sizeof(cont));
			if (bytes < 0)
				printf("Error reading if valid directory on Server 4\n");
			if (cont == 1)
			{
				bytes = read(dfs4, (char*)&files4, sizeof(files4));
				if (bytes < 0)
					printf("Error reading in the number of files on Server 4\n");
				Server4 = calloc(files4, sizeof(Server4));
				int i = 0;
				while (i < files4)
				{
					strcpy(Server4[i].filename, "");
					Server4[i].Parts[0] = 0;
					Server4[i].Parts[1] = 0;
					Server4[i].Parts[2] = 0;
					Server4[i].Parts[3] = 0;
					Server4[i].Parts[4] = 0;
					i++;
				}

				index = 0;
				while (index < files4)
				{
					bytes = read(dfs4, (char*)&filenameSize, sizeof(filenameSize));
					if (bytes < 0)
						printf("Error reading in the size of the file name on Server 4\n");
					
					char filename[filenameSize];
					bytes = read(dfs4, filename, filenameSize);
					if (bytes < 0)
						printf("Error reading in the file name on Server 4\n");
					
					int pt;
					if (filename[filenameSize - 1] == '1')
						pt = 1;
					else if (filename[filenameSize - 1] == '2')
						pt = 2;
					else if (filename[filenameSize - 1] == '3')
						pt = 3;
					else
						pt = 4;

					filename[filenameSize - 2] = '\0';
					char* name = filename;
					name++;

					i = 0;
					while (i <= index)
					{
						if (strcmp(Server4[i].filename, "") == 0)
						{
							strcpy(Server4[i].filename, name);
							Server4[i].Parts[pt] = 1;
							break;
						}
						else if (strcmp(Server4[i].filename, name) == 0)
						{
							Server4[i].Parts[pt] = 1;
							break;
						}
						else
							i++;
					}
					index++;
				}
			}
			else
				printf("Invalid directory on Server 4\n");
		}
	}
/*------------Compare the number of files-----------------------*/
	if (files1 == 0 && files2 == 0 && files3 == 0 && files4 == 0)
		return;
	int TotalFiles;
	if (files1 == files2 && files2 == files3 && files3 == files4)
		TotalFiles = files1;
	else if (files1 < files2 && files3 < files2 && files4 < files2)
		TotalFiles = files2;
	else if (files1 < files3 && files2 < files3 && files4 < files3)
		TotalFiles = files3;
	else if (files1 < files4 && files2 < files4 && files3 < files4)
		TotalFiles = files4;
	else
		TotalFiles = files1;

	struct listFiles Total[TotalFiles];
	int i = 0;
	while (i < TotalFiles)
	{
		strcpy(Total[i].filename, "");
		Total[i].Parts[0] = 0;
		Total[i].Parts[1] = 0;
		Total[i].Parts[2] = 0;
		Total[i].Parts[3] = 0;
		Total[i].Parts[4] = 0;
		i++;
	}
	index = 0;

	while (index < files1)
	{
		strcpy(Total[index].filename, Server1[index].filename);
		Total[index].Parts[1] = Server1[index].Parts[1];
		Total[index].Parts[2] = Server1[index].Parts[2];
		Total[index].Parts[3] = Server1[index].Parts[3];
		Total[index].Parts[4] = Server1[index].Parts[4];
		index++;
	}

	index = 0;
	while (index < files2)
	{
		if (strcmp(Server2[index].filename, "") == 0)
			break;
		int i = 0;
		while (i < TotalFiles)
		{
			if (strcmp(Total[i].filename, Server2[index].filename) == 0)
			{
				if (Server2[index].Parts[1] == 1 && Total[i].Parts[1] == 0)
					Total[i].Parts[1] = 1;
				if (Server2[index].Parts[2] == 1 && Total[i].Parts[2] == 0)
					Total[i].Parts[2] = 1;
				if (Server2[index].Parts[3] == 1 && Total[i].Parts[3] == 0)
					Total[i].Parts[3] = 1;
				if (Server2[index].Parts[4] == 1 && Total[i].Parts[4] == 0)
					Total[i].Parts[4] = 1;
				break;
			}
			else if (strcmp(Total[i].filename, "") == 0)
			{
				strcpy(Total[i].filename, Server2[index].filename);
				Total[i].Parts[1] = Server2[index].Parts[1];
				Total[i].Parts[2] = Server2[index].Parts[2];
				Total[i].Parts[3] = Server2[index].Parts[3];
				Total[i].Parts[4] = Server2[index].Parts[4];
				break;
			}
			else
				i++;
		}
		index++;
	}

	index = 0;
	while (index < files3)
	{
		if (strcmp(Server3[index].filename, "") == 0)
			break;
		int i = 0;
		while (i < TotalFiles)
		{
			if (strcmp(Total[i].filename, Server3[index].filename) == 0)
			{
				if (Server3[index].Parts[1] == 1 && Total[i].Parts[1] == 0)
					Total[i].Parts[1] = 1;
				if (Server3[index].Parts[2] == 1 && Total[i].Parts[2] == 0)
					Total[i].Parts[2] = 1;
				if (Server3[index].Parts[3] == 1 && Total[i].Parts[3] == 0)
					Total[i].Parts[3] = 1;
				if (Server3[index].Parts[4] == 1 && Total[i].Parts[4] == 0)
					Total[i].Parts[4] = 1;
				break;
			}
			else if (strcmp(Total[i].filename, "") == 0)
			{
				strcpy(Total[i].filename, Server3[index].filename);
				Total[i].Parts[1] = Server3[index].Parts[1];
				Total[i].Parts[2] = Server3[index].Parts[2];
				Total[i].Parts[3] = Server3[index].Parts[3];
				Total[i].Parts[4] = Server3[index].Parts[4];
				break;
			}
			else
				i++;
		}
		index++;
	}
	index = 0;
	while (index < files4)
	{
		if (strcmp(Server4[index].filename, "") == 0)
			break;
		int i = 0;
		while (i < TotalFiles)
		{
			if (strcmp(Total[i].filename, Server4[index].filename) == 0)
			{
				if (Server4[index].Parts[1] == 1 && Total[i].Parts[1] == 0)
					Total[i].Parts[1] = 1;
				if (Server4[index].Parts[2] == 1 && Total[i].Parts[2] == 0)
					Total[i].Parts[2] = 1;
				if (Server4[index].Parts[3] == 1 && Total[i].Parts[3] == 0)
					Total[i].Parts[3] = 1;
				if (Server4[index].Parts[4] == 1 && Total[i].Parts[4] == 0)
					Total[i].Parts[4] = 1;
				break;
			}
			else if (strcmp(Total[i].filename, "") == 0)
			{
				strcpy(Total[i].filename, Server4[index].filename);
				Total[i].Parts[1] = Server4[index].Parts[1];
				Total[i].Parts[2] = Server4[index].Parts[2];
				Total[i].Parts[3] = Server4[index].Parts[3];
				Total[i].Parts[4] = Server4[index].Parts[4];
				break;
			}
			else
				i++;
		}
		index++;
	}
	index = 0;
	printf("LIST\n");
	while (index <= TotalFiles)
	{
		if (strcmp(Total[index].filename, "") == 0)
			break;
		if (Total[index].Parts[1] == 1 && Total[index].Parts[2] == 1 && Total[index].Parts[3] == 1 && Total[index].Parts[4] == 1)
			printf("%s\n", Total[index].filename);
		else
			printf("%s [incomplete]\n", Total[index].filename);
		index++;
	}
	printf("\n");
	free(AuthBuf);
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
