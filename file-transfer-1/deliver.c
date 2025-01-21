#include <stdio.h>
#include <stdlib.h>     
#include <string.h>     
#include <netdb.h>     
#include <arpa/inet.h> 
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>

#define MAXBUFLEN 1500

// credits: some of this code is adapted from beej's handbook, mainly section 6.3

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: deliver <server address> <server port number>\n");
    }

    // POPULATE ADDRINFOS
    int status;
    struct addrinfo hints;
    struct addrinfo *servinfo; // linked list of struct addrinfos

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;

    if ((status = getaddrinfo(argv[1], argv[2], &hints, &servinfo)) != 0) { // non-zero return means error
        fprintf(stderr, "getaddrinfo error: %s\n", gai_strerror(status));
        exit(1);
    }

    // LOOP THRU AND GET FIRST SOCKET WE CAN
    int sockfd;
    struct addrinfo *curr;
    for (curr = servinfo; curr != NULL; curr = curr->ai_next) {
        // get socket descriptor
        sockfd = socket(curr->ai_family, curr->ai_socktype, curr->ai_protocol);
        if (sockfd == -1) { // if error, try the next addrinfo
            perror("socket");
            continue; 
        }
        // no need to bind, client can use any random available port
        break; // if successful for one of the addrinfos, we're good break
    }

    if (curr == NULL) {
        fprintf(stderr, "client failed to bind socket\n");
        exit(1);
    }

    // // QUERY USER
    // printf("Input file transfer cmd: ftp <file-name>\n");
    // char input_buf[MAXBUFLEN] = {0};
    // if (fgets(input_buf, MAXBUFLEN, stdin) == NULL) {
    //     printf("Error reading input.\n");
    //     freeaddrinfo(servinfo);
    //     close(sockfd);
    //     exit(1);
    // }
    // input_buf[strcspn(input_buf, "\n")] = '\0'; // replace the trailing newline with null character


    // char *cmd = strtok(input_buf, " "); // up to first space
    // char *filename = strtok(NULL, " "); // up to 2nd space (should be end of string)
    // if (!cmd || !filename || (strtok(NULL, " ") != NULL)) { // make sure cmd and filename are non-NULL and there is nothing left in the input
    //     printf("Input must be of form ftp <file-name>, with no spaces in the file name.\n");
    //     freeaddrinfo(servinfo);
    //     close(sockfd);
    //     exit(1);
    // }

    // SEND A MESSAGE
    char *msg = "ftp";
    int numbytes;
    numbytes = sendto(sockfd, msg, strlen(msg), 0, curr->ai_addr, curr->ai_addrlen);
    if (numbytes == -1) {
        perror("sendto");
        freeaddrinfo(servinfo);
        close(sockfd);
        exit(1);
    }

    printf(">>> sent message %d bytes long\n", numbytes);

    // RECEIVE SERVER REPLY
    char recv_buf[MAXBUFLEN];
    struct sockaddr_storage serv_addr;
    socklen_t serv_addr_len = sizeof serv_addr;
    numbytes = recvfrom(sockfd, recv_buf, MAXBUFLEN - 1, 0, (struct sockaddr *) &serv_addr, &serv_addr_len);
    if (numbytes == -1) {
        perror("recvfrom");
        exit(1);
    }

    if (strcmp("yes", recv_buf) == 0) {
        printf("A file transfer can start.\n");
    } else {
        freeaddrinfo(servinfo);
        close(sockfd);
        exit(1);
    }

    // don't need it anymore
    freeaddrinfo(servinfo);
    close(sockfd);

    return 0;
}