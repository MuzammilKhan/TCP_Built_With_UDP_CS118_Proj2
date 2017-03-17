SHELL := /bin/bash

CC=gcc
#to simply build executable
default:
	gcc server.c -o server
	gcc client.c -o client
#deletes files creted in compilation and other processes
client:
	gcc client.c -o client

server:
	gcc server.c -o server
clean:
	shopt -s extglob; \
	`rm -f client server `

dist:
	tar -czvf project2_104332038_204282448.tar  Makefile README report.pdf client.c  server.c