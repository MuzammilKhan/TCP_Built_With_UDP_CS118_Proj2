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
	`rm -f client server test_server`

dist:
	zip project1_104332038  Makefile README report.pdf webserver.c
