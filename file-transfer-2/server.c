#include <stdio.h>
#include <stdlib.h>     
#include <string.h>     
#include <netdb.h>     
#include <arpa/inet.h> 
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>


#define MAXBUFLEN 1500
#define FRAG_SIZE 1000

// credits: some of this code is adapted from beej's handbook, mainly section 6.3

struct packet {
    unsigned int total_frag;
    unsigned int frag_no;
    unsigned int size;
    char *filename;
    char filedata[1000];
};

struct ackpkt {
    unsigned int ack_nack; // 1 for ack, 0 for nack
    unsigned int frag_no;
};

size_t serializeAck(const struct ackpkt *ackpkt, char *dest_buf, size_t buf_size) {
    // returns length of serialized data, including terminating null char
    // serialized data will be null-terminated
    return 1 + snprintf(dest_buf, buf_size, "%u:%u", ackpkt->ack_nack, ackpkt->frag_no);
}

void deserializePkt(const char *src_buf, size_t buf_size, struct packet *pkt) {
    char temp_buf[buf_size + 1];
    memcpy(temp_buf, src_buf, buf_size);
    temp_buf[buf_size] = '\0'; // bc strtok needs null terminated

    char *field = strtok(temp_buf, ":");
    pkt->total_frag = atoi(field);
    field = strtok(NULL, ":");
    pkt->frag_no = atoi(field);
    field = strtok(NULL, ":");
    pkt->size = atoi(field);
    field = strtok(NULL, ":");
    pkt->filename = strdup(field); // rmb to free this later

    size_t end_of_header = strlen(field) + 1; // bc strtok replaces colons with null char
    memcpy(pkt->filedata, field + end_of_header, pkt->size);
}

int recvMsg(int sockfd, char *recv_buf, struct sockaddr_storage *client_addr_ptr, socklen_t *client_addr_len_ptr) {
    // here we also return the client addr info thru the pointers
    int numbytes;
    numbytes = recvfrom(sockfd, recv_buf, MAXBUFLEN - 1, 0, (struct sockaddr *) client_addr_ptr, client_addr_len_ptr);

    if (numbytes == -1) {
        perror("recvfrom");
        exit(1);
    }

    return numbytes;
}

void sendMsg(int sockfd, const char *msg, size_t len, struct sockaddr *client_addr_ptr, socklen_t client_addr_len) {
    // msg may not be a string
    int numbytes;
    numbytes = sendto(sockfd, msg, len, 0, client_addr_ptr, client_addr_len);

    if (numbytes == -1) {
        perror("sendto");
        exit(1);
    }
}

void recvFile(int sockfd, int verbose) {

    struct sockaddr_storage client_addr;
    socklen_t client_addr_len = sizeof client_addr;
    struct packet pkt = {0};
    char recv_buf[MAXBUFLEN];

    FILE *file = NULL;
    char *recv_filename = NULL;
    unsigned int expected_frag_no;


    while (1) {

        recvMsg(sockfd, recv_buf, &client_addr, &client_addr_len);
        deserializePkt(recv_buf, MAXBUFLEN, &pkt);

        if (!recv_filename) { // first packet, get the filename and open the file
            recv_filename = strdup(pkt.filename);
            expected_frag_no = 1;
            file = fopen(recv_filename, "wb"); // will overwrite if exists, and create if not
            if (!file) {
                perror("fopen");
                exit(1);
            }
            printf(">>> Receiving file: %s\n", recv_filename);
        }

        fwrite(pkt.filedata, 1, pkt.size, file);
        if (verbose) {
            printf("Received fragment %u/%u (%d file bytes)\n", pkt.frag_no, pkt.total_frag, pkt.size);
        }

        // send ack/nack
        if (pkt.frag_no == expected_frag_no) {
            struct ackpkt ack = {1, expected_frag_no};
            char msg[MAXBUFLEN];
            size_t msg_len = serializeAck(&ack, msg, MAXBUFLEN);
            sendMsg(sockfd, msg, msg_len, (struct sockaddr *) &client_addr, client_addr_len);
        } else {
            struct ackpkt nack = {0, expected_frag_no};
            char msg[MAXBUFLEN];
            size_t msg_len = serializeAck(&nack, msg, MAXBUFLEN);
            sendMsg(sockfd, msg, msg_len, (struct sockaddr *) &client_addr, client_addr_len);
        }

        expected_frag_no += 1;

        if (pkt.frag_no == pkt.total_frag) {
            printf(">>> Finished receiving file\n");
            break;
        }
    }

    fclose(file);
    free(pkt.filename);
    free(recv_filename);
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: server <server port number>\n");
        exit(1);
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
        // initial handshake
        int numbytes;
        char recv_buf[MAXBUFLEN];
        struct sockaddr_storage client_addr;
        socklen_t client_addr_len = sizeof client_addr;
        numbytes = recvMsg(sockfd, recv_buf, &client_addr, &client_addr_len);
        recv_buf[numbytes] = '\0'; // initial message we know should be string

        printf(">>> received message %d bytes long\n", numbytes);
        printf("%s\n", recv_buf);

        // reply depending on if it's ftp or not
        if (strcmp("ftp", recv_buf) == 0) {
            char *send_buf = "yes";
            printf(">>> replying with yes\n");
            sendMsg(sockfd, send_buf, strlen(send_buf), (struct sockaddr *) &client_addr, client_addr_len);
            recvFile(sockfd, 0);
        } else {
            char *send_buf = "no";
            printf(">>> replying with no\n");
            sendMsg(sockfd, send_buf, strlen(send_buf), (struct sockaddr *) &client_addr, client_addr_len);
        }
    }

    close(sockfd);
    return 0;
}