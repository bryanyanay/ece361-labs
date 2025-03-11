#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#define MAX_NAME 32 // max length of client id
#define MAX_DATA 256 // max length of data being sent in packet

typedef enum {
    LOGIN, LO_ACK, LO_NAK,
    EXIT, JOIN, JN_ACK, JN_NAK,
    LEAVE_SESS, NEW_SESS, NS_ACK,
    MESSAGE, QUERY, QU_ACK
} message_type;

struct message {
    unsigned int type;
    unsigned int size;
    unsigned char source[MAX_NAME];
    unsigned char data[MAX_DATA];
};

void serialize_message(struct message *msg, char *buffer) {
    sprintf(buffer, "%u:%u:%s:%s", msg->type, msg->size, msg->source, msg->data);
}

void deserialize_message(char *buffer, struct message *msg) {
    sscanf(buffer, "%u:%u:%[^:]:%[^\n]", &msg->type, &msg->size, msg->source, msg->data);
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <port>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    // bind the TCP port
    int listener_fd, new_socket;
    struct addrinfo hints, *ai_head, *ai_curr;

    int port = atoi(argv[1]);
    char port_str[6];
    snprintf(port_str, sizeof(port_str), "%d", port);

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    int rv;
    if ((rv = getaddrinfo(NULL, port_str, &hints, &ai_head)) != 0) {
        fprintf(stderr, "getaddrinfo error: %s\n", gai_strerror(rv));
        exit(1);
    }

    for (ai_curr = ai_head; ai_curr != NULL; ai_curr = ai_curr->ai_next) {
        listener_fd = socket(ai_curr->ai_family, ai_curr->ai_socktype, ai_curr->ai_protocol);
        if (listener_fd < 0) { // if socket call fails, try the next one
            continue;
        }

        // deal with address already in use error
        int yes = 1;
        setsockopt(listener_fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));

        if (bind(listener_fd, ai_curr->ai_addr, ai_curr->ai_addrlen) < 0) { // if bind fails, try next one
            close(listener_fd);
            continue;
        }
        break; // if we successfully socket() and bind()
    }
    if (ai_curr == NULL) {
        fprintf(stderr, "socket or bind failed\n");
        exit(1);
    }

    freeaddrinfo(ai_head);

    if (listen(listener_fd, 10) == -1) {
        perror("listen");
        exit(1);
    }

    while (1) {
        if ((new_socket = accept(listener_fd, NULL, NULL)) == -1) {
            perror("accept");
            exit(1);
        }
        printf("New client connected.\n");
    }

    return 0;
}
