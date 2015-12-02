#CSCI 5273 - Network Systems
#Programming Assignment 4

#Compiling
In order to compile, type "make" into the terminal.
user@cu-cs-vm:~/erea0128_PA4$ make

#Cleanup
To clean up the executable, type "make clean" into the terminal.
user@cu-cs-vm:~/erea0128_PA4$ make clean

#Project Description
Creating a webproxy to work with a client VM and server VM. I will be running echo-client on the client VM and echo-server on the server VM. The client VM will send a message to the server VM through the webproxy and vice versa. The webproxy will act as a man in the middle between the client and server.