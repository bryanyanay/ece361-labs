#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <time.h>

#define MAXBUFLEN 1500

struct packet {
    unsigned int total_frag;
    unsigned int frag_no;
    unsigned int size;
    char *filename;
    char filedata[1000];
};

void sendMsg(int sockfd, const void *msg, size_t len, struct addrinfo *ai) {
    int numbytes;
    numbytes = sendto(sockfd, msg, len, 0, ai->ai_addr, ai->ai_addrlen);

    if (numbytes == -1) {
        perror("sendto");
        exit(1);
    }
}

int recvMsg(int sockfd, char *recv_buf) {
    int numbytes;
    struct sockaddr_storage serv_addr;
    socklen_t serv_addr_len = sizeof serv_addr;
    numbytes = recvfrom(sockfd, recv_buf, MAXBUFLEN - 1, 0, (struct sockaddr *) &serv_addr, &serv_addr_len);
    if (numbytes == -1) {
        perror("recvfrom");
        exit(1);
    }

    return numbytes;
}

double get_time_diff(struct timespec start, struct timespec end) {
    return (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec) / 1e9;
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: deliver <server address> <server port number>\n");
        return 1;
    }

    int status;
    struct addrinfo hints, *servinfo, *curr;
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;

    if ((status = getaddrinfo(argv[1], argv[2], &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo error: %s\n", gai_strerror(status));
        exit(1);
    }

    int sockfd;
    for (curr = servinfo; curr != NULL; curr = curr->ai_next) {
        sockfd = socket(curr->ai_family, curr->ai_socktype, curr->ai_protocol);
        if (sockfd == -1) {
            perror("socket");
            continue;
        }
        break;
    }

    if (curr == NULL) {
        fprintf(stderr, "client failed to bind socket\n");
        exit(1);
    }

    printf("Input file transfer cmd: ftp <file-name>\n>>> ");
    char input_buf[MAXBUFLEN] = {0};
    if (fgets(input_buf, MAXBUFLEN, stdin) == NULL) {
        fprintf(stderr, "Error reading input.\n");
        freeaddrinfo(servinfo);
        close(sockfd);
        exit(1);
    }
    input_buf[strcspn(input_buf, "\n")] = '\0';

    char *cmd = strtok(input_buf, " ");
    char *filename = strtok(NULL, " ");
    if (!cmd || !filename || (strtok(NULL, " ") != NULL)) {
        fprintf(stderr, "Input must be of form ftp <file-name>.\n");
        freeaddrinfo(servinfo);
        close(sockfd);
        exit(1);
    }

    if (access(filename, F_OK) != 0) {
        fprintf(stderr, "File does not exist.\n");
        freeaddrinfo(servinfo);
        close(sockfd);
        exit(1);
    }

    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start); // Start time

    char *msg = "ftp";
    sendMsg(sockfd, msg, strlen(msg), curr);

    char recv_buf[MAXBUFLEN];
    int numbytes = recvMsg(sockfd, recv_buf);

    clock_gettime(CLOCK_MONOTONIC, &end); // End time

    double rtt = get_time_diff(start, end);
    printf("Round-Trip Time: %.6f seconds\n", rtt);

    if (strcmp("yes", recv_buf) == 0) {
        printf("A file transfer can start.\n");
    }

    freeaddrinfo(servinfo);
    close(sockfd);

    return 0;
}
