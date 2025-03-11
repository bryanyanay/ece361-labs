#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#define MAX_NAME 32
#define MAX_DATA 256
#define USER_INPUT_MAX_SIZE 512

// NO CLIENT DATA CAN INCLUDE THE COLON CHARACTER

typedef enum {
    LOGIN, LO_ACK, LO_NAK,
    EXIT, JOIN, JN_ACK, JN_NAK,
    LEAVE_SESS, NEW_SESS, NS_ACK,
    MESSAGE, QUERY, QU_ACK
} message_type;

struct message {
    unsigned int type;
    unsigned int size; // will be number of bytes in source + data
    unsigned char source[MAX_NAME];
    unsigned char data[MAX_DATA];
};

void serialize_message(struct message *msg, char *buffer) {
    // terminating null char from msg->source and ->data aren't included
    // however a null char is appended to the very end of the whole string
    sprintf(buffer, "%u:%u:%s:%s", msg->type, msg->size, msg->source, msg->data);
}

void deserialize_message(char *buffer, struct message *msg) {
    sscanf(buffer, "%u:%u:%[^:]:%[^\0]", &msg->type, &msg->size, msg->source, msg->data);
}

void send_message(int client_socket, int type, const char *client_id, const char *data) {
    struct message msg;
    msg.type = type;
    msg.size = strlen(data) + 1 + strlen(); // this doesn't include terminating null char
    strcpy((char *)msg.source, client_id);
    strcpy((char *)msg.data, data);
        
    char buffer[MAX_NAME + MAX_DATA + 100];
    serialize_message(&msg, buffer);
    send(client_socket, buffer, strlen(buffer), 0);
}

int main() {

    char client_id[MAX_NAME];
    int client_socket;

    char user_input[USER_INPUT_MAX_SIZE];

    printf(">>> ");

    while (fgets(user_input, USER_INPUT_MAX_SIZE, stdin)) {
        printf("User inputted: ", user_input);
        if (strncmp(user_input, "/login", 6) == 0) {
            char server_ip[INET_ADDRSTRLEN];
            int server_port;
            char password[MAX_DATA];
            
            sscanf(user_input, "/login %s %s %s %d", client_id, password, server_ip, &server_port);
            
            client_socket = socket(AF_INET, SOCK_STREAM, 0);
            if (client_socket < 0) {
                perror("socket");
                exit(1);
            }
            
            struct sockaddr_in server_addr;
            server_addr.sin_family = AF_INET;
            server_addr.sin_port = htons(server_port);
            inet_pton(AF_INET, server_ip, &server_addr.sin_addr);
            
            if (connect(client_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
                perror("connect");
                exit(1);
            }
            
            printf("Connected to server.\n");
            // pthread_t recv_thread;
            // pthread_create(&recv_thread, NULL, receive_handler, NULL);
            
            send_message(LOGIN, password);
            // receive the login ack, and report the result ot user
        } else if (strncmp(user_input, "/logout", 7) == 0) {
            // send_message(EXIT, "");
            // close(client_socket);
            // printf("Logged out.\n");
        } else if (strncmp(user_input, "/joinsession", 12) == 0) {
            // char session_id[MAX_NAME];
            // sscanf(command, "/joinsession %s", session_id);
            // send_message(JOIN, session_id);
        } else if (strncmp(user_input, "/leavesession", 13) == 0) {
            // send_message(LEAVE_SESS, "");
        } else if (strncmp(user_input, "/createsession", 14) == 0) {
            char session_id[MAX_NAME];
            // sscanf(command, "/createsession %s", session_id);
            // send_message(NEW_SESS, session_id);
        } else if (strncmp(user_input, "/list", 5) == 0) {
            // send_message(QUERY, "");
        } else if (strncmp(user_input, "/quit", 5) == 0) {
            // send_message(EXIT, "");
            // close(client_socket);
            // printf("Client exiting.\n");
            // exit(0);
        } else {
            // send_message(MESSAGE, command);
        }
        printf(">>> ");
    }

    return 0;
}