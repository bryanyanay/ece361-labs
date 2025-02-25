CC = gcc

all: server_dir/server client_dir/deliver

server_dir/server: server.o
	mkdir -p server_dir
	gcc -o server_dir/server server.o

client_dir/deliver: deliver.o
	mkdir -p client_dir
	gcc -o client_dir/deliver deliver.o

server.o: server.c
	gcc -c server.c -o server.o

deliver.o: deliver.c
	gcc -c deliver.c -o deliver.o

clean:
	rm -f server.o deliver.o
	rm -f server_dir/server client_dir/deliver
	# rm -rf server_dir client_dir 
