All: Server Client

Server: dfs.c
	gcc -Wall dfs.c -o dfs -lcrypto -lssl

Client: dfc.c
	gcc -Wall dfc.c -o dfc -lcrypto -lssl

.PHONY: clean

clean:
	rm -f dfs
	rm -f dfc
