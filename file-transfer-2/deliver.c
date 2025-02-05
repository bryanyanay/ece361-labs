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

void deserializeAck(const char *src_buf, size_t buf_size, struct ackpkt *ackpkt) {
    char temp_buf[buf_size + 1];
    memcpy(temp_buf, src_buf, buf_size); 
    temp_buf[buf_size] = '\0'; // should be unnecessary bc the serialized ack packet is null-terminated

    char *field = strtok(temp_buf, ":");
    ackpkt->ack_nack = atoi(field);
    field = strtok(temp_buf, ":");
    ackpkt->frag_no = atoi(field);
}

size_t serializePkt(const struct packet *pkt, char *dest_buf, size_t buf_size) {
    // note here we write the numbers as their corresponding ascii chars, not as binary numbers
    // snprintf writes a terminating null, but memcpy overwrites that with the first byte of filedata
    int header_len = snprintf(dest_buf, buf_size, "%u:%u:%u:%s:", pkt->total_frag, pkt->frag_no, pkt->size, pkt->filename);
    
    if (header_len < 0 || header_len >= buf_size) {
        fprintf(stderr, "Error: header_len error when serializing packet\n");
        return 0;
    }

    size_t total_size = header_len + pkt->size;
    if (total_size > buf_size) {
        fprintf(stderr, "Error: packet too big for serialization\n");
        return 0;
    }

    memcpy(dest_buf + header_len, pkt->filedata, pkt->size);
    return total_size;
};

void sendMsg(int sockfd, const void *msg, size_t len, struct addrinfo *ai) {
    int numbytes;
    numbytes = sendto(sockfd, msg, len, 0, ai->ai_addr, ai->ai_addrlen);

    if (numbytes == -1) {
        perror("sendto");
        exit(1);
    }
}

// NOTE: perhaps in the future pass the expected length to receive into recvMsg
int recvMsg(int sockfd, void *recv_buf) {
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

void sendFile(int sockfd, const char *filename, struct addrinfo *ai, int verbose) {
    
    FILE *file = fopen(filename, "rb");
    if (!file) { // see if NULL
        perror("fopen");
        exit(1);
    }

    fseek(file, 0, SEEK_END); // move to end of file
    long file_size = ftell(file); // position in file (we are at end, so we get length)
    rewind(file);

    unsigned int total_frag = (file_size + (FRAG_SIZE - 1)) / FRAG_SIZE; // file_size / FRAG_SIZE would truncate towards 0, add FRAG_SIZE - 1 to ceil
    struct packet pkt;
    pkt.filename = strdup(filename);
    pkt.total_frag = total_frag;

    printf("File %s is %d bytes long, %u fragments\n", filename, file_size, total_frag);

    unsigned int frag_no = 1;
    while (frag_no <= total_frag) {
        long initial_pos = ftell(file);

        pkt.frag_no = frag_no;
        pkt.size = fread(pkt.filedata, 1, FRAG_SIZE, file);

        char send_buf[MAXBUFLEN];
        size_t send_len = serializePkt(&pkt, send_buf, MAXBUFLEN);
        sendMsg(sockfd, send_buf, send_len, ai);
        if (verbose) {
            printf("Sent packet %u/%u (%d file bytes)\n", pkt.frag_no, pkt.total_frag, pkt.size);
        }

        struct ackpkt ack_nack;
        char recv_buf[MAXBUFLEN];
        recvMsg(sockfd, recv_buf);
        deserializeAck(recv_buf, MAXBUFLEN, &ack_nack);

        if (ack_nack.ack_nack == 1) {
            if (verbose) {
                printf("Received ack for fragment %u\n", ack_nack.frag_no);
            }
            frag_no += 1;
        } else { // retransmit if nack
            printf("Received nack for fragment %u\n", ack_nack.frag_no);
            fseek(file, initial_pos, SEEK_SET);
        }
    }

    printf("Finished transmitting file.\n");

    free(pkt.filename);
}

double get_time_diff(struct timespec start, struct timespec end) {
    // in milliseconds
    return (end.tv_sec - start.tv_sec) * 1e3 + (end.tv_nsec - start.tv_nsec) / 1e6;
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: deliver <server address> <server port number>\n");
        return 1;
    }

    // POPULATE ADDRINFOS
    int status;
    struct addrinfo hints, *servinfo;
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;

    // no need for us to use inet_pton since getaddrinfo accepts a string ip address (or hostname)
    if ((status = getaddrinfo(argv[1], argv[2], &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo error: %s\n", gai_strerror(status));
        exit(1);
    }

    // loop thru and get first socket we can
    int sockfd;
    struct addrinfo *curr;
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

    // QUERY USER
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
    if (!cmd || !filename || (strtok(NULL, " ") != NULL)) { // make sure cmd and filename are non-NULL and there is nothing left in the input
        fprintf(stderr, "Input must be of form ftp <file-name>, with no spaces in file name.\n");
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

    // initial handshake
    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start); 

    char *msg = "ftp";
    sendMsg(sockfd, msg, strlen(msg), curr);

    char recv_buf[MAXBUFLEN];
    int numbytes = recvMsg(sockfd, recv_buf);
    recv_buf[numbytes] = '\0'; // reply we know should be string

    clock_gettime(CLOCK_MONOTONIC, &end); 

    double rtt = get_time_diff(start, end);
    printf("Round-Trip Time: %.6f milliseconds\n", rtt);

    if (strcmp("yes", recv_buf) == 0) {
        printf("A file transfer can start.\n");
    } else {
        printf("Server cannot accept file transfer right now.\n");
        exit(1);
    }

    sendFile(sockfd, filename, curr, 0);

    freeaddrinfo(servinfo);
    close(sockfd);

    return 0;
}
