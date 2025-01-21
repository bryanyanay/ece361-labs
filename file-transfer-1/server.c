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
    if (argc != 2) {
        fprintf(stderr, "Usage: server <server port number>\n");
    }

    // POPULATE ADDRINFOS
    int status;
    struct addrinfo hints;
    struct addrinfo *servinfo; // point to head of linked list of struct addrinfos

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET; // force ipv4
    hints.ai_socktype = SOCK_DGRAM; // use UDP
    hints.ai_flags = AI_PASSIVE; // i believe ends up listening on wildcard address, i.e., all interfaces (bc hostname/ipaddr specify as NULL)

    if ((status = getaddrinfo(NULL, argv[1], &hints, &servinfo)) != 0) { // non-zero return means error
        fprintf(stderr, "getaddrinfo error: %s\n", gai_strerror(status));
        exit(1);
    }

    // LOOP THRU AND BIND TO GET SOCKET FOR FIRST ONE WE CAN
    int sockfd;
    struct addrinfo *curr;
    for (curr = servinfo; curr != NULL; curr = curr->ai_next) {
        // get socket descriptor
        sockfd = socket(curr->ai_family, curr->ai_socktype, curr->ai_protocol);
        if (sockfd == -1) { // if error, try the next addrinfo
            perror("socket");
            continue; 
        }
        // bind socket descriptor to port
        if (bind(sockfd, curr->ai_addr, curr->ai_addrlen) == -1) {
            close(sockfd);
            perror("bind");
            continue; 
        }
        break; // if successful for one of the addrinfos, we're good break
    }

    if (curr == NULL) {
        fprintf(stderr, "server failed to bind socket\n");
        exit(1);
    }

    // don't need it anymore
    freeaddrinfo(servinfo);

    // START ACCEPTING DATA 
    printf(">>> begin listening...\n");

    while (1) {
        // receive the message
        int numbytes;
        char recv_buf[MAXBUFLEN];
        struct sockaddr_storage client_addr;
        socklen_t client_addr_len = sizeof client_addr;
        numbytes = recvfrom(sockfd, recv_buf, MAXBUFLEN - 1, 0, (struct sockaddr *) &client_addr, &client_addr_len);
        if (numbytes == -1) {
            perror("recvfrom");
            close(sockfd);
            exit(1);
        }

        printf(">>> received message %d bytes long\n", numbytes);
        recv_buf[numbytes] = '\0'; // since we assume it is string
        printf("%s\n", recv_buf);

        // reply depending on if it's ftp or not
        if (strcmp("ftp", recv_buf) == 0) {
            char *send_buf = "yes";
            printf(">>> replying with yes\n");
            numbytes = sendto(sockfd, send_buf, strlen(send_buf), 0, (struct sockaddr *) &client_addr, client_addr_len);
            if (numbytes == -1) {
                perror("sendto");
                close(sockfd);
                exit(1);
            }
        } else {
            char *send_buf = "no";
            printf(">>> replying with no\n");
            numbytes = sendto(sockfd, send_buf, strlen(send_buf), 0, (struct sockaddr *) &client_addr, client_addr_len);
            if (numbytes == -1) {
                perror("sendto");
                close(sockfd);
                exit(1);
            }
        }
    }

    close(sockfd);
    return 0;
}