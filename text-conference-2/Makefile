CC = gcc
CFLAGS = -Wall -Wextra -g
SRC = client.c server.c message.c
OBJ = $(SRC:.c=.o)
TARGETS = client server

all: $(TARGETS)

client: client.o message.o
	$(CC) $(CFLAGS) -o client client.o message.o

server: server.o message.o
	$(CC) $(CFLAGS) -o server server.o message.o

%.o: %.c message.h
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(TARGETS) *.o
