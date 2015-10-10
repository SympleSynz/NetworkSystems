all: Server Client

Server: Server.c
	gcc -Wall Server.c -o Server -lcrypto -lssl

Client: Client.c
	gcc -Wall Client.c -o Client -lcrypto -lssl

.PHONY: clean

clean:
	rm -f Server
	rm -f Client
