CC = gcc
CFLAGS = -Wall -std=c99
LINKFLAGS = -pthread
RM = rm -f
SERVER_NAME = ftp_srv

all: $(SERVER_NAME)

$(SERVER_NAME): responses.o file_worker.o ftp_srv.o
	$(CC) ftp_srv.o responses.o file_worker.o -o $(SERVER_NAME) $(CFLAGS) $(LINKFLAGS)

ftp_srv.o: constants.h
responses.o: responses.h
file_worker.o: file_worker.h

clean:
	$(RM) *.o
	$(RM) $(SERVER_NAME)

