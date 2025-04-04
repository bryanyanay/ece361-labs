#include "message.h"


void serialize_message(const struct message *msg, char *buffer) {
    // terminating null char from msg->source and ->data aren't included
    // however a null char is appended to the very end of the whole string
    // note that here size is max 9999, bc it must be represented in 4 characters
    sprintf(buffer, "%04u:%04u:%s:%s", msg->type, msg->size, msg->source, msg->data);
}

void deserialize_message(char *buffer, struct message *msg) {
    sscanf(buffer, "%04u:%04u:%[^:]:%[^:]", &msg->type, &msg->size, msg->source, msg->data);
}

void print_message(const struct message *msg) {
    printf("Message:\n");
    printf("-------------------\n");
    printf("Type: %u\n", msg->type);
    printf("Size: %u bytes\n", msg->size);
    printf("Source: \"%s\"\n", msg->source);
    printf("Data: \"%s\"\n", msg->data);
    printf("-------------------\n");
}

int send_message(int sock, const struct message *msg) {
    char buffer[MAX_BUF_SIZE];  
    serialize_message(msg, buffer);

    // so we don't send the whole buffer, only up to and including terminating null char
    size_t length = strlen(buffer) + 1; // include the terminating null char  
    size_t sent = 0;

    while (sent < length) {
        ssize_t bytes = send(sock, buffer + sent, length - sent, 0);
        if (bytes <= 0) return bytes;  // Error
        sent += bytes;
    }
    return 1;  // Success
}

ssize_t recv_all(int sock, char *buffer, size_t length) {
    size_t received = 0;
    while (received < length) {
        ssize_t bytes = recv(sock, buffer + received, length - received, 0);
        if (bytes <= 0) return bytes; // Handle errors
        received += bytes;
    }
    return received;
}

int receive_message(int sock, struct message *msg) {
    char buffer[MAX_BUF_SIZE];  
    memset(buffer, '\0', sizeof(buffer));

    // the header is "type:size:" with no terminating null char
    int expected_header_bytes = 4 + 4 + 2;
    ssize_t header_bytes = recv_all(sock, buffer, expected_header_bytes); // WE COULD'VE GOTTEN MORE THAN JUST HEADER
    // if (header_bytes <= 0) {
    //     fprintf(stderr, "recv_all failed in receive_message\n");
    //     exit(1);
    // }
    if (header_bytes <= 0) return header_bytes;  

    unsigned int type, size;
    if (sscanf(buffer, "%d:%d:", &type, &size) != 2) {
        fprintf(stderr, "sscanf failed to scan header, buffer is: %s", buffer);
        exit(1);
    }
    
    // receive the remaining message data
    int expected_body_bytes = size + 2; // add 2, one for the colon, one for terminating null

    ssize_t body_bytes = recv_all(sock, buffer + header_bytes, expected_body_bytes);
    // if (body_bytes <= 0) {
    //     fprintf(stderr, "recv_all failed in receive_message\n");
    //     exit(1);
    // }
    if (body_bytes <= 0) return body_bytes;  

    deserialize_message(buffer, msg);
    return 1; // Success
}

void send_loack(int sock, const char *client_id) {
    struct message loack_msg;
    memset(&loack_msg, 0, sizeof(loack_msg));

    loack_msg.type = LO_ACK;

    strncpy((char *)loack_msg.source, client_id, MAX_NAME - 1);
    loack_msg.source[MAX_NAME - 1] = '\0';

    loack_msg.data[0] = '\0';

    loack_msg.size = strlen((char *)loack_msg.source) + strlen((char *)loack_msg.data);

    if (send_message(sock, &loack_msg) < 0) {
        fprintf(stderr, "Failed to send login ack message.\n");
        exit(1);
    }
}

void send_lonak(int sock, const char *client_id, const char *data) {
    struct message lonak_msg;
    memset(&lonak_msg, 0, sizeof(lonak_msg));

    lonak_msg.type = LO_NAK;

    strncpy((char *)lonak_msg.source, client_id, MAX_NAME - 1);
    lonak_msg.source[MAX_NAME - 1] = '\0';

    strcpy((char *)lonak_msg.data, data);

    lonak_msg.size = strlen((char *)lonak_msg.source) + strlen((char *)lonak_msg.data);

    if (send_message(sock, &lonak_msg) < 0) {
        fprintf(stderr, "Failed to send login nak message.\n");
        exit(1);
    }
}

void send_login(int sock, const char *client_id, const char *password) {
    struct message login_msg;
    memset(&login_msg, 0, sizeof(login_msg));

    login_msg.type = LOGIN;

    strncpy((char *)login_msg.source, client_id, MAX_NAME - 1);
    login_msg.source[MAX_NAME - 1] = '\0';

    strncpy((char *)login_msg.data, password, MAX_DATA - 1);
    login_msg.data[MAX_DATA - 1] = '\0';

    login_msg.size = strlen((char *)login_msg.source) + strlen((char *)login_msg.data);

    if (send_message(sock, &login_msg) < 0) {
        fprintf(stderr, "Failed to send login message.\n");
        exit(1);
    }
}

void send_exit(int sock, const char *client_id) {
    struct message exit_msg;
    memset(&exit_msg, 0, sizeof(exit_msg));

    exit_msg.type = EXIT;

    strncpy((char *)exit_msg.source, client_id, MAX_NAME - 1);
    exit_msg.source[MAX_NAME - 1] = '\0';

    exit_msg.data[0] = '\0';

    exit_msg.size = strlen((char *)exit_msg.source) + strlen((char *)exit_msg.data);

    if (send_message(sock, &exit_msg) < 0) {
        fprintf(stderr, "Failed to send exit message.\n");
        exit(1);
    }
}

void send_join(int sock, const char *client_id, const char *session_id) {
    struct message msg;
    memset(&msg, 0, sizeof(msg));

    msg.type = JOIN;

    strncpy((char *)msg.source, client_id, MAX_NAME - 1);
    msg.source[MAX_NAME - 1] = '\0';

    strncpy((char *)msg.data, session_id, MAX_DATA - 1);
    msg.data[MAX_DATA - 1] = '\0';

    msg.size = strlen((char *)msg.source) + strlen((char *)msg.data);

    if (send_message(sock, &msg) < 0) {
        fprintf(stderr, "Failed to send join message.\n");
        exit(1);
    }
}

void send_joinack(int sock, const char *client_id, const char *session_id) {
    struct message msg;
    memset(&msg, 0, sizeof(msg));

    msg.type = JN_ACK;

    strncpy((char *)msg.source, client_id, MAX_NAME - 1);
    msg.source[MAX_NAME - 1] = '\0';

    strncpy((char *)msg.data, session_id, MAX_DATA - 1);
    msg.data[MAX_DATA - 1] = '\0';

    msg.size = strlen((char *)msg.source) + strlen((char *)msg.data);

    if (send_message(sock, &msg) < 0) {
        fprintf(stderr, "Failed to send join ack message.\n");
        exit(1);
    }
}

void send_joinnak(int sock, const char *client_id, const char *session_id, const char *reason) {
    struct message msg;
    memset(&msg, 0, sizeof(msg));

    msg.type = JN_NAK;

    strncpy((char *)msg.source, client_id, MAX_NAME - 1);
    msg.source[MAX_NAME - 1] = '\0';

    snprintf((char *)msg.data, MAX_DATA, "%s, %s", session_id, reason);

    msg.size = strlen((char *)msg.source) + strlen((char *)msg.data);

    if (send_message(sock, &msg) < 0) {
        fprintf(stderr, "Failed to send join nak message.\n");
        exit(1);
    }
}

void send_leavesess(int sock, const char *client_id) {
    struct message msg;
    memset(&msg, 0, sizeof(msg));

    msg.type = LEAVE_SESS;

    strncpy((char *)msg.source, client_id, MAX_NAME - 1);
    msg.source[MAX_NAME - 1] = '\0';

    msg.data[0] = '\0';

    msg.size = strlen((char *)msg.source) + strlen((char *)msg.data);

    if (send_message(sock, &msg) < 0) {
        fprintf(stderr, "Failed to send leave session message.\n");
        exit(1);
    }
}

void send_newsess(int sock, const char *client_id, const char *session_id) {
    struct message msg;
    memset(&msg, 0, sizeof(msg));

    msg.type = NEW_SESS;

    strncpy((char *)msg.source, client_id, MAX_NAME - 1);
    msg.source[MAX_NAME - 1] = '\0';

    strncpy((char *)msg.data, session_id, MAX_DATA - 1);
    msg.data[MAX_DATA - 1] = '\0';

    msg.size = strlen((char *)msg.source) + strlen((char *)msg.data);

    if (send_message(sock, &msg) < 0) {
        fprintf(stderr, "Failed to send new session message.\n");
        exit(1);
    }
}

void send_newsessack(int sock, const char *client_id) {
    struct message msg;
    memset(&msg, 0, sizeof(msg));

    msg.type = NS_ACK;

    strncpy((char *)msg.source, client_id, MAX_NAME - 1);
    msg.source[MAX_NAME - 1] = '\0';

    msg.data[0] = '\0';

    msg.size = strlen((char *)msg.source) + strlen((char *)msg.data);

    if (send_message(sock, &msg) < 0) {
        fprintf(stderr, "Failed to send new session ack message.\n");
        exit(1);
    }
}

void send_query(int sock, const char *client_id) {
    struct message msg;
    memset(&msg, 0, sizeof(msg));

    msg.type = QUERY;

    strncpy((char *)msg.source, client_id, MAX_NAME - 1);
    msg.source[MAX_NAME - 1] = '\0';

    msg.data[0] = '\0';

    msg.size = strlen((char *)msg.source) + strlen((char *)msg.data);

    if (send_message(sock, &msg) < 0) {
        fprintf(stderr, "Failed to send new query message.\n");
        exit(1);
    }
}

void send_quack(int sock, const char *client_id, const char *user_list) {
    struct message msg;
    memset(&msg, 0, sizeof(msg));

    msg.type = QU_ACK;

    strncpy((char *)msg.source, client_id, MAX_NAME - 1);
    msg.source[MAX_NAME - 1] = '\0';

    strncpy((char *)msg.data, user_list, MAX_DATA - 1);
    msg.data[MAX_DATA - 1] = '\0';

    msg.size = strlen((char *)msg.source) + strlen((char *)msg.data);

    if (send_message(sock, &msg) < 0) {
        fprintf(stderr, "Failed to send new qu_ack message.\n");
        exit(1);
    }
}

void send_usermsg(int sock, const char *client_id, const char *msgdata) {
    struct message msg;
    memset(&msg, 0, sizeof(msg));

    msg.type = MESSAGE;

    strncpy((char *)msg.source, client_id, MAX_NAME - 1);
    msg.source[MAX_NAME - 1] = '\0';

    strncpy((char *)msg.data, msgdata, MAX_DATA - 1);
    msg.data[MAX_DATA - 1] = '\0';

    msg.size = strlen((char *)msg.source) + strlen((char *)msg.data);

    if (send_message(sock, &msg) < 0) {
        fprintf(stderr, "Failed to send user message.\n");
        exit(1);
    }
}

void send_getmsg(int sock, const char *client_id) {
    struct message msg;
    memset(&msg, 0, sizeof(msg));

    msg.type = GET_MSG;

    strncpy((char *)msg.source, client_id, MAX_NAME - 1);
    msg.source[MAX_NAME - 1] = '\0';

    msg.data[0] = '\0';

    msg.size = strlen((char *)msg.source) + strlen((char *)msg.data);

    if (send_message(sock, &msg) < 0) {
        fprintf(stderr, "Failed to send get message.\n");
        exit(1);
    }
}

void send_privmsg(int sock, const char *client_id, const char *dest_user, const char *message) {
    // we do smth special with privmsg
    // the client_id field will actually contain the username of the destination user
    // the data field will contain "[FROM sending-username] message"

    struct message msg;
    memset(&msg, 0, sizeof(msg));

    msg.type = PRIV_MSG;

    strncpy((char *)msg.source, dest_user, MAX_NAME - 1); 
    msg.source[MAX_NAME - 1] = '\0';

    snprintf((char *) msg.data, MAX_DATA, "[PRIVATE FROM %s] %s", client_id, message);

    msg.size = strlen((char *)msg.source) + strlen((char *)msg.data);

    if (send_message(sock, &msg) < 0) {
        fprintf(stderr, "Failed to send get message.\n");
        exit(1);
    }
}


void send_suack(int sock, const char *client_id) {
    struct message msg;
    memset(&msg, 0, sizeof(msg));

    msg.type = SU_ACK;

    strncpy((char *)msg.source, client_id, MAX_NAME - 1);
    msg.source[MAX_NAME - 1] = '\0';

    msg.data[0] = '\0';

    msg.size = strlen((char *)msg.source) + strlen((char *)msg.data);

    if (send_message(sock, &msg) < 0) {
        fprintf(stderr, "Failed to send sign up ack message.\n");
        exit(1);
    }
}

void send_sunak(int sock, const char *client_id, const char *data) {
    struct message msg;
    memset(&msg, 0, sizeof(msg));

    msg.type = SU_NAK;

    strncpy((char *)msg.source, client_id, MAX_NAME - 1);
    msg.source[MAX_NAME - 1] = '\0';

    strcpy((char *)msg.data, data);

    msg.size = strlen((char *)msg.source) + strlen((char *)msg.data);

    if (send_message(sock, &msg) < 0) {
        fprintf(stderr, "Failed to send sign up nak message.\n");
        exit(1);
    }
}

void send_signup(int sock, const char *client_id, const char *password) {
    struct message msg;
    memset(&msg, 0, sizeof(msg));

    msg.type = SIGN_UP;

    strncpy((char *)msg.source, client_id, MAX_NAME - 1);
    msg.source[MAX_NAME - 1] = '\0';

    strncpy((char *)msg.data, password, MAX_DATA - 1);
    msg.data[MAX_DATA - 1] = '\0';

    msg.size = strlen((char *)msg.source) + strlen((char *)msg.data);

    if (send_message(sock, &msg) < 0) {
        fprintf(stderr, "Failed to send sign up message.\n");
        exit(1);
    }
}