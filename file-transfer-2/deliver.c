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
#define MAX_TIMEOUT 30000
#define MAX(a, b) ((a) > (b) ? (a) : (b))

// credits: some of this code is adapted from beej's handbook, mainly section 6.3

// I ACCIDENTALLY WORKED ON THIS FOR FILE-TRANSFER-3; SO THIS IS ACTUALLY (INCOMPLETE) FILE-TRANSFER-3 CODE

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

double timeout_ms = 1000; // initial timeout 1 sec
double estimatedRTT = 1000, devRTT = 500;
int exp_backoff = 0; // whether we are in exponential backoff mode or not

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
int recvMsg(int sockfd, void *recv_buf, double timeout_ms) {
    // will return -1 on timeout

    // set timeout 
    struct timeval timeout_struct;
    timeout_struct.tv_sec = (long) (timeout_ms / 1000);
    timeout_struct.tv_usec = ((long) (timeout_ms * 1000)) % 1000000;

    if (setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &timeout_struct, sizeof(timeout_struct)) < 0) {
        perror("setsockopt");
        exit(1);
    }

    // receive the data
    int numbytes;
    struct sockaddr_storage serv_addr;
    socklen_t serv_addr_len = sizeof serv_addr;

    numbytes = recvfrom(sockfd, recv_buf, MAXBUFLEN - 1, 0, (struct sockaddr *) &serv_addr, &serv_addr_len);
    if (numbytes == -1) {
        if (errno == EWOULDBLOCK || errno == EAGAIN) { // timeout
            return -1; 
        }
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

    // begin transmission
    unsigned int frag_no = 1;
    while (frag_no <= total_frag) {
        long initial_pos = ftell(file);

        pkt.frag_no = frag_no;
        pkt.size = fread(pkt.filedata, 1, FRAG_SIZE, file);

        struct timespec start, end;

        char send_buf[MAXBUFLEN];
        size_t send_len = serializePkt(&pkt, send_buf, MAXBUFLEN);
        clock_gettime(CLOCK_MONOTONIC, &start); // start of RTT
        sendMsg(sockfd, send_buf, send_len, ai);
        if (verbose) {
            printf("Sent packet %u/%u (%d file bytes)\n", pkt.frag_no, pkt.total_frag, pkt.size);
        }

        struct ackpkt ack_nack;
        char recv_buf[MAXBUFLEN];
        int numbytes = recvMsg(sockfd, recv_buf, timeout_ms);

        if (numbytes == -1) { // timeout
            printf(">>> TIMEOUT for fragment %u, : waited %.6f ms", frag_no, timeout_ms);
            exp_backoff = 1;
            timeout_ms = MAX(timeout_ms * 2, MAX_TIMEOUT);

            fseek(file, initial_pos, SEEK_SET);
            continue;
        }

        clock_gettime(CLOCK_MONOTONIC, &end); // end of RTT
        double rtt = get_time_diff(start, end);

        deserializeAck(recv_buf, MAXBUFLEN, &ack_nack);

        if (ack_nack.ack_nack == 1) {
            if (verbose) {
                printf("Received ack for fragment %u\n", ack_nack.frag_no);
            }
            if (exp_backoff) {
                exp_backoff = 0;
                timeout_ms = MAX(estimatedRTT + 4 * devRTT, MAX_TIMEOUT);
            } else {
                updateRTT(rtt);
                timeout_ms = MAX(estimatedRTT + 4 * devRTT, MAX_TIMEOUT);
            }
            frag_no += 1; 
        } else { // retransmit if nack (this case isn't rlly used)
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

/* Timeout calculation
EstimatedRTT = (1-0.125) * EstimatedRTT + (0.125) * SampleRTT
DevRTT = (1-0.25) * DevRTT + (0.25) * |SampleRTT - EstimatedRTT|
initial DevRTT will be half of first EstimatedRTT
timeout = EstimatedRTT + 4 * DevRTT
initial timeout = 1 sec?
karn's alg: exponential backoff on retransmission
cap the timeout at 30s
*/

void updateRTT(double sampleRTT) {
    estimatedRTT = (1 - 0.125) * estimatedRTT + (0.125) * sampleRTT;
    if (sampleRTT - estimatedRTT > 0) {
        devRTT = (1 - 0.25) * devRTT + (0.25) * (sampleRTT - estimatedRTT);
    } else {
        devRTT = (1 - 0.25) * devRTT + (0.25) * (estimatedRTT - sampleRTT);
    }
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

    while (1) { // keep retransmitting if timeout
        char *msg = "ftp";
        sendMsg(sockfd, msg, strlen(msg), curr);

        char recv_buf[MAXBUFLEN];
        int numbytes = recvMsg(sockfd, recv_buf, timeout_ms);

        if (numbytes == -1) { // timeout
            printf(">>> TRANSMISSION TIMED OUT: waited %.6f ms", timeout_ms);
            exp_backoff = 1;
            timeout_ms = MAX(timeout_ms * 2, MAX_TIMEOUT);
            continue;
        } else {
            break;
        }
    }
    
    recv_buf[numbytes] = '\0'; // reply we know should be string

    clock_gettime(CLOCK_MONOTONIC, &end); 

    double rtt = get_time_diff(start, end);

    if (exp_backoff) { // if first message required retransmissions, then the RTT we measured isn't rlly valid
        printf("Round-Trip Time (likely invalid, required retransmissions): %.6f milliseconds\n", rtt);
        // reset exp_backoff
        exp_backoff = 0;
        timeout_ms = MAX(estimatedRTT + 4 * devRTT, MAX_TIMEOUT); 
    } else {
        printf("Round-Trip Time: %.6f milliseconds\n", rtt);
        updateRTT(rtt);
        timeout_ms = MAX(estimatedRTT + 4 * devRTT, MAX_TIMEOUT);
    }

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
