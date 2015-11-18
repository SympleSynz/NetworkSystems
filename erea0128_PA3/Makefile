All: WebProxy proxy

WebProxy: WebProxy.c
	gcc -Wall -o webproxy WebProxy.c -lrt -lcrypto -lssl -lpthread

proxy: proxy.c
	gcc -Wall -o proxy proxy.c -lrt -lpthread

.PHONY: clean

clean:
	rm -f webproxy
	rm -f proxy