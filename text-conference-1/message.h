#ifndef MESSAGE_H
#define MESSAGE_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <stddef.h> 
#include <sys/types.h> 

#define MAX_NAME 64
#define MAX_DATA 2048
#define USER_INPUT_MAX_SIZE 2048
#define MAX_BUF_SIZE MAX_NAME + MAX_DATA + 256

// NO CLIENT DATA CAN INCLUDE THE COLON CHARACTER

typedef enum {
    LOGIN, LO_ACK, LO_NAK,
    EXIT, JOIN, JN_ACK, JN_NAK,
    LEAVE_SESS, NEW_SESS, NS_ACK,
    MESSAGE, QUERY, QU_ACK
} message_type;

struct message {
    unsigned int type;
    unsigned int size; // will be number of bytes in source + data, not including their null terminators
    unsigned char source[MAX_NAME];
    unsigned char data[MAX_DATA];
};

void serialize_message(const struct message *msg, char *buffer);
void deserialize_message(char *buffer, struct message *msg);
int send_message(int sock, const struct message *msg);
ssize_t recv_all(int sock, char *buffer, size_t length);
int receive_message(int sock, struct message *msg);
void print_message(const struct message *msg);

void send_loack(int sock, const char *client_id);
void send_lonak(int sock, const char *client_id, const char *data);
void send_login(int sock, const char *client_id, const char *password);
void send_exit(int sock, const char *client_id);

void send_join(int sock, const char *client_id, const char *session_id);
void send_joinack(int sock, const char *client_id, const char *session_id);
void send_joinnak(int sock, const char *client_id, const char *session_id, const char *reason); // gotta test this
void send_leavesess(int sock, const char *client_id);
void send_newsess(int sock, const char *client_id, const char *session_id);
void send_newsessack(int sock, const char *client_id);

#endif
